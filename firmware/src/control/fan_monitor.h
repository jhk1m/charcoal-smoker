// Tach supervision for the 4-wire fan (doc 09 section 6a). The Delta BFB0712HH-AWPH runs at FULL speed when
// its PWM lead is open, so the tach is the only way to see a broken control line (doc 06 R5).
//   BLOWER_STALL:   driven above 0 percent with no tach pulses for stall_s.
//   BLOWER_RUNAWAY: RPM above (commanded share of rated + margin) for runaway_s.
// Pure C++ so it is unit-tested natively; main.cpp feeds it the pulse count once per control tick.
#pragma once

#include <algorithm>
#include <cstdint>

#include "state_machine.h"

namespace control {

struct FanLimits {
    uint16_t rated_rpm;          // full-speed RPM
    uint8_t runaway_margin_pct;  // percent of rated_rpm allowed above the commanded share
    uint32_t stall_s;
    uint32_t runaway_s;
};

class FanMonitor {
public:
    FanMonitor(const FanLimits& l, uint8_t pulses_per_rev) : l_(l) { setPulsesPerRev(pulses_per_rev); }

    void setPulsesPerRev(uint8_t ppr) { ppr_ = ppr == 0 ? 1 : ppr; }
    uint8_t pulsesPerRev() const { return ppr_; }

    // Bench switch: with the checks off, update() still measures RPM but never reports a fault.
    void setEnabled(bool en) {
        enabled_ = en;
        stall_acc_s_ = runaway_acc_s_ = 0.0f;
    }
    bool enabled() const { return enabled_; }

    // Call once per control tick with the tach pulses counted since the previous call.
    //   powered:    the fan supply was enabled over that interval
    //   driven_pct: duty the fan was actually given over that interval (after long-PWM and the kick)
    Fault update(bool powered, uint8_t driven_pct, uint32_t pulses, float dt_s) {
        if (dt_s <= 0.0f) return Fault::NONE;
        rpm_ = 60.0f * static_cast<float>(pulses) / (static_cast<float>(ppr_) * dt_s);
        if (!powered || !enabled_) {
            stall_acc_s_ = runaway_acc_s_ = 0.0f;
            return Fault::NONE;
        }

        const float rated = static_cast<float>(l_.rated_rpm);
        const float expected = rated * static_cast<float>(std::min<uint8_t>(driven_pct, 100)) / 100.0f;
        const float limit = expected + rated * static_cast<float>(l_.runaway_margin_pct) / 100.0f;
        if (rpm_ > limit) runaway_acc_s_ += dt_s;
        else runaway_acc_s_ = 0.0f;

        // Only driven time counts toward a stall, and only a driven interval with pulses clears it, so a
        // fan that never starts is still caught when long PWM drives it one second in every ten.
        if (driven_pct > 0) {
            if (pulses == 0) stall_acc_s_ += dt_s;
            else stall_acc_s_ = 0.0f;
        }

        if (runaway_acc_s_ >= static_cast<float>(l_.runaway_s)) return Fault::BLOWER_RUNAWAY;
        if (stall_acc_s_ >= static_cast<float>(l_.stall_s)) return Fault::BLOWER_STALL;
        return Fault::NONE;
    }

    float rpm() const { return rpm_; }

private:
    FanLimits l_;
    uint8_t ppr_ = 1;
    bool enabled_ = true;
    float rpm_ = 0.0f;
    float stall_acc_s_ = 0.0f;
    float runaway_acc_s_ = 0.0f;
};

}  // namespace control
