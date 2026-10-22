// Long-period PWM for duties below the blower's minimum spin duty (HeaterMeter "long PWM", grillpid.cpp,
// verified 2026-09-21): over a period of period_s (10 s), run at min_pct for a fraction (requested / min)
// of the period and OFF for the rest. Requested duties at or above min_pct pass straight through.
// Hysteresis (2026-10-22): once the fan is spinning the pass-through threshold drops to min_run_pct, so a
// request between min_run and min_start keeps the fan turning instead of stop-starting it every period.
// Pure C++ so it is unit-tested natively; hal::Blower calls it once per control tick.
#pragma once

#include <cstdint>

namespace control {

class LongPwm {
public:
    // min_run_pct 0 = no hysteresis (threshold is min_pct in both directions).
    // min_on_s: shortest on-phase; the period stretches so requested/min_pct is still the average.
    LongPwm(uint8_t min_pct, uint32_t period_s, uint8_t min_run_pct = 0, uint32_t min_on_s = 0)
        : min_pct_(min_pct), min_run_pct_(min_run_pct), period_s_(period_s), min_on_s_(min_on_s) {}

    void setMinPct(uint8_t min_pct) { min_pct_ = min_pct; }
    uint8_t minPct() const { return min_pct_; }
    // Threshold used while the fan is spinning; clamped to min_pct.
    uint8_t minRunPct() const { return (min_run_pct_ == 0 || min_run_pct_ > min_pct_) ? min_pct_ : min_run_pct_; }

    // requested: 0..100. Returns the duty to apply this tick (0, min_pct, or requested).
    uint8_t step(uint8_t requested, float dt_s) {
        if (requested == 0 || min_pct_ == 0 || period_s_ == 0) {
            phase_s_ = 0.0f;
            last_out_ = requested;
            return requested;
        }
        // Spinning fan (last output non-zero) passes through down to min_run; from rest it needs min_start.
        const uint8_t threshold = (last_out_ > 0) ? minRunPct() : min_pct_;
        if (requested >= threshold) {
            phase_s_ = 0.0f;
            last_out_ = requested;
            return requested;
        }
        // On-time within the period is proportional to requested/min.
        float period = static_cast<float>(period_s_);
        float on_s = period * static_cast<float>(requested) / static_cast<float>(min_pct_);
        if (on_s < static_cast<float>(min_on_s_)) {
            // Stretch the period instead of the duty: the fan needs min_on_s to restart and be seen by the tach.
            on_s = static_cast<float>(min_on_s_);
            period = on_s * static_cast<float>(min_pct_) / static_cast<float>(requested);
        }
        const uint8_t out = (phase_s_ < on_s) ? min_pct_ : 0;
        phase_s_ += dt_s;
        if (phase_s_ >= period) phase_s_ -= period;
        last_out_ = out;
        return out;
    }

    void reset() { phase_s_ = 0.0f; last_out_ = 0; }

private:
    uint8_t min_pct_;
    uint8_t min_run_pct_;
    uint32_t period_s_;
    uint32_t min_on_s_;
    float phase_s_ = 0.0f;
    uint8_t last_out_ = 0;
};

}  // namespace control
