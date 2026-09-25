#include "blower.h"

#include <Arduino.h>

namespace hal {

bool Blower::begin(int pin, uint32_t freq_hz, uint8_t res_bits, uint8_t min_start_pct, uint8_t max_pct, uint32_t kick_ms,
                   uint32_t long_pwm_period_s) {
    pin_ = pin;
    freq_hz_ = freq_hz;
    res_bits_ = res_bits;
    max_count_ = (1u << res_bits) - 1;
    max_pct_ = max_pct;
    kick_ms_ = kick_ms;
    long_pwm_ = control::LongPwm(min_start_pct, long_pwm_period_s);

    // Drive the gate low before attaching PWM so there is no glitch on boot.
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
    const bool ok = ledcAttach(static_cast<uint8_t>(pin_), freq_hz_, res_bits_);
    if (ok) ledcWrite(static_cast<uint8_t>(pin_), 0);
    return ok;
}

void Blower::write(uint8_t pct) {
    if (pct > 100) pct = 100;
    applied_pct_ = pct;
    const uint32_t duty = (static_cast<uint32_t>(pct) * max_count_) / 100u;
    ledcWrite(static_cast<uint8_t>(pin_), duty);
}

// Apply a scheduled duty with the start kick.
void Blower::apply(uint8_t pct) {
    const bool was_off = (applied_pct_ == 0) && !kicking_;
    target_pct_ = pct;
    if (pct == 0) {
        kicking_ = false;
        write(0);
        return;
    }
    if (was_off && kick_ms_ > 0) {
        kicking_ = true;
        kick_end_ms_ = millis() + kick_ms_;
        write(max_pct_);
        return;
    }
    if (!kicking_) write(pct);
}

void Blower::setDutyPercent(uint8_t pct, float dt_s) {
    if (pct > max_pct_) pct = max_pct_;
    cmd_pct_ = pct;
    apply(long_pwm_.step(pct, dt_s));
}

void Blower::service(uint32_t now_ms) {
    if (kicking_ && static_cast<int32_t>(now_ms - kick_end_ms_) >= 0) {
        kicking_ = false;
        write(target_pct_);
    }
}

void Blower::off() {
    cmd_pct_ = 0;
    target_pct_ = 0;
    kicking_ = false;
    long_pwm_.reset();
    write(0);
}

bool Blower::setFrequency(uint32_t freq_hz) {
    const uint32_t got = ledcChangeFrequency(static_cast<uint8_t>(pin_), freq_hz, res_bits_);
    if (got == 0) return false;
    freq_hz_ = got;
    write(applied_pct_);
    return true;
}

}  // namespace hal
