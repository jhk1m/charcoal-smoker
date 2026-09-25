// Blower driver: LEDC PWM on a low-side N-MOSFET. Arduino-ESP32 3.x LEDC API (ledcAttach/ledcWrite).
// VERIFIED: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html
//   bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution);
//   bool ledcWrite(uint8_t pin, uint32_t duty);
//   uint32_t ledcChangeFrequency(uint8_t pin, uint32_t freq, uint8_t resolution);
//
// Duty handling (revised 2026-09-21, doc 06 P6): commands at or above min_start_pct are applied as-is;
// commands between 1 and min_start_pct - 1 are emulated with long-period PWM (control::LongPwm) so the
// average airflow can go below the minimum spin speed instead of being compressed into [min, max].
// A 100 percent kick of kick_ms breaks stiction whenever the motor starts from OFF.
#pragma once

#include <cstdint>

#include "control/long_pwm.h"

namespace hal {

class Blower {
public:
    bool begin(int pin, uint32_t freq_hz, uint8_t res_bits, uint8_t min_start_pct, uint8_t max_pct, uint32_t kick_ms,
               uint32_t long_pwm_period_s);

    // Commanded duty in percent (0..100). Call once per control tick (long PWM is stepped here).
    void setDutyPercent(uint8_t pct, float dt_s);

    // Emergency/unconditional off. Also called by fault paths.
    void off();

    // Bench helper for PWM frequency selection (arch doc section 4). Re-attaches at the new frequency.
    bool setFrequency(uint32_t freq_hz);
    uint32_t frequency() const { return freq_hz_; }

    // Bench helper: minimum duty at which the motor spins from rest. Below it the long-PWM path is used.
    void setMinStart(uint8_t pct) { long_pwm_.setMinPct(pct); }
    uint8_t minStart() const { return long_pwm_.minPct(); }

    // Software clamp on commanded duty.
    void setMaxPercent(uint8_t pct) { max_pct_ = pct; }
    uint8_t maxPercent() const { return max_pct_; }

    uint8_t commandedPercent() const { return cmd_pct_; }
    uint8_t appliedPercent() const { return applied_pct_; }

    // Must be called from the control loop so the start kick can time out.
    void service(uint32_t now_ms);

private:
    void write(uint8_t pct);
    void apply(uint8_t pct);

    int pin_ = -1;
    uint32_t freq_hz_ = 0;
    uint8_t res_bits_ = 10;
    uint32_t max_count_ = 1023;
    uint8_t max_pct_ = 100;
    uint32_t kick_ms_ = 0;
    uint8_t cmd_pct_ = 0;
    uint8_t target_pct_ = 0;   // after long-PWM scheduling
    uint8_t applied_pct_ = 0;
    bool kicking_ = false;
    uint32_t kick_end_ms_ = 0;
    control::LongPwm long_pwm_{0, 0};
};

}  // namespace hal
