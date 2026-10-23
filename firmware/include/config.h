// Compile-time configuration. Runtime-tunable values live in storage/settings.h (NVS) and use these as defaults.
// OPEN items from 02-phase1-architecture.md are marked; fill in after the bench measurements.
#pragma once

#include <cstdint>

namespace cfg {

// ---- Control loop timing ----
constexpr uint32_t CONTROL_PERIOD_MS = 1000;  // 1 Hz sample + duty update
constexpr uint32_t WATCHDOG_TIMEOUT_MS = 5000;  // control task must feed TWDT within this

// ---- Blower type ----
// Delta BFB0712HH-AWPH, 4-wire (doc 04 "Blower loop"): the PWM lead is driven open-drain through a 2N7000, so
// the GPIO duty is INVERTED; FAN_SUPPLY_EN gates the fan supply; the tach is supervised (doc 09 section 6a).
// Build with -DBLOWER_LEGACY_2WIRE for the old 6 V brushed blower on a low-side MOSFET (no tach, no supply cut).
#if defined(BLOWER_LEGACY_2WIRE)
constexpr bool BLOWER_4WIRE = false;
#else
constexpr bool BLOWER_4WIRE = true;
#endif

// ---- Blower PWM ----
// 4-wire: 25 kHz, inside the Intel 4-wire range of 21 to 28 kHz; the console and NVS are held to that range.
// Legacy 2-wire: 25 kHz vs 200 Hz was a bench question (arch doc section 4).
constexpr uint32_t BLOWER_PWM_FREQ_HZ = 25000;
constexpr uint32_t BLOWER_4WIRE_FREQ_MIN_HZ = 21000;
constexpr uint32_t BLOWER_4WIRE_FREQ_MAX_HZ = 28000;
constexpr uint8_t  BLOWER_PWM_RES_BITS = 10;  // 0..1023 duty counts
constexpr uint32_t BLOWER_PWM_MAX_COUNT = (1u << BLOWER_PWM_RES_BITS) - 1;
// Minimum duty (percent) at which the blower reliably spins from rest. NVS default; the bench value for the
// Delta is 15 (`n 15`, measured 2026-10-10: starts at 10 with the kick, 4 of 4). Commands below the active
// threshold are emulated with long-period PWM (doc 06 P6, HeaterMeter) instead of being clamped up.
constexpr uint8_t  BLOWER_MIN_START_DUTY_PCT = 30;
// Minimum duty at which an already spinning fan keeps running (Delta: runs at 10, dies at 5; doc 09 M5).
// Hysteresis for the long-PWM threshold: a spinning fan is not stopped for a request between here and
// BLOWER_MIN_START_DUTY_PCT. Must be at or below the min-start duty; LongPwm clamps it there.
constexpr uint8_t  BLOWER_MIN_RUN_DUTY_PCT = 10;
constexpr uint32_t BLOWER_LONG_PWM_PERIOD_S = 10;
// Minimum long-PWM on-time. MEASURED 2026-10-22 on the Delta (doc 05): after a stop through the PWM lead
// (supply on, 0 percent) the fan restarts on its own 0 to 3 s after the next non-zero duty, kick or not, so a
// one-tick on-phase often sees no tach pulse at all and the stall counter adds up across the gaps (false
// BLOWER_STALL at setpoint). 5 s = 3 s restart + 1 s tach lag + 1 s margin. The period stretches above
// BLOWER_LONG_PWM_PERIOD_S when needed so the average duty is still requested/min_start.
constexpr uint32_t BLOWER_LONG_PWM_MIN_ON_S = 5;
// Software clamp. Drum smokers need a whisper of air: 20 to 25 CFM fans at full duty cool the pit and blow ash
// (doc 06 section 1 item 4, section 1b item 3). Tunable over BLE / NVS.
constexpr uint8_t  BLOWER_MAX_DUTY_PCT = 60;
// Cap for RAMP and RECOVERY until the setpoint is first reached (HeaterMeter startup max; PBC fan users cap at 50).
constexpr uint8_t  STARTUP_MAX_DUTY_PCT = 50;
constexpr uint32_t BLOWER_KICK_MS = 300;       // 100 percent kick when starting from OFF
constexpr uint32_t BLOWER_SERVICE_MS = 10;     // how often the control task ends an expired kick between ticks

// ---- Fan tach supervision (4-wire only, doc 09 section 6a) ----
// OPEN: pulses per revolution. The datasheet FG waveform reads as 2; count it on the bench (console: p <n>).
constexpr uint8_t  FAN_TACH_PULSES_PER_REV = 2;
constexpr uint32_t FAN_TACH_GLITCH_NS = 10000;  // PCNT glitch filter; a tach half-period at 4400 RPM is 3.4 ms
constexpr uint16_t FAN_RATED_RPM = 4400;        // datasheet, +/- 10 percent
// BLOWER_STALL: the fan is driven and the tach is silent for this long (driven seconds; long-PWM off time
// does not count). Also what an open thermostat contact or a dead supply looks like.
constexpr uint32_t FAN_STALL_S = 5;  // was 3; the Delta restarts up to 3 s late after a PWM-lead stop (2026-10-22, doc 05)
// BLOWER_RUNAWAY: measured RPM above (commanded share of rated + this margin of rated) for this long, which
// is what a broken PWM lead looks like on this fan (it runs at full speed). Action: cut FAN_SUPPLY_EN.
// OPEN: the margin assumes speed roughly linear in duty; check against the real RPM-vs-duty curve.
constexpr uint8_t  FAN_RUNAWAY_MARGIN_PCT = 25;
constexpr uint32_t FAN_RUNAWAY_S = 5;

// ---- Temperature limits (Celsius) ----
constexpr float SETPOINT_MIN_C = 60.0f;
constexpr float SETPOINT_MAX_C = 200.0f;   // short cooks (doc 06 section 6a); 400 F+ is sear, out of scope
constexpr float SETPOINT_DEFAULT_C = 110.0f;
// Software over-temp alarm (PIT_OVER_LIMIT): pit >= min(setpoint + PIT_OVER_MARGIN_C, PIT_SOFT_LIMIT_C).
// PIT_SOFT_LIMIT_C is the absolute cap and must stay BELOW the thermostat dial (270 C) so software acts first.
constexpr float PIT_OVER_MARGIN_C = 40.0f;
constexpr float PIT_SOFT_LIMIT_C = 240.0f;
constexpr float PIT_SENSOR_MIN_VALID_C = -20.0f;
constexpr float PIT_SENSOR_MAX_VALID_C = 400.0f;

// ---- State machine thresholds (doc 06 section 7) ----
constexpr float    IGNITE_EXIT_MARGIN_C = 17.0f;      // IGNITE -> RAMP when pit >= setpoint - margin (BBQ Guru: 30 F)
constexpr uint8_t  IGNITE_DUTY_PCT = 50;              // was 90; small lit fire, let the controller bring it up
constexpr uint32_t IGNITE_TIMEOUT_S = 40 * 60;        // no rise in this window -> FAULT
constexpr float    IGNITE_MIN_RISE_C = 10.0f;         // rise required within IGNITE_TIMEOUT_S
constexpr float    HOLD_BAND_C = 5.0f;                // RAMP/RECOVERY -> HOLD when |err| <= band for HOLD_SETTLE_S
constexpr uint32_t HOLD_SETTLE_S = 60;
constexpr uint32_t RECOVERY_MAX_S = 10 * 60;          // RECOVERY -> HOLD at the latest
// Lid detection, rate style (HeaterMeter LidTrack: -0.5 F/s at 225 F = -0.28 C/s; x4 at 260 C).
constexpr float    LID_OPEN_DROP_C_PER_S = 0.3f;      // at setpoints <= 121 C; scaled up to 1.2 C/s at 260 C
constexpr float    LID_OPEN_TOTAL_DROP_C = 0.0f;      // classic "pit N C below setpoint" trigger; 0 = disabled (LidTrack advice)
constexpr uint32_t LID_RATE_WINDOW_S = 10;
constexpr uint32_t LID_OPEN_MIN_S = 30;               // HeaterMeter LIDOPEN_MIN_AUTORESUME
constexpr uint32_t LID_OPEN_TIMEOUT_S = 4 * 60;       // HeaterMeter default lid duration 240 s
constexpr uint8_t  LID_OPEN_DUTY_PCT = 0;             // blower floor while lid is open
constexpr float    LID_OPEN_RECOVER_BAND_C = 0.0f;    // exit LID_OPEN when pit >= setpoint - band (HeaterMeter: at setpoint; a band exits mid-climb and overshoots)
constexpr float    LID_EXIT_RISE_ABOVE_LOW_C = 11.0f; // LidTrack: 20 F above the low peak ...
constexpr float    LID_EXIT_PLATEAU_C_PER_S = 0.06f;  // ... and 0 < rate < 0.1 F/s
constexpr uint8_t  LID_GUARD_OUT_EMA_PCT = 90;        // no lid mode when output EMA (of max duty) >= this: probable fuel out
constexpr float    LID_RISING_RATE_C_PER_S = 0.3f;    // drum: opening the lid RAISES the pit (PBC: +55 C in 3 to 4 min)
constexpr uint32_t LID_RISING_WINDOW_S = 20;
constexpr uint8_t  LID_RISING_DUTY_PCT = 10;          // rising check only while commanded duty <= this
// Averages (HeaterMeter grillpid_conf.h)
constexpr float    OUT_EMA_ALPHA = 2.0f / 241.0f;
constexpr float    PROBE_EMA_ALPHA = 2.0f / 61.0f;
// Warnings and FIRE_OUT (doc 06 E8, E25, P5)
constexpr float    FUEL_LOW_FALL_C = 5.0f;
constexpr uint32_t FUEL_LOW_WINDOW_S = 600;
constexpr uint8_t  FUEL_LOW_OUT_PCT = 90;
constexpr uint32_t FUEL_LOW_CLEAR_S = 300;           // latched: clears only after 5 min without the condition
constexpr float    FIRE_OUT_BELOW_SP_C = 40.0f;
constexpr uint32_t FIRE_OUT_S = 15 * 60;
constexpr float    PIT_ALARM_DEV_C = 14.0f;           // UltraQ / Signals: +/- 25 F
constexpr float    PIT_ALARM_HYST_C = 1.0f;
constexpr float    CREEP_ABOVE_SP_C = 14.0f;
constexpr uint32_t CREEP_S = 15 * 60;

// ---- PID defaults (OPEN: derive from step-response cook, arch doc section 6) ----
// HeaterMeter defaults are P 4 / I 0.02 / D 5 per F (7.2 / 0.036 / 9 per C). Ours are softer, which is the
// right side to err on for a thin drum. D is per C of deviation from the 60 s measurement average.
constexpr float PID_KP_DEFAULT = 4.0f;   // percent duty per degree C
constexpr float PID_KI_DEFAULT = 0.02f;  // percent duty per (degree C * s)
constexpr float PID_KD_DEFAULT = 0.0f;   // percent duty per degree C of (T - EMA(T))

// ---- Sensor filtering ----
constexpr uint8_t MEDIAN_WINDOW = 5;

// ---- Event log ----
constexpr uint16_t EVENT_LOG_CAPACITY = 256;  // RAM ring buffer; flash persistence is Phase 2

// ---- BLE ----
constexpr const char* BLE_DEVICE_NAME = "SMOKER-CTRL";
// Custom 128-bit UUIDs. Base 7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0001, incrementing the last group.
constexpr const char* UUID_SERVICE     = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0001";
constexpr const char* UUID_PIT_TEMP    = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0002";  // float32 C, notify
constexpr const char* UUID_SETPOINT    = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0003";  // float32 C, read/write (encrypted)
constexpr const char* UUID_BLOWER_DUTY = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0004";  // uint8 percent, read/notify
constexpr const char* UUID_STATE       = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0005";  // uint8 enum, notify
constexpr const char* UUID_FAULT       = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0006";  // uint8 enum, notify
constexpr const char* UUID_MODE        = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0007";  // uint8: 0 auto, 1 manual (write enc)
constexpr const char* UUID_MANUAL_DUTY = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0008";  // uint8 percent (write enc)
constexpr const char* UUID_EVENT_LOG   = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0009";  // read, one record per read (chunked)
constexpr const char* UUID_DEVICE_INFO = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a000a";  // utf8 string, read
constexpr const char* UUID_WARNINGS    = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a000b";  // uint8 bitmask (control::Warning), read/notify
constexpr const char* UUID_TIMER       = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a000c";  // uint16 minutes, read/write enc; 0 clears. TIMER_DONE arrives on WARNINGS
constexpr const char* UUID_CONSOLE     = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a000d";  // utf8 console line, write enc+authen; replies notified on the same char (2026-10-22)
constexpr const char* UUID_TELEMETRY   = "7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a000e";  // utf8 CSV line, notify at 1 Hz, same columns as the serial stream
// Static passkey. Comes from firmware/.env (git-ignored) through load_env.py as -DSMOKER_BLE_PASSKEY=<6 digits>;
// host builds (tests, sim) do not pair and get 0. The value committed before 2026-10-23 is retired.
#if defined(ARDUINO) && !defined(SMOKER_BLE_PASSKEY)
#error "SMOKER_BLE_PASSKEY not defined: copy firmware/.env.example to firmware/.env and set it"
#endif
#ifndef SMOKER_BLE_PASSKEY
#define SMOKER_BLE_PASSKEY 0
#endif
constexpr uint32_t BLE_PASSKEY = SMOKER_BLE_PASSKEY;
constexpr uint32_t BLE_PAIRING_WINDOW_MS = 120 * 1000;  // 60 s was too short for a phone PIN prompt (2026-10-23)

}  // namespace cfg
