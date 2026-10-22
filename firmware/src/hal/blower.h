// Blower driver: LEDC PWM. Arduino-ESP32 3.x LEDC API (ledcAttach/ledcWrite).
// VERIFIED: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html
//   bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution);
//   bool ledcWrite(uint8_t pin, uint32_t duty);
//   uint32_t ledcChangeFrequency(uint8_t pin, uint32_t freq, uint8_t resolution);
// VERIFIED 2026-10-04 in cores/esp32/esp32-hal-ledc.c (3.3.12): ledcWrite() turns a duty at or above
// (2^resolution - 1) into 2^resolution, a constant high level, so both ends of the range are glitch-free.
//
// Two drive models (doc 04 "Blower loop"):
//   4-wire (Delta BFB0712HH-AWPH): the PWM pin drives a 2N7000 that pulls the fan's PWM lead low, so the
//     GPIO duty is INVERTED (GPIO high = fan sees low). A second pin, SUPPLY_EN, gates the fan supply through
//     an IRLZ44N. The PWM pin is driven only while the supply is enabled: with the supply cut the pin is low,
//     the 2N7000 is off and the PWM lead is released. off() cuts the supply; setDutyPercent() enables it.
//   Legacy 2-wire: the PWM pin drives a low-side MOSFET directly, no inversion, no supply pin (pass -1).
//
// Duty handling (revised 2026-09-21, doc 06 P6): commands at or above min_start_pct are applied as-is;
// commands between 1 and min_start_pct - 1 are emulated with long-period PWM (control::LongPwm) so the
// average airflow can go below the minimum spin speed instead of being compressed into [min, max]. Once the
// fan is spinning the threshold drops to min_run_pct (BLOWER_MIN_RUN_DUTY_PCT) so it is not stop-started.
// A 100 percent kick of kick_ms breaks stiction whenever the motor starts from OFF.
#pragma once

#include <cstdint>

#include "control/long_pwm.h"

namespace hal {

class Blower {
public:
    // inverted: 4-wire open-drain drive. supply_en_pin: fan supply gate, -1 = not fitted.
    bool begin(int pin, uint32_t freq_hz, uint8_t res_bits, uint8_t min_start_pct, uint8_t min_run_pct, uint8_t max_pct,
               uint32_t kick_ms, uint32_t long_pwm_period_s, uint32_t long_pwm_min_on_s, bool inverted,
               int supply_en_pin);

    // Commanded duty in percent (0..100). Call once per control tick (long PWM is stepped here).
    // A non-zero applied duty enables the fan supply. An applied duty of 0 CUTS the supply (2026-10-22,
    // doc 05): the Delta stopped through its PWM lead restarts 0 to 4+ s late or not at all within a
    // long-PWM on-phase, while a cold start after a supply cut spun on the first tick 6 of 6 times.
    void setDutyPercent(uint8_t pct, float dt_s);

    // Emergency/unconditional off: cuts the fan supply and releases the PWM pin. Also called by fault paths.
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

    // True while the fan is powered and driven at a non-zero duty (supply on, PWM lead under control).
    bool armed() const { return armed_; }
    // Actual state of the SUPPLY_EN pin (always false when no supply pin is fitted).
    bool supplyOn() const { return supply_on_; }

    // Must be called from the control loop so the start kick can time out.
    void service(uint32_t now_ms);

private:
    void write(uint8_t pct);
    void writeCounts(uint32_t counts);
    void apply(uint8_t pct);
    void setSupply(bool on);

    int pin_ = -1;
    int supply_pin_ = -1;
    bool inverted_ = false;
    bool attached_ = false;    // LEDC attach succeeded; the supply is never enabled without it
    bool armed_ = false;
    bool supply_on_ = false;
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
