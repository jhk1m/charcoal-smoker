#include "console.h"

#include <Arduino.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "pins.h"

namespace ui {

void Console::begin(const ConsoleDeps& d) {
    d_ = d;
    Serial.println("# bench console ready, type ? for help");
}

void Console::simStep(float dt_s) {
    if (sim_ && sim_rate_c_per_s_ != 0.0f) sim_c_ += sim_rate_c_per_s_ * dt_s;
}

void Console::poll() {
    while (Serial.available() > 0) {
        const int ch = Serial.read();
        if (ch < 0) return;
        if (ch == '\r' || ch == '\n') {   // accept CR, LF or CRLF line endings from any terminal
            buf_[len_] = '\0';
            if (len_ > 0) {
                Serial.printf("# > %s\n", buf_);   // raw line as received, for diagnosing dropped characters
                handleLine(buf_);
            }
            len_ = 0;
            continue;
        }
        if (len_ < sizeof(buf_) - 1) buf_[len_++] = static_cast<char>(ch);
    }
}

void Console::help() const {
    Serial.println("# commands (values in C and percent)");
    Serial.println("#   ?            this help              d   dump settings and state");
    Serial.println("#   v            toggle the CSV stream   r   reset NVS to defaults and reboot");
    Serial.println("#   s <C>        setpoint                c   start cook   x  stop   k  clear fault");
    Serial.println("#   m <pct>      manual mode at duty     m   with no value exits to stop");
    Serial.println("#   g <kp> <ki> <kd>                     PID gains, saved to NVS");
    Serial.println("#   f <hz>       blower PWM frequency    n <pct>  blower min start duty");
    Serial.println("#   X <pct>      blower max duty         o <C>    probe offset (ice/boiling check)");
    Serial.println("#   l <0|1>      lid detection on/off    a <min>  cook timer, TIMER_DONE warning at zero; a 0 clears");
    Serial.println("#   p <n>        fan tach pulses per revolution, saved to NVS");
    Serial.println("#   T <0|1>      fan tach faults off/on. NOT saved: on again at every boot. T 0 = bench with no fan");
    Serial.println("#   i            sensor identity check: run once with the probe open, once with T+ shorted to T-");
    Serial.println("#   t <C>        simulate the probe at C (no sensor needed); t off  returns to the sensor");
    Serial.println("#   t <C> <C/s>  simulate with a ramp, e.g.  t 25 0.5");
}

void Console::dump() const {
    const storage::Settings& s = *d_.settings;
    Serial.printf("# hw=%s tc=%s pwm_pin=%d freq=%lu Hz min_start=%u%% max=%u%% offset=%.2f C\n",
                  pins::HW_NAME, hal::PitSensor::backendName(), pins::BLOWER_PWM,
                  static_cast<unsigned long>(d_.blower->frequency()), d_.blower->minStart(), d_.blower->maxPercent(),
                  static_cast<double>(s.probe_offset_c));
    Serial.printf("# setpoint=%.1f C gains kp=%.3f ki=%.4f kd=%.2f lid_detect=%d cook_active=%d\n",
                  static_cast<double>(s.setpoint_c), static_cast<double>(s.gains.kp), static_cast<double>(s.gains.ki),
                  static_cast<double>(s.gains.kd), s.lid_detect ? 1 : 0, s.cook_active ? 1 : 0);
    Serial.printf("# state=%s fault=%s warnings=0x%02x duty=%u%% applied=%u%% out_ema=%.1f sim=%d %.1f C limit=%.0f C timer=%lu s\n",
                  control::toString(d_.sm->state()), control::toString(d_.sm->fault()), d_.sm->warnings(),
                  d_.blower->commandedPercent(), d_.blower->appliedPercent(),
                  static_cast<double>(d_.sm->outputEma()), sim_ ? 1 : 0, static_cast<double>(sim_c_),
                  static_cast<double>(d_.sm->overLimitC()), static_cast<unsigned long>(d_.sm->timerS()));
    if (cfg::BLOWER_4WIRE) {
        Serial.printf("# fan=4-wire tach_pin=%d supply_en_pin=%d supply=%d ppr=%u rpm=%.0f tach_faults=%d\n",
                      pins::FAN_TACH, pins::FAN_SUPPLY_EN, d_.blower->supplyOn() ? 1 : 0, d_.fan->pulsesPerRev(),
                      static_cast<double>(d_.fan->rpm()), d_.fan->enabled() ? 1 : 0);
    } else {
        Serial.println("# fan=legacy 2-wire (no tach, no supply cut)");
    }
}

void Console::handleLine(char* line) {
    // tokenise
    char* argv[4] = {nullptr, nullptr, nullptr, nullptr};
    int argc = 0;
    for (char* tok = strtok(line, " \t"); tok && argc < 4; tok = strtok(nullptr, " \t")) argv[argc++] = tok;
    if (argc == 0) return;
    const char cmd = argv[0][0];
    const float v1 = argc > 1 ? strtof(argv[1], nullptr) : 0.0f;
    storage::Settings& st = *d_.settings;

    switch (cmd) {
        case '?': help(); return;
        case 'd': dump(); return;
        case 'v':
            csv_ = !csv_;
            Serial.printf("# csv %s\n", csv_ ? "on" : "off");
            return;
        case 'r':
            d_.store->reset();
            Serial.println("# NVS cleared, rebooting");
            Serial.flush();
            delay(100);
            ESP.restart();
            return;
        case 's': {
            const float c = std::clamp(v1, cfg::SETPOINT_MIN_C, cfg::SETPOINT_MAX_C);
            d_.sm->setSetpoint(c);
            st.setpoint_c = c;
            d_.store->save(st);
            Serial.printf("# setpoint %.1f C\n", static_cast<double>(c));
            return;
        }
        case 'a':
            d_.sm->setTimer(static_cast<uint32_t>(std::max(v1, 0.0f)) * 60u);
            Serial.printf("# timer %lu s\n", static_cast<unsigned long>(d_.sm->timerS()));
            return;
        case 'c':
            if (d_.sm->state() == control::State::FAULT) {
                Serial.printf("# refused: in FAULT (%s). Type k to clear it first.\n",
                              control::toString(d_.sm->fault()));
                return;
            }
            d_.sm->startCook(d_.sm->setpoint());
            d_.setCookActive(d_.sm->state() == control::State::IGNITE);
            Serial.printf("# cook started at %.1f C\n", static_cast<double>(d_.sm->setpoint()));
            return;
        case 'x':
            if (d_.sm->state() == control::State::FAULT) {
                Serial.printf("# refused: in FAULT (%s). Type k to clear it first.\n",
                              control::toString(d_.sm->fault()));
                return;
            }
            d_.sm->stop();
            d_.setCookActive(false);
            Serial.printf("# stopped, state=%s\n", control::toString(d_.sm->state()));
            return;
        case 'k':
            d_.sm->clearFault();
            Serial.printf("# fault cleared, state=%s\n", control::toString(d_.sm->state()));
            return;
        case 'm': {
            if (argc < 2) {
                d_.sm->stop();
                Serial.println("# manual off");
                return;
            }
            if (d_.sm->state() == control::State::FAULT) {
                Serial.printf("# refused: in FAULT (%s). Type k to clear it first.\n",
                              control::toString(d_.sm->fault()));
                return;
            }
            const uint8_t duty = static_cast<uint8_t>(std::clamp(v1, 0.0f, 100.0f));
            if (d_.sm->state() != control::State::MANUAL) d_.sm->enterManual(duty);
            else d_.sm->setManualDuty(duty);
            d_.setCookActive(false);
            Serial.printf("# manual duty %u%%\n", duty);
            return;
        }
        case 'g': {
            if (argc < 4) { Serial.println("# usage: g <kp> <ki> <kd>"); return; }
            st.gains = {strtof(argv[1], nullptr), strtof(argv[2], nullptr), strtof(argv[3], nullptr)};
            d_.sm->setGains(st.gains);
            d_.store->save(st);
            Serial.printf("# gains kp=%.3f ki=%.4f kd=%.2f\n", static_cast<double>(st.gains.kp),
                          static_cast<double>(st.gains.ki), static_cast<double>(st.gains.kd));
            return;
        }
        case 'f': {
            // The 4-wire fan's PWM input is specified for 21 to 28 kHz only.
            const float lo = cfg::BLOWER_4WIRE ? static_cast<float>(cfg::BLOWER_4WIRE_FREQ_MIN_HZ) : 100.0f;
            const float hi = cfg::BLOWER_4WIRE ? static_cast<float>(cfg::BLOWER_4WIRE_FREQ_MAX_HZ) : 40000.0f;
            const uint32_t hz = static_cast<uint32_t>(std::clamp(v1, lo, hi));
            if (!d_.blower->setFrequency(hz)) { Serial.println("# setFrequency failed"); return; }
            st.blower_pwm_freq_hz = d_.blower->frequency();
            d_.store->save(st);
            Serial.printf("# pwm %lu Hz\n", static_cast<unsigned long>(st.blower_pwm_freq_hz));
            return;
        }
        case 'n': {
            const uint8_t pct = static_cast<uint8_t>(std::clamp(v1, 1.0f, 99.0f));
            d_.blower->setMinStart(pct);
            st.blower_min_start_pct = pct;
            d_.store->save(st);
            Serial.printf("# min start %u%%\n", pct);
            return;
        }
        case 'X': {
            const uint8_t pct = static_cast<uint8_t>(std::clamp(v1, 10.0f, 100.0f));
            st.blower_max_pct = pct;
            d_.blower->setMaxPercent(pct);
            d_.sm->setThresholds(d_.makeThresholds(st));
            d_.store->save(st);
            Serial.printf("# max duty %u%%\n", pct);
            return;
        }
        case 'o': {
            const float off = std::clamp(v1, -20.0f, 20.0f);
            d_.pit->setOffset(off);
            st.probe_offset_c = off;
            d_.store->save(st);
            Serial.printf("# probe offset %.2f C\n", static_cast<double>(off));
            return;
        }
        case 'l': {
            const bool en = v1 != 0.0f;
            d_.sm->setLidDetectEnabled(en);
            st.lid_detect = en;
            d_.store->save(st);
            Serial.printf("# lid detect %s\n", en ? "on" : "off");
            return;
        }
        case 'p': {
            if (argc < 2) { Serial.println("# usage: p <pulses per revolution, 1..8>"); return; }
            const uint8_t ppr = static_cast<uint8_t>(std::clamp(v1, 1.0f, 8.0f));
            d_.fan->setPulsesPerRev(ppr);
            st.fan_tach_ppr = ppr;
            d_.store->save(st);
            Serial.printf("# fan tach %u pulses per revolution\n", ppr);
            return;
        }
        case 'T': {
            if (!cfg::BLOWER_4WIRE) { Serial.println("# legacy 2-wire build: no tach"); return; }
            if (argc < 2) { Serial.println("# usage: T <0|1>"); return; }
            const bool en = v1 != 0.0f;
            d_.fan->setEnabled(en);
            Serial.printf("# fan tach faults %s%s\n", en ? "on" : "OFF",
                          en ? "" : " until reboot: BLOWER_STALL and BLOWER_RUNAWAY are not checked");
            return;
        }
        case 'i': {
            // Doc 09 section 2a, steps 3 to 6. Judgement: open input must raise the open-circuit bit; shorted input
            // must read the cold junction within 2 C (the test both MAX31855s failed at 23 C low).
            const hal::PitSensor::SelfTest t = d_.pit->selfTest();
            Serial.printf("# %s on CS GPIO%d: %s\n", hal::PitSensor::backendName(), pins::TC_CS,
                          t.present ? "present" : "NOT DETECTED (check VCC, CS, CLK, DO/SDO wiring)");
            if (!t.present) return;
            if (t.has_register_test)
                Serial.printf("# register readback (type J then K): %s\n",
                              t.register_ok ? "PASS, writable CR1, real MAX31856"
                                            : "FAIL, type did not read back: relabeled or dead chip");
            const bool open = (t.fault_bits & 0x01u) != 0;  // bit0 is open-circuit on both backends
            const float delta = t.tc_c - t.cj_c;
            Serial.printf("# tc=%.2f C cj=%.2f C delta=%.2f C fault_bits=0x%02x%s\n", static_cast<double>(t.tc_c),
                          static_cast<double>(t.cj_c), static_cast<double>(delta), t.fault_bits,
                          open ? " OPEN" : "");
            if (open) {
                Serial.println("# input open: correct if nothing is on T+/T-. Now short T+ to T- and run i again.");
            } else if (delta > -2.0f && delta < 2.0f) {
                Serial.println("# shorted-input PASS: reads the cold junction within 2 C");
            } else {
                Serial.printf("# shorted-input FAIL: %.1f C off the cold junction. If T+/T- really are shorted, "
                              "reject the board\n", static_cast<double>(delta));
            }
            return;
        }
        case 't': {
            if (argc > 1 && strcmp(argv[1], "off") == 0) {
                sim_ = false;
                sim_rate_c_per_s_ = 0.0f;
                Serial.println("# sim probe off, using the sensor");
                return;
            }
            if (argc < 2) { Serial.println("# usage: t <C> [C/s] | t off"); return; }
            sim_ = true;
            sim_c_ = v1;
            sim_rate_c_per_s_ = argc > 2 ? strtof(argv[2], nullptr) : 0.0f;
            Serial.printf("# sim probe %.1f C, ramp %.3f C/s\n", static_cast<double>(sim_c_),
                          static_cast<double>(sim_rate_c_per_s_));
            return;
        }
        default:
            Serial.printf("# unknown command '%c', type ? for help\n", cmd);
            return;
    }
}

}  // namespace ui
