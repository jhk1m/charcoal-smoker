#include "state_machine.h"

#include <algorithm>
#include <cmath>

namespace control {

const char* toString(State s) {
    switch (s) {
        case State::IDLE: return "IDLE";
        case State::IGNITE: return "IGNITE";
        case State::RAMP: return "RAMP";
        case State::HOLD: return "HOLD";
        case State::LID_OPEN: return "LID_OPEN";
        case State::FAULT: return "FAULT";
        case State::MANUAL: return "MANUAL";
        case State::RECOVERY: return "RECOVERY";
    }
    return "?";
}

const char* toString(Fault f) {
    switch (f) {
        case Fault::NONE: return "NONE";
        case Fault::SENSOR_OPEN: return "SENSOR_OPEN";
        case Fault::SENSOR_SHORT: return "SENSOR_SHORT";
        case Fault::SENSOR_RANGE: return "SENSOR_RANGE";
        case Fault::IGNITE_TIMEOUT: return "IGNITE_TIMEOUT";
        case Fault::PIT_OVER_LIMIT: return "PIT_OVER_LIMIT";
        case Fault::WATCHDOG_RESET: return "WATCHDOG_RESET";
        case Fault::FIRE_OUT: return "FIRE_OUT";
        case Fault::BLOWER_STALL: return "BLOWER_STALL";
        case Fault::BLOWER_RUNAWAY: return "BLOWER_RUNAWAY";
    }
    return "?";
}

StateMachine::StateMachine(const Thresholds& t, PidGains gains)
    : t_(t), pid_(gains, 0.0f, static_cast<float>(t.startup_max_duty_pct)) {
    pid_.setMeasurementAvgAlpha(t_.probe_ema_alpha);
    pid_.setIntegralMax(static_cast<float>(t_.startup_max_duty_pct));
}

// ---------------------------------------------------------------- caps

void StateMachine::applyCaps() {
    const float cap = static_cast<float>(reached_setpoint_ ? t_.max_duty_pct : t_.startup_max_duty_pct);
    pid_.setOutputLimits(0.0f, cap);
    pid_.setIntegralMax(cap);
}

void StateMachine::enterClosedLoop(bool fresh, const Sample& s) {
    pid_.setIntegralFrozen(false);
    if (fresh) pid_.reset(s.pit_c);
    applyCaps();
}

// ---------------------------------------------------------------- transitions

void StateMachine::transition(State next, const Sample& s) {
    const State prev = state_;
    state_ = next;
    state_elapsed_s_ = 0.0f;
    hold_settle_s_ = 0.0f;
    rise_accum_s_ = 0.0f;
    fire_out_accum_s_ = 0.0f;
    creep_accum_s_ = 0.0f;

    switch (next) {
        case State::IGNITE:
            ignite_start_c_ = s.pit_c;
            ignite_baseline_set_ = false;
            reached_setpoint_ = false;
            break;
        case State::RAMP:
            // Fresh PID when coming from open loop (IGNITE, IDLE, MANUAL). A setpoint change from a
            // closed-loop state keeps the integral (clamped to the startup cap by applyCaps()).
            enterClosedLoop(prev == State::IGNITE || prev == State::IDLE || prev == State::MANUAL, s);
            break;
        case State::HOLD:
            // Settled inside the band counts as reached even if the pit never crossed the setpoint,
            // otherwise a weak fire could stay capped at the startup duty forever.
            reached_setpoint_ = true;
            enterClosedLoop(false, s);
            break;
        case State::RECOVERY:
            enterClosedLoop(false, s);
            break;
        case State::LID_OPEN:
            pid_.setIntegralFrozen(true);
            reached_setpoint_ = false;   // HeaterMeter: "reached" is since the last setpoint change or lid event
            lid_low_c_ = s.pit_c;
            lid_high_c_ = s.pit_c;
            break;
        case State::IDLE:
        case State::FAULT:
        case State::MANUAL:
            pid_.setIntegralFrozen(false);
            reached_setpoint_ = false;
            warnings_ = 0;
            pit_high_ = pit_low_ = fuel_low_ = false;
            fuel_low_clear_accum_s_ = 0.0f;
            break;
    }
}

void StateMachine::startCook(float setpoint_c) {
    if (state_ == State::FAULT) return;  // must clearFault() first
    setpoint_c_ = setpoint_c;
    Sample none{last_pit_c_, true, Fault::NONE, 0.0f};
    transition(State::IGNITE, none);
}

void StateMachine::stop() {
    if (state_ == State::FAULT) return;
    setTimer(0);
    Sample none{last_pit_c_, true, Fault::NONE, 0.0f};
    transition(State::IDLE, none);
}

void StateMachine::setSetpoint(float setpoint_c) {
    setpoint_c_ = setpoint_c;
    reached_setpoint_ = false;
    Sample none{last_pit_c_, true, Fault::NONE, 0.0f};
    switch (state_) {
        case State::RAMP:
        case State::HOLD:
        case State::RECOVERY:
            transition(State::RAMP, none);  // HeaterMeter: setpoint change re-enters STARTUP
            break;
        default:
            break;
    }
}

void StateMachine::enterManual(uint8_t duty_pct) {
    if (state_ == State::FAULT) return;
    manual_duty_ = std::min<uint8_t>(duty_pct, t_.max_duty_pct);
    Sample none{last_pit_c_, true, Fault::NONE, 0.0f};
    transition(State::MANUAL, none);
}

void StateMachine::setManualDuty(uint8_t duty_pct) {
    manual_duty_ = std::min<uint8_t>(duty_pct, t_.max_duty_pct);
}

void StateMachine::clearFault() {
    if (state_ != State::FAULT) return;
    fault_ = Fault::NONE;
    Sample none{last_pit_c_, true, Fault::NONE, 0.0f};
    transition(State::IDLE, none);
}

void StateMachine::raiseFault(Fault f) {
    // Same scope as the checks in tick(): nothing to fault in IDLE, and the first fault stays latched.
    if (f == Fault::NONE || state_ == State::IDLE || state_ == State::FAULT) return;
    fault_ = f;
    Sample none{last_pit_c_, true, Fault::NONE, 0.0f};
    transition(State::FAULT, none);
}

// ---------------------------------------------------------------- history and rates

void StateMachine::updateRate(const Sample& s) {
    if (s.dt_s <= 0.0f) return;
    rate_ring_c_[rate_head_] = s.pit_c;
    rate_ring_dt_[rate_head_] = s.dt_s;
    rate_head_ = static_cast<uint8_t>((rate_head_ + 1) % RATE_RING);
    if (rate_count_ < RATE_RING) ++rate_count_;

    // Walk back until the window is covered. dt of the newest sample spans newest-1 -> newest.
    const float window = static_cast<float>(t_.lid_rate_window_s);
    float span = 0.0f;
    uint8_t idx = static_cast<uint8_t>((rate_head_ + RATE_RING - 1) % RATE_RING);  // newest
    uint8_t steps = 0;
    while (steps + 1 < rate_count_ && span < window) {
        span += rate_ring_dt_[idx];
        idx = static_cast<uint8_t>((idx + RATE_RING - 1) % RATE_RING);
        ++steps;
    }
    if (span >= 0.5f * window && span > 0.0f) {
        rate_c_per_s_ = (s.pit_c - rate_ring_c_[idx]) / span;
    } else {
        rate_c_per_s_ = 0.0f;
    }
}

void StateMachine::updateFuelHistory(const Sample& s) {
    fuel_decimate_acc_s_ += s.dt_s;
    if (fuel_count_ == 0 || fuel_decimate_acc_s_ >= static_cast<float>(FUEL_DECIMATE_S)) {
        fuel_decimate_acc_s_ = 0.0f;
        fuel_ring_c_[fuel_head_] = s.pit_c;
        fuel_head_ = static_cast<uint8_t>((fuel_head_ + 1) % FUEL_RING);
        if (fuel_count_ < FUEL_RING) ++fuel_count_;
    }
}

void StateMachine::updateFuelLow(const Sample& s) {
    const uint8_t back = static_cast<uint8_t>(std::min<uint32_t>(t_.fuel_low_window_s / FUEL_DECIMATE_S, FUEL_RING - 1));
    if (fuel_count_ <= back) return;
    const uint8_t idx = static_cast<uint8_t>((fuel_head_ + FUEL_RING - 1 - back) % FUEL_RING);
    const float fall = fuel_ring_c_[idx] - s.pit_c;
    const bool cond = (out_ema_ >= static_cast<float>(t_.fuel_low_out_pct)) && (fall >= t_.fuel_low_fall_c);
    // Latch with a clear delay: the raw condition flickers as the pit hovers around the fall threshold
    // (seen in the closed-loop sim during a wind gust), which would spam the phone.
    if (cond) {
        fuel_low_ = true;
        fuel_low_clear_accum_s_ = 0.0f;
    } else if (fuel_low_) {
        fuel_low_clear_accum_s_ += s.dt_s;
        if (fuel_low_clear_accum_s_ >= static_cast<float>(t_.fuel_low_clear_s)) {
            fuel_low_ = false;
            fuel_low_clear_accum_s_ = 0.0f;
        }
    }
}

// ---------------------------------------------------------------- lid detection

float StateMachine::lidRateThreshold() const {
    // LidTrack: threshold scales from 1x at <= 121 C (250 F) to 4x at >= 260 C (500 F).
    const float f = std::clamp((setpoint_c_ - 121.0f) / (260.0f - 121.0f), 0.0f, 1.0f);
    return t_.lid_open_drop_c_per_s * (1.0f + 3.0f * f);
}

bool StateMachine::lidDropDetected(const Sample& s) const {
    if (!lid_detect_enabled_ || !reached_setpoint_) return false;
    if (s.pit_c >= setpoint_c_) return false;
    if (out_ema_ >= static_cast<float>(t_.lid_guard_out_ema_pct)) return false;  // probable fuel out, not a lid
    if (rate_c_per_s_ <= -lidRateThreshold()) return true;
    if (t_.lid_open_total_drop_c > 0.0f && (setpoint_c_ - s.pit_c) >= t_.lid_open_total_drop_c) return true;
    return false;
}

bool StateMachine::lidRiseDetected(const Sample& s, uint8_t duty) {
    if (!lid_detect_enabled_ || !reached_setpoint_ || duty > t_.lid_rising_duty_pct) {
        rise_accum_s_ = 0.0f;
        return false;
    }
    if (rate_c_per_s_ >= t_.lid_rising_rate_c_per_s) rise_accum_s_ += s.dt_s;
    else rise_accum_s_ = 0.0f;
    return rise_accum_s_ >= static_cast<float>(t_.lid_rising_window_s);
}

// ---------------------------------------------------------------- warnings and fire-out

bool StateMachine::fireOutDetected(const Sample& s, uint8_t duty) {
    const uint8_t cap = reached_setpoint_ ? t_.max_duty_pct : t_.startup_max_duty_pct;
    const bool pegged = duty >= cap;
    const bool far_below = s.pit_c <= setpoint_c_ - t_.fire_out_below_sp_c;
    if (pegged && far_below) fire_out_accum_s_ += s.dt_s;
    else fire_out_accum_s_ = 0.0f;
    return fire_out_accum_s_ >= static_cast<float>(t_.fire_out_s);
}

void StateMachine::updateWarnings(const Sample& s, uint8_t duty) {
    uint8_t w = 0;
    const bool closed = state_ == State::HOLD || state_ == State::RECOVERY || state_ == State::RAMP;

    if (closed && reached_setpoint_) {
        // Deviation alarms with 1 C arming hysteresis (HeaterMeter).
        if (!pit_high_ && s.pit_c >= setpoint_c_ + t_.pit_alarm_dev_c) pit_high_ = true;
        if (pit_high_ && s.pit_c < setpoint_c_ + t_.pit_alarm_dev_c - t_.pit_alarm_hyst_c) pit_high_ = false;
        if (!pit_low_ && s.pit_c <= setpoint_c_ - t_.pit_alarm_dev_c) pit_low_ = true;
        if (pit_low_ && s.pit_c > setpoint_c_ - t_.pit_alarm_dev_c + t_.pit_alarm_hyst_c) pit_low_ = false;
    } else {
        pit_high_ = pit_low_ = false;
    }

    if (closed) {
        // FUEL_LOW is evaluated only in settled HOLD. In RAMP/RECOVERY the blower is legitimately pegged and
        // the 10 min look-back still sees the pre-lid temperature, which raised a false FUEL_LOW after every
        // lid event in the closed-loop sim when the caps were removed.
        if (state_ == State::HOLD && state_elapsed_s_ >= static_cast<float>(t_.fuel_low_window_s)) updateFuelLow(s);
        if (duty == 0 && s.pit_c >= setpoint_c_ + t_.creep_above_sp_c) creep_accum_s_ += s.dt_s;
        else creep_accum_s_ = 0.0f;
    } else if (state_ != State::LID_OPEN) {
        fuel_low_ = false;
        fuel_low_clear_accum_s_ = 0.0f;
        creep_accum_s_ = 0.0f;
    } else {
        creep_accum_s_ = 0.0f;   // lid open: keep a raised FUEL_LOW, it is still true
    }

    if (pit_high_) w |= Warning::PIT_HIGH;
    if (pit_low_) w |= Warning::PIT_LOW;
    if (fuel_low_) w |= Warning::FUEL_LOW;
    if (creep_accum_s_ >= static_cast<float>(t_.creep_s)) w |= Warning::CREEP;
    if (state_ == State::LID_OPEN && lid_rising_variant_) w |= Warning::LID_RISING;
    if (timer_done_) w |= Warning::TIMER_DONE;
    warnings_ = w;
}

// ---------------------------------------------------------------- closed-loop duty

float StateMachine::closedLoopDuty(const Sample& s) {
    if (!reached_setpoint_ && s.pit_c >= setpoint_c_) {
        reached_setpoint_ = true;
        if (state_ == State::RAMP) pid_.halveIntegral();  // HeaterMeter STARTUP first-crossing rule
        applyCaps();
    }
    return pid_.update(setpoint_c_, s.pit_c, s.dt_s);
}

// ---------------------------------------------------------------- tick

Command StateMachine::tick(const Sample& s) {
    state_elapsed_s_ += s.dt_s;
    if (timer_s_ > 0.0f) {
        timer_s_ -= s.dt_s;
        if (timer_s_ <= 0.0f) { timer_s_ = 0.0f; timer_done_ = true; }
    }
    if (s.sensor_ok) {
        updateRate(s);
        updateFuelHistory(s);
    }

    // ---- Fault checks apply in every state except IDLE/FAULT ----
    if (state_ != State::IDLE && state_ != State::FAULT) {
        if (!s.sensor_ok) {
            fault_ = s.sensor_fault == Fault::NONE ? Fault::SENSOR_RANGE : s.sensor_fault;
            transition(State::FAULT, s);
        } else if (s.pit_c >= overLimitC()) {
            fault_ = Fault::PIT_OVER_LIMIT;
            transition(State::FAULT, s);
        }
    }

    uint8_t duty = 0;

    switch (state_) {
        case State::IDLE:
        case State::FAULT:
            duty = 0;
            break;

        case State::MANUAL:
            duty = manual_duty_;
            break;

        case State::IGNITE:
            duty = t_.ignite_duty_pct;
            // Baseline for the "no rise" check is the first real sample seen in IGNITE, not whatever
            // stale value startCook() had (it may be called before any sample exists).
            if (!ignite_baseline_set_) {
                ignite_start_c_ = s.pit_c;
                ignite_baseline_set_ = true;
            }
            if (s.pit_c >= setpoint_c_ - t_.ignite_exit_margin_c) {
                transition(State::RAMP, s);
            } else if (state_elapsed_s_ >= static_cast<float>(t_.ignite_timeout_s) &&
                       (s.pit_c - ignite_start_c_) < t_.ignite_min_rise_c) {
                fault_ = Fault::IGNITE_TIMEOUT;
                transition(State::FAULT, s);
                duty = 0;
            }
            break;

        case State::RAMP:
        case State::RECOVERY:
        case State::HOLD: {
            duty = static_cast<uint8_t>(std::lround(closedLoopDuty(s)));

            if (fireOutDetected(s, duty)) {
                fault_ = Fault::FIRE_OUT;
                transition(State::FAULT, s);
                duty = 0;
                break;
            }

            if (lidDropDetected(s)) {
                lid_rising_variant_ = false;
                transition(State::LID_OPEN, s);
                duty = t_.lid_open_duty_pct;
                break;
            }
            if (lidRiseDetected(s, duty)) {
                lid_rising_variant_ = true;
                transition(State::LID_OPEN, s);
                duty = t_.lid_open_duty_pct;
                break;
            }

            if (state_ != State::HOLD) {
                if (std::fabs(setpoint_c_ - s.pit_c) <= t_.hold_band_c) {
                    hold_settle_s_ += s.dt_s;
                    if (hold_settle_s_ >= static_cast<float>(t_.hold_settle_s)) transition(State::HOLD, s);
                } else {
                    hold_settle_s_ = 0.0f;
                }
                if (state_ == State::RECOVERY && state_elapsed_s_ >= static_cast<float>(t_.recovery_max_s)) {
                    transition(State::HOLD, s);
                }
            }
            break;
        }

        case State::LID_OPEN: {
            duty = t_.lid_open_duty_pct;
            lid_low_c_ = std::min(lid_low_c_, s.pit_c);
            lid_high_c_ = std::max(lid_high_c_, s.pit_c);
            const bool past_min = state_elapsed_s_ >= static_cast<float>(t_.lid_open_min_s);
            bool exit = state_elapsed_s_ >= static_cast<float>(t_.lid_open_timeout_s);
            if (past_min) {
                if (lid_rising_variant_) {
                    // Drum lid closed again: the rise has stopped.
                    if (rate_c_per_s_ <= 0.0f) exit = true;
                } else {
                    if (s.pit_c >= setpoint_c_ - t_.lid_open_recover_band_c) exit = true;
                    // LidTrack plateau: well above the low peak and no longer climbing fast.
                    if (s.pit_c >= lid_low_c_ + t_.lid_exit_rise_above_low_c && rate_c_per_s_ > 0.0f &&
                        rate_c_per_s_ < t_.lid_exit_plateau_c_per_s) {
                        exit = true;
                    }
                }
            }
            if (exit) {
                transition(State::RECOVERY, s);
                duty = static_cast<uint8_t>(std::lround(closedLoopDuty(s)));
            }
            break;
        }
    }

    duty = std::min<uint8_t>(duty, t_.max_duty_pct);

    // Output average, normalised to the current max duty so a 60 percent cap still reaches the 90
    // percent fire-out guard when the blower is pinned.
    const float norm = t_.max_duty_pct > 0 ? 100.0f * duty / static_cast<float>(t_.max_duty_pct) : 0.0f;
    out_ema_ += t_.out_ema_alpha * (norm - out_ema_);

    updateWarnings(s, duty);
    last_pit_c_ = s.pit_c;
    return Command{duty, state_, fault_, warnings_};
}

}  // namespace control
