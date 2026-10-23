// Cook state machine. Pure C++: takes a Sample each tick, returns a duty command plus warnings.
// States per 02-phase1-architecture.md section 6, revised per 06 section 7 (2026-09-21):
//   startup duty cap with integral clamp and halve-I at first setpoint crossing (HeaterMeter),
//   rate-based lid detection with fire-out guard and plateau exit (HeaterMeter LidTrack),
//   RECOVERY state after a lid event, drum-specific rising-lid check,
//   FUEL_LOW / CREEP / pit deviation warnings, FIRE_OUT fault.
#pragma once

#include <algorithm>
#include <cstdint>

#include "pid.h"

namespace control {

enum class State : uint8_t {
    IDLE = 0,
    IGNITE = 1,
    RAMP = 2,
    HOLD = 3,
    LID_OPEN = 4,
    FAULT = 5,
    MANUAL = 6,
    RECOVERY = 7,   // after LID_OPEN: closed loop with the startup cap until settled
};

enum class Fault : uint8_t {
    NONE = 0,
    SENSOR_OPEN = 1,
    SENSOR_SHORT = 2,      // OVUV or short-to-rail
    SENSOR_RANGE = 3,      // reading outside plausible range
    IGNITE_TIMEOUT = 4,    // no temperature rise during IGNITE
    PIT_OVER_LIMIT = 5,    // software over-temp
    WATCHDOG_RESET = 6,    // set at boot if reset reason was TWDT
    FIRE_OUT = 7,          // pit far below setpoint with the blower pegged for a long time
    BLOWER_STALL = 8,      // 4-wire fan driven with no tach pulses (control::FanMonitor)
    BLOWER_RUNAWAY = 9,    // 4-wire fan far faster than commanded: broken PWM lead (control::FanMonitor)
};

// Bitmask, recomputed every tick (FUEL_LOW is latched with a clear delay). Never affects the blower directly.
namespace Warning {
constexpr uint8_t NONE = 0;
constexpr uint8_t PIT_HIGH = 1u << 0;   // pit >= setpoint + deviation
constexpr uint8_t PIT_LOW = 1u << 1;    // pit <= setpoint - deviation
constexpr uint8_t FUEL_LOW = 1u << 2;   // blower near max and pit falling: add charcoal
constexpr uint8_t CREEP = 1u << 3;      // pit above setpoint with blower off: draft leak or too much lit fuel
constexpr uint8_t LID_RISING = 1u << 4; // lid event detected as a fast rise (drum), not a drop
constexpr uint8_t TIMER_DONE = 1u << 5; // cook timer reached zero; stays up until the timer is set again or the cook stops
}  // namespace Warning

const char* toString(State s);
const char* toString(Fault f);

struct Thresholds {
    // IGNITE
    float ignite_exit_margin_c;
    uint8_t ignite_duty_pct;
    uint32_t ignite_timeout_s;
    float ignite_min_rise_c;
    // RAMP / RECOVERY / HOLD
    uint8_t startup_max_duty_pct;     // duty and integral cap until the setpoint is first reached
    uint8_t max_duty_pct;
    float hold_band_c;
    uint32_t hold_settle_s;
    uint32_t recovery_max_s;          // RECOVERY -> HOLD at the latest after this
    // LID_OPEN, rate style (LidTrack). Rate is measured over lid_rate_window_s.
    float lid_open_drop_c_per_s;      // trigger at reference setpoint (121 C); scaled up to 4x at 260 C
    float lid_open_total_drop_c;      // classic backstop: pit this far below setpoint; 0 disables
    uint32_t lid_rate_window_s;
    uint32_t lid_open_min_s;          // never auto-resume before this
    uint32_t lid_open_timeout_s;
    uint8_t lid_open_duty_pct;        // blower floor while the lid is open
    float lid_open_recover_band_c;    // exit when pit >= setpoint - band
    float lid_exit_rise_above_low_c;  // plateau exit: pit this far above the low peak ...
    float lid_exit_plateau_c_per_s;   // ... and 0 < rate < this
    uint8_t lid_guard_out_ema_pct;    // no lid mode when the output EMA is at/above this (probable fuel out)
    float lid_rising_rate_c_per_s;    // drum: fast rise with the blower off = lid opened
    uint32_t lid_rising_window_s;
    uint8_t lid_rising_duty_pct;      // rising check only when commanded duty <= this
    // Averages
    float out_ema_alpha;              // 2/241 at 1 Hz
    float probe_ema_alpha;            // 2/61 at 1 Hz (PID derivative reference)
    // Warnings / faults
    float fuel_low_fall_c;            // pit fell this much over fuel_low_window_s ...
    uint32_t fuel_low_window_s;       // ... while out EMA >= fuel_low_out_pct
    uint8_t fuel_low_out_pct;
    uint32_t fuel_low_clear_s;        // once raised, FUEL_LOW stays up until the condition has been false this long
    float fire_out_below_sp_c;        // pit this far below setpoint ...
    uint32_t fire_out_s;              // ... for this long with duty pegged at max -> FAULT
    float pit_alarm_dev_c;
    float pit_alarm_hyst_c;
    float creep_above_sp_c;           // pit above setpoint by this with duty 0 ...
    uint32_t creep_s;                 // ... for this long -> CREEP warning
    float pit_over_margin_c;          // over-limit fault at setpoint + this ...
    float pit_soft_limit_c;           // ... capped here (absolute, below the thermostat dial)
};

struct Sample {
    float pit_c;         // filtered pit temperature
    bool sensor_ok;      // false -> fault path
    Fault sensor_fault;  // which sensor fault, if !sensor_ok
    float dt_s;          // seconds since previous sample
};

struct Command {
    uint8_t duty_pct;    // 0..100, before blower min-start / long-PWM handling
    State state;
    Fault fault;
    uint8_t warnings;    // Warning bitmask
};

class StateMachine {
public:
    StateMachine(const Thresholds& t, PidGains gains);

    // User intents
    void startCook(float setpoint_c);          // IDLE/MANUAL -> IGNITE (refused in FAULT)
    void stop();                               // any -> IDLE
    void setSetpoint(float setpoint_c);        // live change; closed-loop states go back to RAMP with the startup cap
    void enterManual(uint8_t duty_pct);        // any non-FAULT -> MANUAL
    void setManualDuty(uint8_t duty_pct);
    void clearFault();                         // FAULT -> IDLE
    void raiseFault(Fault f);                  // fault found outside tick() (fan tach): any active state -> FAULT
    void setGains(PidGains g) { pid_.setGains(g); }
    void setThresholds(const Thresholds& t) { t_ = t; applyCaps(); }
    const Thresholds& thresholds() const { return t_; }
    void setLidDetectEnabled(bool en) { lid_detect_enabled_ = en; }
    void setTimer(uint32_t seconds) { timer_s_ = static_cast<float>(seconds); timer_done_ = false; }  // 0 clears
    uint32_t timerS() const { return static_cast<uint32_t>(timer_s_); }
    float overLimitC() const { return std::min(setpoint_c_ + t_.pit_over_margin_c, t_.pit_soft_limit_c); }

    Command tick(const Sample& s);

    State state() const { return state_; }
    Fault fault() const { return fault_; }
    uint8_t warnings() const { return warnings_; }
    float setpoint() const { return setpoint_c_; }
    bool setpointReached() const { return reached_setpoint_; }
    float outputEma() const { return out_ema_; }
    float rateCPerS() const { return rate_c_per_s_; }
    const Pid& pid() const { return pid_; }

private:
    static constexpr uint8_t RATE_RING = 16;   // >= lid_rate_window_s at 1 Hz
    static constexpr uint8_t FUEL_RING = 24;   // decimated pit history for the fuel-low window
    static constexpr uint32_t FUEL_DECIMATE_S = 30;

    void transition(State next, const Sample& s);
    void enterClosedLoop(bool fresh, const Sample& s);
    void applyCaps();
    float closedLoopDuty(const Sample& s);
    void updateRate(const Sample& s);
    void updateFuelHistory(const Sample& s);
    float lidRateThreshold() const;
    bool lidDropDetected(const Sample& s) const;
    bool lidRiseDetected(const Sample& s, uint8_t duty);
    void updateWarnings(const Sample& s, uint8_t duty);
    bool fireOutDetected(const Sample& s, uint8_t duty);
    void updateFuelLow(const Sample& s);

    Thresholds t_;
    Pid pid_;
    State state_ = State::IDLE;
    Fault fault_ = Fault::NONE;
    uint8_t warnings_ = 0;
    float setpoint_c_ = 0.0f;
    uint8_t manual_duty_ = 0;
    bool lid_detect_enabled_ = true;
    float timer_s_ = 0.0f;      // ponytail: plain countdown in the control tick, runs in any state
    bool timer_done_ = false;

    // per-state bookkeeping
    float state_elapsed_s_ = 0.0f;
    float ignite_start_c_ = 0.0f;
    bool ignite_baseline_set_ = false;
    float hold_settle_s_ = 0.0f;
    float last_pit_c_ = 0.0f;
    bool reached_setpoint_ = false;   // since last setpoint change or lid event
    bool lid_rising_variant_ = false;
    float lid_low_c_ = 0.0f;          // lowest pit seen in LID_OPEN (drop variant)
    float lid_high_c_ = 0.0f;         // highest pit seen in LID_OPEN (rising variant)
    float rise_accum_s_ = 0.0f;
    float fire_out_accum_s_ = 0.0f;
    float creep_accum_s_ = 0.0f;
    bool pit_high_ = false, pit_low_ = false;
    bool fuel_low_ = false;
    float fuel_low_clear_accum_s_ = 0.0f;

    // rate over a short window
    float rate_ring_c_[RATE_RING] = {};
    float rate_ring_dt_[RATE_RING] = {};
    uint8_t rate_head_ = 0, rate_count_ = 0;
    float rate_c_per_s_ = 0.0f;

    // decimated history for FUEL_LOW
    float fuel_ring_c_[FUEL_RING] = {};
    uint8_t fuel_head_ = 0, fuel_count_ = 0;
    float fuel_decimate_acc_s_ = 0.0f;

    // output average (fire-out guard for lid mode)
    float out_ema_ = 0.0f;
};

}  // namespace control
