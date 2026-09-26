# charcoal-smoker firmware (Phase 1)

Temperature-controlled charcoal smoker controller. ESP32-S3 DevKitC-1 clone (N8R8) is the bench board as of 2026-09-23; WROOM-32 and Freenove WROVER maps remain as fallbacks. Blower: Delta BFB0712HH-AWPH 12 V 4-wire (decided 2026-09-24, not yet fitted); the code currently still drives the old 2-wire 6 V blower. K-type pit probe on a MAX31855 or MAX31856, BLE GATT server for control and telemetry, serial bench console.

Start at `..\HANDOFF.md` for project state, decided hardware, the build and flash commands, the console command set, and what is blocked. Design docs live in the Claude project (`claude/00-handoff` to `claude/09-open-items-and-gaps`) and are mirrored in `..\output\`. This README covers the code only.

Bench env to use: `s3-n8r8-uart-max31855` (Serial on the CH343 UART bridge, VID 1A86). The `s3-n8r8*` envs route Serial to the native USB port (VID 303A) instead.

## Toolchain (VERIFIED 2026-09-20)

| Component | Version | Source |
|---|---|---|
| PlatformIO Core | 6.2.0 | pip `platformio` |
| Platform | pioarduino `platform-espressif32` stable (resolved 55.03.312) | https://github.com/pioarduino/platform-espressif32 |
| Arduino-ESP32 core | 3.3.12 (ESP-IDF 5.5.x) | bundled by the platform |
| Board (S3 envs) | `esp32-s3-devkitc1-n8r8` (qio_opi, BOARD_HAS_PSRAM) | `boards/esp32-s3-devkitc1-n8r8.json` in that repo |
| Board (wroom32 envs) | `esp32dev` (classic ESP32, 4 MB flash) | pioarduino boards |
| NimBLE-Arduino | 2.5.1 | https://github.com/h2zero/NimBLE-Arduino |
| Adafruit MAX31856 | 1.2.8 | https://github.com/adafruit/Adafruit_MAX31856 |
| Adafruit BusIO | 1.17.4 | https://github.com/adafruit/Adafruit_BusIO |

The official `platformio/espressif32` platform (7.1.0) still ships Arduino core 2.0.17 and does not build this project (LEDC and TWDT calls use the 3.x / IDF 5 API). Use the pioarduino platform URL in `platformio.ini` as-is.

## Install on Windows 11 (JASON-PC)

Assumptions: Windows 11 Pro, PowerShell, Python 3.10 to 3.14 on PATH, Git installed.

Option A, VS Code: install the "pioarduino ide" extension from the VS Code marketplace (pioarduino README), then File > Open Folder on this directory.

Option B, CLI:

```powershell
python -m pip install --user platformio
```

Then from this directory:

```powershell
pio run -e s3-n8r8-uart-max31855 -t upload --upload-port COM6   # bench env; COM number from pio device list
pio device monitor -e s3-n8r8-uart-max31855 --port COM6
pio test -e native            # needs a host gcc/g++ on PATH; not installed on JASON-PC as of 2026-09-25
```

Local build result 2026-09-25 (JASON-PC): `s3-n8r8-uart-max31855` SUCCESS, RAM 11.7 percent (38360 B), Flash 19.4 percent (647610 B). Native tests: 29/29 in the clean container; blocked locally by the missing host compiler (doc 09 B4).

Build environments (`platformio.ini`), all VERIFIED to compile 2026-09-24 in a clean container; `s3-n8r8-uart` added and built on JASON-PC 2026-09-26. `default_envs = s3-n8r8-uart`.

| env | board | pit sensor | when |
|---|---|---|---|
| `s3-n8r8-uart` (default) | ESP32-S3-DevKitC-1 N8R8, Serial on the CH343 UART bridge | MAX31856 | BENCH ENV from 2026-09-26; MAX31856 due 2026-09-29 to 10-02 |
| `s3-n8r8-uart-max31855` | same | MAX31855 | validated 2026-09-25 with the simulated probe; both MAX31855 boards later failed the shorted-input test and were returned (doc 03), so this is a fallback |
| `freenove-wrover-max31855` | Freenove ESP32-WROVER camera board | MAX31855, hardware SPI read-only | fallback (pins 14/33/13/32, PWM 2) |
| `freenove-wrover` | Freenove ESP32-WROVER | MAX31856 | same board, MAX31856 |
| `wroom32-max31855` | ESP32-WROOM-32 (no camera) | MAX31855 | plain WROOM boards |
| `wroom32` | ESP32-WROOM-32 | MAX31856 (Adafruit lib) | when the MAX31856 boards land |
| `s3-n8r8` | ESP32-S3-DevKitC-1 N8R8 | MAX31856 | S3 boards |
| `s3-n8r8-max31855` | ESP32-S3-DevKitC-1 N8R8 | MAX31855 | S3 boards, MAX31855 |

The sensor backend is chosen with `-DPIT_SENSOR_MAX31855`; the pin map is chosen by the IDF target (`include/pins.h`). Pin map per board is in `claude/04-phase1-wiring.md`.

First `pio run` downloads the toolchain (several hundred MB). USB-UART driver is only needed if the board's COM port does not appear (BOM note).

## Layout

```
platformio.ini            envs: s3-n8r8 (target), native (unit tests)
include/pins.h            GPIO map, mirrors claude/04-phase1-wiring.md
include/config.h          compile-time constants, OPEN items flagged, BLE UUIDs
src/main.cpp              setup(), 1 Hz control task, BLE->control command queue
src/control/              pure C++ (no Arduino): median_filter.h, pid.*, state_machine.*, long_pwm.h
src/ui/console.*          serial bench console (runs inside the control task)
src/hal/                  blower.* (LEDC), pit_sensor.* (MAX31856), watchdog.* (TWDT)
src/ble/ble_service.*     NimBLE GATT server, passkey bonding, pairing window
src/storage/settings.*    NVS via Preferences (setpoint, PID gains, blower params)
src/log/event_log.h       12-byte packed event ring buffer (RAM; flash is Phase 2)
test/test_control/        Unity tests for PID + state machine, run on host
```

## Runtime model

One FreeRTOS task (`control`, core 1, priority 3) runs at 1 Hz and is the only writer to the blower. Each tick: drain BLE command queue, read MAX31856 (continuous conversion, median-of-5), step the state machine, actuate, publish BLE notifications, print one CSV line on Serial. The task feeds the task watchdog (5 s, panic on timeout). A watchdog reset leaves the ESP32 in reset with the gate pull-down holding the blower off; the next boot logs `WATCHDOG_RESET`.

States: `IDLE, IGNITE, RAMP, HOLD, LID_OPEN, FAULT, MANUAL, RECOVERY`. `MANUAL` is the Phase 1 bring-up mode (fixed duty over BLE, no PID). See `control/state_machine.h`.

Control behaviour (revised 2026-09-21 per design doc 06 section 7, patterns taken from HeaterMeter source):
- IGNITE runs a fixed 50 percent duty until the pit is within 17 C of the setpoint. There is no ignition on charcoal; the user lights a small fire first.
- RAMP and RECOVERY cap the duty and the PID integral at `STARTUP_MAX_DUTY_PCT` (50) until the pit first crosses the setpoint; at that crossing the integral is halved. HOLD has the full `BLOWER_MAX_DUTY_PCT` (60 default, NVS-tunable).
- PID: derivative on the measurement against a 60 s EMA, conditional integration anti-windup, integral clamped to the current cap.
- LID_OPEN triggers on a fall rate of 0.3 C/s over 10 s at cook setpoints (scaled up to 1.2 C/s at 260 C), only after the setpoint has been reached and only while the output EMA is under 90 percent of max (a pegged blower with a falling pit is fuel running out, not a lid). Blower goes to 0. Exit after at least 30 s when the pit is back at the setpoint, or has climbed 11 C off its low and plateaued, or after 240 s; exit goes to RECOVERY. A drum-specific rising variant triggers when the pit climbs 0.3 C/s for 20 s with the blower at or under 10 percent (opening a drum lid raises the pit).
- Warnings characteristic (bitmask, never touches the blower): PIT_HIGH / PIT_LOW (+/- 14 C with 1 C hysteresis, armed after the setpoint is reached, muted in LID_OPEN), FUEL_LOW (output EMA >= 90 percent and pit fell 5 C over 10 min), CREEP (pit 14 C over setpoint with the blower off for 15 min: draft leak or too much lit fuel), LID_RISING.
- FIRE_OUT fault: pit 40 C or more below setpoint with the blower pegged for 15 min. Blower off; there is nothing to reignite.
- Blower duties below the min-start duty are produced by long-period PWM (10 s period at min-start) instead of being clamped up; a 300 ms kick still breaks stiction on every start from OFF.
- Setpoint change in a closed-loop state re-enters RAMP with the startup cap (integral kept, clamped).
- Power loss: `cook_active` is persisted in NVS when a cook starts and cleared on stop, manual, or fault. A boot with it set restarts the cook through IGNITE (which exits at once if the pit is still hot) and logs `POWER_RESUME`.

## Bench console (serial, 115200)

Type into the serial monitor while the firmware runs. It exists so the bring-up measurements can be made
without a reflash, and so the state machine can be exercised with nothing wired to the board.

| Command | Effect |
|---|---|
| `?` / `d` | help / dump settings and current state |
| `v` | mute or unmute the 1 Hz CSV stream (mute it while typing) |
| `s <C>` | setpoint, clamped to SETPOINT_MIN_C..MAX_C, saved to NVS |
| `c` / `x` / `k` | start cook / stop / clear fault |
| `m <pct>` | manual mode at a fixed duty; `m` alone stops |
| `g <kp> <ki> <kd>` | PID gains, saved to NVS |
| `f <hz>` | blower PWM frequency, saved to NVS |
| `n <pct>` | blower minimum start duty (below it the long-PWM path runs), saved to NVS |
| `X <pct>` | blower maximum duty, rebuilds the state machine thresholds, saved to NVS |
| `o <C>` | pit probe offset from the ice / boiling check, saved to NVS |
| `l <0/1>` | lid detection on or off |
| `i` | sensor identity check (doc 09 section 2a): re-probes the chip, on the MAX31856 writes type J, reads it back, restores K; prints tc, cold junction, delta and fault bits. Run once with the probe open (expect OPEN), once with T+ shorted to T- (expect delta within 2 C) |
| `t <C> [C/s]` | simulate the probe at C, optionally ramping; `t off` returns to the real sensor |

`t` is the no-hardware path: flash the board, type `t 25 0.5`, then `c`, and watch IGNITE, RAMP, HOLD,
LID_OPEN and the warnings run against a synthetic pit with the blower duty printed each second.

## BLE interface

Device name `SMOKER-CTRL`. Service `7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0001`. Little-endian.

| Char (last group) | Type | Access | Meaning |
|---|---|---|---|
| `0002` | float32 | read, notify | pit temperature C (filtered) |
| `0003` | float32 | read, write(enc+authen), notify | setpoint C, clamped 60..180 |
| `0004` | uint8 | read, notify | commanded blower duty percent |
| `0005` | uint8 | read, notify | state enum (see state_machine.h) |
| `0006` | uint8 | read, notify | fault enum |
| `0007` | uint8 | write(enc+authen) | 0 start auto cook, 1 manual, 2 stop, 3 clear fault |
| `0008` | uint8 | write(enc+authen) | manual duty percent (only in MANUAL) |
| `0009` | 12 bytes | read(enc) | pops one event record per read; zero-length when empty |
| `000a` | utf8 | read | device info string |
| `000b` | uint8 | read, notify | warnings bitmask: bit0 PIT_HIGH, bit1 PIT_LOW, bit2 FUEL_LOW, bit3 CREEP, bit4 LID_RISING |

Security: bonding + MITM + LE Secure Connections, static passkey `cfg::BLE_PASSKEY` (change before enclosure build). New bonds are accepted only during the pairing window (60 s after boot in Phase 1; button-triggered later). Bonds created outside the window are deleted and the link dropped.

Serial CSV header: `t_s,state,fault,pit_raw_c,pit_filt_c,cj_c,duty_cmd,duty_applied,tc_fault_bits,warnings,out_ema,rate_c_s`. Capture this during the step-response cook to derive PID gains.

## OPEN items carried in code (config.h)

- `BLOWER_PWM_FREQ_HZ` 25 kHz vs 200 Hz: bench test (arch doc section 4). `hal::Blower::setFrequency()` exists for live switching.
- `BLOWER_MIN_START_DUTY_PCT` placeholder 30: measure.
- `PID_K*_DEFAULT`: placeholders until step-response data exists.
- `BLE_PASSKEY`: bench value.
- `PIT_SOFT_LIMIT_C` 200: must stay below the thermostat dial setting.

## Not in Phase 1

Meat probes, flash-persisted log, OTA, pairing button, display, current sense, Wi-Fi.
