// Closed-loop simulation: the real firmware control core (src/control) against sim::Plant.
// Build: see sim/README.md. Writes out/<scenario>.csv, out/<scenario>_events.csv, out/<scenario>_fast.csv
// and out/metrics.json.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "config.h"
#include "control/long_pwm.h"
#include "control/median_filter.h"
#include "control/state_machine.h"
#include "plant.h"

using namespace control;

// ------------------------------------------------------------ firmware HAL emulation (hal::Blower without Arduino)
class SimBlower {
public:
    SimBlower(uint8_t min_start, uint8_t max_pct, double kick_s, uint32_t long_period_s)
        : lp_(min_start, long_period_s, cfg::BLOWER_MIN_RUN_DUTY_PCT, cfg::BLOWER_LONG_PWM_MIN_ON_S),
          max_pct_(max_pct), kick_s_(kick_s) {}

    void setDuty(uint8_t pct, float dt_s, double now) {
        if (pct > max_pct_) pct = max_pct_;
        target_ = lp_.step(pct, dt_s);
        const bool was_off = applied_ == 0 && !kicking_;
        if (target_ == 0) { kicking_ = false; applied_ = 0; return; }
        if (was_off && kick_s_ > 0) { kicking_ = true; kick_end_ = now + kick_s_; applied_ = max_pct_; return; }
        if (!kicking_) applied_ = target_;
    }
    void service(double now) {
        if (kicking_ && now >= kick_end_) { kicking_ = false; applied_ = target_; }
    }
    void off() { target_ = 0; applied_ = 0; kicking_ = false; lp_.reset(); }
    uint8_t applied() const { return applied_; }

private:
    LongPwm lp_;
    uint8_t max_pct_;
    double kick_s_;
    uint8_t target_ = 0, applied_ = 0;
    bool kicking_ = false;
    double kick_end_ = 0;
};

// ------------------------------------------------------------ thresholds exactly as main.cpp builds them
static Thresholds firmwareThresholds(uint8_t max_duty) {
    Thresholds t{};
    t.ignite_exit_margin_c = cfg::IGNITE_EXIT_MARGIN_C;
    t.ignite_duty_pct = cfg::IGNITE_DUTY_PCT;
    t.ignite_timeout_s = cfg::IGNITE_TIMEOUT_S;
    t.ignite_min_rise_c = cfg::IGNITE_MIN_RISE_C;
    t.startup_max_duty_pct = std::min<uint8_t>(cfg::STARTUP_MAX_DUTY_PCT, max_duty);
    t.max_duty_pct = max_duty;
    t.hold_band_c = cfg::HOLD_BAND_C;
    t.hold_settle_s = cfg::HOLD_SETTLE_S;
    t.recovery_max_s = cfg::RECOVERY_MAX_S;
    t.lid_open_drop_c_per_s = cfg::LID_OPEN_DROP_C_PER_S;
    t.lid_open_total_drop_c = cfg::LID_OPEN_TOTAL_DROP_C;
    t.lid_rate_window_s = cfg::LID_RATE_WINDOW_S;
    t.lid_open_min_s = cfg::LID_OPEN_MIN_S;
    t.lid_open_timeout_s = cfg::LID_OPEN_TIMEOUT_S;
    t.lid_open_duty_pct = cfg::LID_OPEN_DUTY_PCT;
    t.lid_open_recover_band_c = cfg::LID_OPEN_RECOVER_BAND_C;
    t.lid_exit_rise_above_low_c = cfg::LID_EXIT_RISE_ABOVE_LOW_C;
    t.lid_exit_plateau_c_per_s = cfg::LID_EXIT_PLATEAU_C_PER_S;
    t.lid_guard_out_ema_pct = cfg::LID_GUARD_OUT_EMA_PCT;
    t.lid_rising_rate_c_per_s = cfg::LID_RISING_RATE_C_PER_S;
    t.lid_rising_window_s = cfg::LID_RISING_WINDOW_S;
    t.lid_rising_duty_pct = cfg::LID_RISING_DUTY_PCT;
    t.out_ema_alpha = cfg::OUT_EMA_ALPHA;
    t.probe_ema_alpha = cfg::PROBE_EMA_ALPHA;
    t.fuel_low_fall_c = cfg::FUEL_LOW_FALL_C;
    t.fuel_low_window_s = cfg::FUEL_LOW_WINDOW_S;
    t.fuel_low_out_pct = cfg::FUEL_LOW_OUT_PCT;
    t.fuel_low_clear_s = cfg::FUEL_LOW_CLEAR_S;
    t.fire_out_below_sp_c = cfg::FIRE_OUT_BELOW_SP_C;
    t.fire_out_s = cfg::FIRE_OUT_S;
    t.pit_alarm_dev_c = cfg::PIT_ALARM_DEV_C;
    t.pit_alarm_hyst_c = cfg::PIT_ALARM_HYST_C;
    t.creep_above_sp_c = cfg::CREEP_ABOVE_SP_C;
    t.creep_s = cfg::CREEP_S;
    t.pit_over_margin_c = cfg::PIT_OVER_MARGIN_C;
    t.pit_soft_limit_c = cfg::PIT_SOFT_LIMIT_C;
    return t;
}

// ------------------------------------------------------------ scenario
struct Window { double start_s, end_s; };

struct Scenario {
    std::string name;
    std::string description;
    sim::PlantParams plant;
    Thresholds th;
    PidGains gains{cfg::PID_KP_DEFAULT, cfg::PID_KI_DEFAULT, cfg::PID_KD_DEFAULT};
    uint8_t min_start_pct = cfg::BLOWER_MIN_START_DUTY_PCT;
    float setpoint_c = cfg::SETPOINT_DEFAULT_C;
    double duration_s = 12 * 3600.0;
    std::vector<Window> lids{{3 * 3600.0, 3 * 3600.0 + 90.0}, {6 * 3600.0, 6 * 3600.0 + 20.0}};
    std::vector<Window> wind{{5 * 3600.0, 5 * 3600.0 + 1200.0}};
    double wind_factor = 2.0;
    Window fast_window{7200.0, 7800.0};  // full-rate trace for the blower zoom: 10 min of HOLD at 2 h
    bool write_files = true;
};

struct Metrics {
    double first_reach_s = -1;
    double startup_peak_c = 0;
    double startup_overshoot_c = 0;
    double hold_rms_c = 0;
    double hold_max_abs_c = 0;
    double hold_time_s = 0;
    double mean_duty_hold = 0;
    int lid_detections = 0;
    int lid_true_positive = 0;
    int lid_false_positive = 0;
    double lid1_detect_delay_s = -1;
    double lid1_recovery_peak_c = 0;
    double fuel_low_first_s = -1;
    double fire_out_s = -1;
    double creep_first_s = -1;
    double pit_high_s = 0;
    double pit_low_s = 0;
    double fuel_used_kg = 0;
    double burn_avg_kg_h_hold = 0;
    double long_pwm_fraction = 0;   // share of HOLD seconds where the command was below min-start
    double wind_max_dev_c = 0;
    double last_fuel_low_rise_s = -1;
    double fuel_low_to_fire_out_min = -1;
    std::string final_state;
    std::string final_fault;
};

static bool inAny(const std::vector<Window>& ws, double t, double pad = 0) {
    for (const auto& w : ws) if (t >= w.start_s && t < w.end_s + pad) return true;
    return false;
}

static Metrics run(const Scenario& sc) {
    sim::Plant plant(sc.plant, 42);
    StateMachine sm(sc.th, sc.gains);
    SimBlower blower(sc.min_start_pct, sc.th.max_duty_pct, cfg::BLOWER_KICK_MS / 1000.0, cfg::BLOWER_LONG_PWM_PERIOD_S);
    MedianFilter<cfg::MEDIAN_WINDOW> med;

    FILE* f = nullptr; FILE* fe = nullptr; FILE* ff = nullptr;
    if (sc.write_files) {
        f = std::fopen(("out/" + sc.name + ".csv").c_str(), "w");
        fe = std::fopen(("out/" + sc.name + "_events.csv").c_str(), "w");
        ff = std::fopen(("out/" + sc.name + "_fast.csv").c_str(), "w");
        std::fprintf(f, "t_s,chamber_c,probe_c,setpoint_c,duty_cmd,duty_applied_avg,fan_speed,air_cfm,state,fault,warnings,lit_kg,fuel_left_kg,meat_c,out_ema,burn_kg_h,lid,wind\n");
        std::fprintf(fe, "t_s,kind,value,pit_c\n");
        std::fprintf(ff, "t_s,duty_cmd,duty_applied,fan_speed,air_cfm\n");
    }

    Metrics m;
    sm.setSetpoint(sc.setpoint_c);
    sm.startCook(sc.setpoint_c);

    const double dt = 0.1;
    const int per_tick = 10;  // controller at 1 Hz
    Command cmd{0, sm.state(), sm.fault(), 0};
    State last_state = sm.state();
    Fault last_fault = sm.fault();
    uint8_t last_warn = 0;
    double applied_acc = 0; int applied_n = 0;
    double sq = 0; double duty_hold = 0; double burn_hold = 0; double below_min_hold = 0;
    bool reached = false; double hold_entered = -1;
    bool lid1_seen = false;
    double lid1_start = sc.lids.empty() ? -1 : sc.lids[0].start_s;
    bool in_lid1_recovery = false;

    const long steps = static_cast<long>(sc.duration_s / dt);
    for (long i = 0; i < steps; ++i) {
        const double t = i * dt;
        sim::Disturbance d;
        d.lid_open = inAny(sc.lids, t);
        d.wind_factor = inAny(sc.wind, t) ? sc.wind_factor : 1.0;

        if (i % per_tick == 0) {
            const float probe = static_cast<float>(plant.probeReading());
            med.push(probe);
            Sample s{med.value(), true, Fault::NONE, 1.0f};
            cmd = sm.tick(s);
            if (cmd.state == State::FAULT || cmd.state == State::IDLE) blower.off();
            else blower.setDuty(cmd.duty_pct, 1.0f, t);

            const float pit = med.value();
            // events
            if (cmd.state != last_state) {
                if (fe) std::fprintf(fe, "%.0f,state,%s,%.2f\n", t, toString(cmd.state), pit);
                if (cmd.state == State::LID_OPEN) {
                    ++m.lid_detections;
                    const bool tp = inAny(sc.lids, t, 120.0);
                    if (tp) ++m.lid_true_positive; else ++m.lid_false_positive;
                    if (!lid1_seen && lid1_start >= 0 && t >= lid1_start && t < lid1_start + 300) {
                        lid1_seen = true;
                        m.lid1_detect_delay_s = t - lid1_start;
                        in_lid1_recovery = true;
                    }
                }
                if (last_state == State::RECOVERY && in_lid1_recovery) in_lid1_recovery = false;
                last_state = cmd.state;
            }
            if (cmd.fault != last_fault) {
                if (fe) std::fprintf(fe, "%.0f,fault,%s,%.2f\n", t, toString(cmd.fault), pit);
                if (cmd.fault == Fault::FIRE_OUT && m.fire_out_s < 0) m.fire_out_s = t;
                last_fault = cmd.fault;
            }
            if (cmd.warnings != last_warn) {
                if (fe) std::fprintf(fe, "%.0f,warnings,0x%02x,%.2f\n", t, cmd.warnings, pit);
                if ((cmd.warnings & Warning::FUEL_LOW) && m.fuel_low_first_s < 0) m.fuel_low_first_s = t;
                if ((cmd.warnings & Warning::FUEL_LOW) && !(last_warn & Warning::FUEL_LOW)) m.last_fuel_low_rise_s = t;
                if ((cmd.warnings & Warning::CREEP) && m.creep_first_s < 0) m.creep_first_s = t;
                last_warn = cmd.warnings;
            }
            if (cmd.warnings & Warning::PIT_HIGH) m.pit_high_s += 1;
            if (cmd.warnings & Warning::PIT_LOW) m.pit_low_s += 1;

            // metrics
            if (!reached && pit >= sc.setpoint_c) { reached = true; m.first_reach_s = t; }
            if (hold_entered < 0 && cmd.state == State::HOLD) hold_entered = t;
            if (hold_entered < 0 || t < hold_entered + 600) m.startup_peak_c = std::max<double>(m.startup_peak_c, plant.chamberC());
            if (in_lid1_recovery) m.lid1_recovery_peak_c = std::max<double>(m.lid1_recovery_peak_c, plant.chamberC());
            const bool quiet = !inAny(sc.lids, t, 900.0) && !inAny(sc.wind, t, 1800.0) && plant.fuelLeftKg() > 0.6;
            if (cmd.state == State::HOLD && inAny(sc.wind, t, 1800.0)) {
                m.wind_max_dev_c = std::max(m.wind_max_dev_c, std::fabs(plant.chamberC() - sc.setpoint_c));
            }
            if (cmd.state == State::HOLD && quiet) {
                const double e = plant.chamberC() - sc.setpoint_c;
                sq += e * e;
                m.hold_max_abs_c = std::max(m.hold_max_abs_c, std::fabs(e));
                m.hold_time_s += 1;
                duty_hold += cmd.duty_pct;
                burn_hold += plant.burnKgPerH();
                if (cmd.duty_pct > 0 && cmd.duty_pct < sc.min_start_pct) below_min_hold += 1;
            }
        }

        blower.service(t);
        plant.step(dt, blower.applied(), d);
        applied_acc += blower.applied(); ++applied_n;

        if (ff && t >= sc.fast_window.start_s && t < sc.fast_window.end_s) {
            std::fprintf(ff, "%.1f,%u,%u,%.3f,%.2f\n", t, cmd.duty_pct, blower.applied(), plant.fanSpeed(), plant.airCfm());
        }
        if (f && i % 50 == 0) {
            std::fprintf(f, "%.0f,%.2f,%.2f,%.1f,%u,%.1f,%.3f,%.2f,%s,%s,%u,%.3f,%.3f,%.2f,%.1f,%.3f,%d,%.1f\n", t,
                         plant.chamberC(), med.value(), sc.setpoint_c, cmd.duty_pct, applied_acc / std::max(1, applied_n),
                         plant.fanSpeed(), plant.airCfm(), toString(cmd.state), toString(cmd.fault), cmd.warnings,
                         plant.litKg(), plant.fuelLeftKg(), plant.meatC(), sm.outputEma(), plant.burnKgPerH(),
                         d.lid_open ? 1 : 0, d.wind_factor);
            applied_acc = 0; applied_n = 0;
        }
        if (cmd.state == State::FAULT && plant.chamberC() < sc.plant.t_amb_c + 15) break;  // cold, nothing more to see
    }

    m.startup_overshoot_c = m.startup_peak_c - sc.setpoint_c;
    if (m.fire_out_s > 0 && m.last_fuel_low_rise_s > 0) m.fuel_low_to_fire_out_min = (m.fire_out_s - m.last_fuel_low_rise_s) / 60.0;
    if (m.hold_time_s > 0) {
        m.hold_rms_c = std::sqrt(sq / m.hold_time_s);
        m.mean_duty_hold = duty_hold / m.hold_time_s;
        m.burn_avg_kg_h_hold = burn_hold / m.hold_time_s;
        m.long_pwm_fraction = below_min_hold / m.hold_time_s;
    }
    if (m.lid1_recovery_peak_c > 0) m.lid1_recovery_peak_c -= sc.setpoint_c;
    m.fuel_used_kg = plant.fuelUsedKg();
    m.final_state = toString(sm.state());
    m.final_fault = toString(sm.fault());
    if (f) std::fclose(f);
    if (fe) std::fclose(fe);
    if (ff) std::fclose(ff);
    return m;
}

static void writeMetrics(FILE* j, const Scenario& sc, const Metrics& m, bool last) {
    std::fprintf(j,
        "  {\"name\":\"%s\",\"description\":\"%s\",\"kp\":%.3f,\"ki\":%.4f,\"kd\":%.2f,\"fan_cfm\":%.1f,\"flap\":%s,"
        "\"max_duty\":%u,\"startup_cap\":%u,\"ignite_duty\":%u,"
        "\"first_reach_min\":%.1f,\"startup_overshoot_c\":%.1f,\"hold_rms_c\":%.2f,\"hold_max_abs_c\":%.1f,\"hold_hours\":%.2f,"
        "\"mean_duty_hold\":%.1f,\"long_pwm_fraction\":%.2f,\"lid_detections\":%d,\"lid_true\":%d,\"lid_false\":%d,"
        "\"lid1_detect_s\":%.0f,\"lid1_recovery_overshoot_c\":%.1f,\"fuel_low_h\":%.2f,\"fire_out_h\":%.2f,\"creep_h\":%.2f,"
        "\"wind_max_dev_c\":%.1f,\"fuel_low_lead_min\":%.0f,\"pit_high_min\":%.1f,\"pit_low_min\":%.1f,\"fuel_used_kg\":%.2f,\"burn_kg_h_hold\":%.3f,\"final_state\":\"%s\",\"final_fault\":\"%s\"}%s\n",
        sc.name.c_str(), sc.description.c_str(), sc.gains.kp, sc.gains.ki, sc.gains.kd, sc.plant.fan_cfm_max,
        sc.plant.flap_installed ? "true" : "false", sc.th.max_duty_pct, sc.th.startup_max_duty_pct, sc.th.ignite_duty_pct,
        m.first_reach_s / 60.0, m.startup_overshoot_c, m.hold_rms_c, m.hold_max_abs_c, m.hold_time_s / 3600.0,
        m.mean_duty_hold, m.long_pwm_fraction, m.lid_detections, m.lid_true_positive, m.lid_false_positive,
        m.lid1_detect_delay_s, m.lid1_recovery_peak_c, m.fuel_low_first_s / 3600.0, m.fire_out_s / 3600.0,
        m.creep_first_s / 3600.0, m.wind_max_dev_c, m.fuel_low_to_fire_out_min, m.pit_high_s / 60.0, m.pit_low_s / 60.0, m.fuel_used_kg, m.burn_avg_kg_h_hold,
        m.final_state.c_str(), m.final_fault.c_str(), last ? "" : ",");
}

int main(int argc, char** argv) {
    const bool sweep_only = argc > 1 && std::strcmp(argv[1], "--sweep") == 0;
    std::vector<Scenario> scs;

    Scenario base;
    base.name = "nominal";
    base.description = "Firmware defaults, Delta 15.3 CFM blower (measured start/stall duties), flap installed, 4 kg basket, lid 90 s at 3 h and 20 s at 6 h, wind x2 at 5 h";
    base.th = firmwareThresholds(cfg::BLOWER_MAX_DUTY_PCT);
    scs.push_back(base);

    Scenario noflap = base;
    noflap.name = "no_flap";
    noflap.description = "Same, no backdraft flap: stack draft leaks through the duct when the fan is off";
    noflap.plant.flap_installed = false;
    scs.push_back(noflap);

    Scenario noflap_low = noflap;
    noflap_low.name = "no_flap_93c";
    noflap_low.description = "No flap at a 93 C (200 F) setpoint: draft alone exceeds the heat demand";
    noflap_low.setpoint_c = 93.0f;
    scs.push_back(noflap_low);

    Scenario flap_low = base;
    flap_low.name = "flap_93c";
    flap_low.description = "Flap installed at a 93 C (200 F) setpoint, for comparison";
    flap_low.setpoint_c = 93.0f;
    scs.push_back(flap_low);

    Scenario nocap = base;
    nocap.name = "caps_removed";
    nocap.description = "Phase 1 defaults: IGNITE 90 percent, no startup cap, max duty 100";
    nocap.th = firmwareThresholds(100);
    nocap.th.startup_max_duty_pct = 100;
    nocap.th.ignite_duty_pct = 90;
    nocap.th.ignite_exit_margin_c = 15.0f;
    scs.push_back(nocap);

    Scenario big = base;
    big.name = "fan_20cfm";
    big.description = "20 CFM blower (FireBoard class) with the same gains and caps";
    big.plant.fan_cfm_max = 20.0;
    scs.push_back(big);

    std::FILE* j = std::fopen(sweep_only ? "out/sweep.json" : "out/metrics.json", "w");
    std::fprintf(j, "[\n");
    if (!sweep_only) {
        for (size_t i = 0; i < scs.size(); ++i) {
            const Metrics m = run(scs[i]);
            writeMetrics(j, scs[i], m, i + 1 == scs.size());
            std::printf("%-14s reach %3.0f min ovs %5.1f  HOLD rms %.2f max %4.1f  wind %4.1f  duty %4.1f lpwm %.2f  lid %d/%d fp %d rec %5.1f  fuelLow %.2f h lead %3.0f min  fireOut %.2f h  creep %.2f h  used %.2f kg  burn %.3f\n",
                        scs[i].name.c_str(), m.first_reach_s / 60, m.startup_overshoot_c, m.hold_rms_c, m.hold_max_abs_c,
                        m.wind_max_dev_c, m.mean_duty_hold, m.long_pwm_fraction, m.lid_true_positive, m.lid_detections,
                        m.lid_false_positive, m.lid1_recovery_peak_c, m.fuel_low_first_s / 3600, m.fuel_low_to_fire_out_min,
                        m.fire_out_s / 3600, m.creep_first_s / 3600, m.fuel_used_kg, m.burn_avg_kg_h_hold);
        }
    } else {
        const float kps[] = {2.0f, 4.0f, 8.0f};
        const float kis[] = {0.005f, 0.02f, 0.05f};
        const float kds[] = {0.0f, 5.0f};
        int n = 0, total = 3 * 3 * 2;
        for (float kp : kps) for (float ki : kis) for (float kd : kds) {
            Scenario s = base;
            s.gains = {kp, ki, kd};
            s.write_files = false;
            char buf[64];
            std::snprintf(buf, sizeof buf, "kp%.0f_ki%.3f_kd%.0f", kp, ki, kd);
            s.name = buf;
            s.description = "gain sweep on the nominal plant";
            const Metrics m = run(s);
            writeMetrics(j, s, m, ++n == total);
        }
    }
    std::fprintf(j, "]\n");
    std::fclose(j);
    return 0;
}
