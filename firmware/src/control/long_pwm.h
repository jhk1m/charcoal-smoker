// Long-period PWM for duties below the blower's minimum spin duty (HeaterMeter "long PWM", grillpid.cpp,
// verified 2026-09-21): over a period of period_s (10 s), run at min_pct for a fraction (requested / min)
// of the period and OFF for the rest. Requested duties at or above min_pct pass straight through.
// Pure C++ so it is unit-tested natively; hal::Blower calls it once per control tick.
#pragma once

#include <cstdint>

namespace control {

class LongPwm {
public:
    LongPwm(uint8_t min_pct, uint32_t period_s) : min_pct_(min_pct), period_s_(period_s) {}

    void setMinPct(uint8_t min_pct) { min_pct_ = min_pct; }
    uint8_t minPct() const { return min_pct_; }

    // requested: 0..100. Returns the duty to apply this tick (0, min_pct, or requested).
    uint8_t step(uint8_t requested, float dt_s) {
        if (requested == 0 || min_pct_ == 0 || period_s_ == 0) {
            phase_s_ = 0.0f;
            return requested;
        }
        if (requested >= min_pct_) {
            phase_s_ = 0.0f;
            return requested;
        }
        // On-time within the period is proportional to requested/min.
        const float period = static_cast<float>(period_s_);
        const float on_s = period * static_cast<float>(requested) / static_cast<float>(min_pct_);
        const uint8_t out = (phase_s_ < on_s) ? min_pct_ : 0;
        phase_s_ += dt_s;
        if (phase_s_ >= period) phase_s_ -= period;
        return out;
    }

    void reset() { phase_s_ = 0.0f; }

private:
    uint8_t min_pct_;
    uint32_t period_s_;
    float phase_s_ = 0.0f;
};

}  // namespace control
