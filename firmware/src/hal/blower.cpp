#include "blower.h"

#include <Arduino.h>

namespace hal {

bool Blower::begin(int pin, uint32_t freq_hz, uint8_t res_bits, uint8_t min_start_pct, uint8_t min_run_pct, uint8_t max_pct,
                   uint32_t kick_ms, uint32_t long_pwm_period_s, uint32_t long_pwm_min_on_s, bool inverted,
                   int supply_en_pin) {
    pin_ = pin;
    supply_pin_ = supply_en_pin;
    inverted_ = inverted;
    freq_hz_ = freq_hz;
    res_bits_ = res_bits;
    max_count_ = (1u << res_bits) - 1;
    max_pct_ = max_pct;
    kick_ms_ = kick_ms;
    long_pwm_ = control::LongPwm(min_start_pct, long_pwm_period_s, min_run_pct, long_pwm_min_on_s);

    // Fan supply cut before anything else: the 10k gate pull-down has held it off through reset.
    if (supply_pin_ >= 0) {
        pinMode(supply_pin_, OUTPUT);
        digitalWrite(supply_pin_, LOW);
    }
    supply_on_ = false;
    armed_ = false;

    // Drive the gate low before attaching PWM so there is no glitch on boot.
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
    attached_ = ledcAttach(static_cast<uint8_t>(pin_), freq_hz_, res_bits_);
    if (attached_) writeCounts(0);
    return attached_;
}

void Blower::setSupply(bool on) {
    if (supply_pin_ < 0) return;
    // Without a working PWM output a 4-wire fan would run at full speed, so the supply stays cut.
    supply_on_ = on && attached_;
    digitalWrite(supply_pin_, supply_on_ ? HIGH : LOW);
}

void Blower::writeCounts(uint32_t counts) {
    if (!attached_) return;
    if (!ledcWrite(static_cast<uint8_t>(pin_), counts)) {
        // PWM output lost: an uncontrolled 4-wire fan runs flat out, so cut its supply. The tach monitor
        // then reports BLOWER_STALL.
        attached_ = false;
        setSupply(false);
        log_e("ledcWrite failed on GPIO%d, fan supply cut", pin_);
    }
}

void Blower::write(uint8_t pct) {
    if (pct > 100) pct = 100;
    applied_pct_ = pct;
    const uint32_t on = (static_cast<uint32_t>(pct) * max_count_) / 100u;
    // Inverted (open-drain driver): the GPIO is high for the part of the period the fan must see low.
    writeCounts(inverted_ ? max_count_ - on : on);
}

// Apply a scheduled duty with the start kick.
void Blower::apply(uint8_t pct) {
    const bool was_off = (applied_pct_ == 0) && !kicking_;
    target_pct_ = pct;
    if (pct == 0) {
        kicking_ = false;
        // Supply cut, not a PWM-lead stop: the next start is then a cold start, which is immediate on the
        // Delta (a PWM-0 restart is not). Gate low with the supply off so the PWM lead is released.
        armed_ = false;
        setSupply(false);
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
    // PWM first, supply second: the fan never has power while its PWM lead is released (= full speed).
    if (!armed_ && applied_pct_ > 0) {
        armed_ = true;
        setSupply(true);
    }
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
    armed_ = false;
    setSupply(false);
    applied_pct_ = 0;
    // Gate low in both drive models: legacy MOSFET off; 4-wire driver off, PWM lead released (supply is cut).
    writeCounts(0);
}

bool Blower::setFrequency(uint32_t freq_hz) {
    const uint32_t got = ledcChangeFrequency(static_cast<uint8_t>(pin_), freq_hz, res_bits_);
    if (got == 0) return false;
    freq_hz_ = got;
    if (armed_) write(applied_pct_);
    else writeCounts(0);
    return true;
}

}  // namespace hal
