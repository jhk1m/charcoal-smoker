# Phase 1 - Firmware Framework (DECIDED)

Created: 2026-09-20
Updated: 2026-09-25 (first hardware bring-up recorded; local build results and the native-test blocker added at the end)
Status: control core validated on the ESP32-S3 with a simulated probe. Nothing wired. Native tests last passed 29/29 in the container; they cannot run on JASON-PC until a host g++ is installed.
Location: C:\Users\wl_de\projects\charcoal-smoker\firmware\ (README.md there covers layout, install, BLE table)

## Decision
DECIDED: PlatformIO + Arduino-ESP32 3.x via the pioarduino platform. Closes the "framework choice" OPEN item in 02-phase1-architecture.md section 8 and 11.
Rejected: ESP-IDF native (no Adafruit MAX31856 driver, slower bring-up); Arduino IDE 2.x (no multi-file project structure, no native tests); official platformio/espressif32 7.1.0 (ships Arduino 2.0.17 / IDF 4.4.7, no 3.x support, incompatible LEDC and TWDT APIs).

## Verified toolchain (2026-09-20, clean container build)
| Component | Version | Verified at |
|---|---|---|
| PlatformIO Core | 6.2.0 | pip |
| Platform | pioarduino platform-espressif32 stable, resolved 55.03.312 | github.com/pioarduino/platform-espressif32 README |
| Arduino-ESP32 | 3.3.12 (IDF 5.5.x) | bundled; pioarduino README text said 3.3.11 at time of check |
| Board id | esp32-s3-devkitc1-n8r8 (memory_type qio_opi, psram opi, -DBOARD_HAS_PSRAM, default_8MB.csv) | boards/esp32-s3-devkitc1-n8r8.json in that repo |
| NimBLE-Arduino | 2.5.1 (newest 2.x tag; master is 3.0.0-dev, do not use) | github.com/h2zero/NimBLE-Arduino tag 2.5.1 headers |
| Adafruit MAX31856 | 1.2.8 | github.com/adafruit/Adafruit_MAX31856 |
| Adafruit BusIO | 1.17.4 | resolved by build |
| VS Code extension | pioarduino IDE, id pioarduino.pioarduino-ide | marketplace.visualstudio.com |
| Windows install | python -m pip install --user platformio, Python 3.10 to 3.14 | pioarduino README |

APIs verified against primary sources before use: ledcAttach/ledcWrite/ledcChangeFrequency (Arduino-ESP32 3.x LEDC docs), esp_task_wdt_init/reconfigure/add/reset with esp_task_wdt_config_t (ESP-IDF 5.5 WDT docs), Preferences begin/putFloat/getFloat/putUChar/putUInt with 15-char key limit (Arduino-ESP32 Preferences tutorial), NimBLE 2.x server/characteristic/security callback signatures (headers at tag 2.5.1), Adafruit_MAX31856 constructor/begin/setThermocoupleType/setConversionMode/readFault/readThermocoupleTemperature (header at 1.2.8).

## Build results (2026-09-20, Phase 1 scaffold)
- pio run -e s3-n8r8: SUCCESS. RAM 11.7 percent (38424 B), Flash 18.8 percent (627526 B of 3342336 B app partition).
- pio test -e native: 12/12 Unity tests pass (median filter, PID clamp/anti-windup/freeze, state machine IDLE/IGNITE/RAMP/HOLD/LID_OPEN/FAULT/MANUAL, ignite timeout, sensor fault, over-limit).
- Two bugs found and fixed by the tests before first flash: IGNITE no-rise baseline used a stale pre-cook temperature; LID_OPEN entry tick still emitted the PID duty instead of the lid-open floor.

## Architecture as implemented
- Single 1 Hz FreeRTOS control task (core 1, prio 3) is the only blower writer. BLE callbacks post to a FreeRTOS queue; the task drains it each tick.
- Control core (src/control/) is pure C++ with no Arduino dependency, so it runs on the host under the native env.
- Task watchdog 5 s, panic on timeout. Boot logs WATCHDOG_RESET fault if the previous reset was TWDT.
- Blower: LEDC 25 kHz / 10-bit, 300 ms 100 percent kick from OFF, live setFrequency() for the 25 kHz vs 200 Hz bench test. Below the min-start duty: long-period PWM (Phase 1.5).
- Sensor: MAX31856 continuous conversion, K-type, 60 Hz filter, median-of-5, fault bits mapped to SENSOR_OPEN / SENSOR_SHORT / SENSOR_RANGE.
- States: IDLE, IGNITE, RAMP, HOLD, LID_OPEN, FAULT, MANUAL (bring-up step 5), RECOVERY (Phase 1.5).
- BLE: one service, 10 characteristics (see firmware README). Bonding + MITM + SC with static passkey; new bonds only inside a 60 s pairing window (boot-opened in Phase 1, button later). Writes require encrypted+authenticated link.
- Settings in NVS namespace "smoker": setpoint, Kp/Ki/Kd, blower min-start, PWM freq, max duty, cook_active.
- Event log: 256 x 12-byte packed records in RAM, popped one per BLE read.
- Serial CSV at 115200 every second for step-response capture.

## OPEN items now carried in include/config.h
- BLOWER_PWM_FREQ_HZ (25 kHz placeholder), BLOWER_MIN_START_DUTY_PCT (30 placeholder): bench step 2.
- PID_KP/KI/KD_DEFAULT: after step-response cook (step 6). Note: HeaterMeter defaults are per F; ours are per C and softer.
- BLE_PASSKEY 240813: bench value, change before enclosure build.
- PIT_SOFT_LIMIT_C 200: confirm below thermostat dial setting.
- New Phase 1.5 thresholds (lid rate, FUEL_LOW, FIRE_OUT, CREEP, caps) are design values from doc 06; tune against logged cooks.

## Phase 1.5 changes (2026-09-21, implemented from doc 06 section 7; built and tested in a clean container, written back to JASON-PC)
Files changed: include/config.h, platformio.ini, README.md, src/main.cpp, src/control/pid.{h,cpp}, src/control/state_machine.{h,cpp}, src/control/long_pwm.h (new), src/hal/blower.{h,cpp}, src/ble/ble_service.{h,cpp}, src/log/event_log.h, src/storage/settings.{h,cpp}, test/test_control/test_control.cpp.

Control changes:
- PID (HeaterMeter form, verified in grillpid.cpp): derivative on measurement against a 60 s EMA (alpha 2/61), conditional-integration anti-windup, integral clamped to [0, imax], halveIntegral() at the first setpoint crossing.
- Startup cap: RAMP and RECOVERY limit duty and integral to STARTUP_MAX_DUTY_PCT (50) until the pit first crosses the setpoint (or HOLD is entered from inside the band). IGNITE_DUTY_PCT 90 to 50, IGNITE_EXIT_MARGIN_C 15 to 17, BLOWER_MAX_DUTY_PCT default 100 to 60.
- Lid detection (LidTrack pattern): rate over a 10 s window, trigger at -0.3 C/s at setpoints up to 121 C, scaled linearly to -1.2 C/s at 260 C; requires setpoint reached, pit below setpoint, and output EMA (alpha 2/241, normalised to max duty) under 90 percent (fuel-out guard). Classic "N C below setpoint" trigger kept as LID_OPEN_TOTAL_DROP_C, default 0 = disabled. Exit after LID_OPEN_MIN_S 30 on: pit at setpoint (LID_OPEN_RECOVER_BAND_C now 0; a 10 C band exited mid-climb and overshot in the unit test), or plateau (11 C above the low and 0 < rate < 0.06 C/s), or 240 s timeout. Exit goes to RECOVERY (startup cap, integral kept) then HOLD after the settle time or RECOVERY_MAX_S 600.
- Rising-lid variant for the drum: pit rising >= 0.3 C/s for 20 s with commanded duty <= 10 percent -> LID_OPEN with the LID_RISING warning; exits when the rise stops.
- Warnings bitmask (new BLE characteristic 000b, new event type WARNING): PIT_HIGH / PIT_LOW (+/- 14 C, 1 C hysteresis, armed after setpoint reached, muted in LID_OPEN), FUEL_LOW (output EMA >= 90 percent and pit fell >= 5 C over 10 min, from a 30 s decimated history), CREEP (duty 0 and pit >= setpoint + 14 C for 15 min), LID_RISING.
- FIRE_OUT fault: pit <= setpoint - 40 C with duty pegged at the current cap for 15 min -> FAULT, blower off.
- Setpoint change in RAMP/HOLD/RECOVERY re-enters RAMP with the startup cap; integral kept and clamped rather than reset.
- Blower: commands >= min-start apply directly (no more compression into [min, max]); commands below min-start use control::LongPwm (10 s period at min-start, on-fraction = requested / min-start). Kick unchanged.
- Power-loss resume: Settings.cook_active persisted (NVS key cook_active) on start, cleared on stop/manual/fault; boot with it set calls startCook() and logs POWER_RESUME.
- Serial CSV gained warnings, out_ema, rate_c_s columns.
- Cleanups: removed the deprecated NimBLEService::start() call (NimBLE 2.x starts services with the server; the compiler's deprecation text says so) and the -DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=2 build flag (sdkconfig.h redefined it, so it had no effect and produced a warning per translation unit).

Build results (2026-09-21, clean container, pio run -t clean then pio run):
- pio run -e s3-n8r8: SUCCESS, 0 warnings from project sources. RAM 11.7 percent (38440 B), Flash 18.9 percent (630058 B of 3342336 B).
- pio test -e native: 27/27 pass. New tests: conditional integration at imax, halve-I, D on EMA, long-PWM duty accounting, startup cap then full authority, halve-I at first crossing, lid rate trigger + plateau exit + RECOVERY + HOLD, lid 30 s minimum then setpoint exit, lid blocked by the fuel-out guard, rising lid on a drum, FIRE_OUT, FUEL_LOW, deviation alarms with hysteresis, CREEP, setpoint change re-enters RAMP with cap.
- One design correction found by the tests: the original 10 C lid recover band exited LID_OPEN while the pit was still climbing at 1 C/s, which is exactly the recovery overshoot LidTrack's author warns about. Band set to 0 (exit at setpoint, plateau, or timeout).

Container note: PlatformIO's downloader ignored REQUESTS_CA_BUNDLE behind the session proxy; fixed by appending the proxy CA to certifi's bundle in PlatformIO's penv. Not relevant on JASON-PC.

## WROVER bench session (before 2026-09-25; date not in the record, reconstructed 2026-09-26 from the Cowork transcript)

Board: Freenove ESP32-WROVER on the short breadboard, env freenove-wrover-max31855. Both HUABAN MAX31855 boards
tested. Supply: LM2596 B set to 5 V feeding the board; 3.2 V measured at the 3.3 V pin and 3.2 V VCC to GND at the
MAX31855 pins (in the 3.0 to 3.6 V range). LM2596 A set to 6.0 V for the legacy blower stage. K-type probe lead
polarity found by the magnet test (alumel, the negative lead, is magnetic); which colour was magnetic is not recorded.

MAX31855 shorted-input test, T+ shorted to T- with a wire (expected: pit_raw_c equals cj_c within about 2 C):

| Column | Board 1 | Board 2 |
|---|---|---|
| pit_raw_c, shorted, settled | steady 2.25 to 2.50 C | steady 2.00 to 2.25 C |
| cj_c at the same time | 25.06 to 25.12 C | 25.06 to 25.12 C |
| Error | about 22.7 C low | about 23 C low |
| Short removed | pit_raw_c 2047.75, tc_fault_bits 0x01 (open circuit) | same |
| Short restored | fault clears, reading returns | same |
| cj_c with a finger on the chip | rises 25 to 27 C and falls back | same |

What that proves: SPI wiring and frame decode are right (cold junction decodes and tracks hand heat), the
thermocouple input path is connected (open-circuit detection works), supply is in range, and the error is a fixed
offset on two units. That is the chip, not the wiring. Both boards returned 2026-09-26 as defective, reason "reads
about 23 C low with input shorted, both units".

Blower stage (LM2596 A at 6 V, IRLZ44N, 1N5819, 200 ohm and 10 k, legacy blower on pin 2) was the next step in that
session; whether it was wired and what it measured is not recorded. It is moot for the Delta (12 V rail, 2N7000 on
the PWM lead) except for proving the IRLZ44N stage.

## First hardware bring-up (2026-09-25, ESP32-S3 DevKitC-1 N8R8, no external circuit)

Board flashed and run with nothing wired: no blower, no MAX31855, no breadboard. Console driven over serial with the
simulated probe (`t <C> [C/s]`).

Board and port facts learned:
- The DevKitC clone exposes two USB ports: native USB (Espressif VID 303A) and a WCH CH343 UART bridge (VID 1A86).
- With ARDUINO_USB_CDC_ON_BOOT=1, Arduino `Serial` goes to the NATIVE port while the IDF log lines still leave via
  UART0 to the CH343. Monitoring the CH343 therefore showed `[E]`/`[I]` logs and NimBLE messages but no banner and no
  CSV, which looked like a firmware hang and was not. New env `s3-n8r8-uart-max31855` sets ARDUINO_USB_CDC_ON_BOOT=0
  so everything lands on the CH343. That is the bench console from now on.
- Uploading over the native port failed with "No serial data received" until the board was put into download mode by
  hand (hold BOOT, tap RESET, release BOOT); the port also re-enumerates, so the COM number changes. Over the CH343
  the auto-reset works normally.
- Typing into `pio device monitor` needs `monitor_filters = ... send_on_enter` and `monitor_echo = yes`; both are now
  set for the UART env (VERIFIED against the PlatformIO monitor CLI docs).

PASS on hardware:
- Boot, LEDC attach on GPIO4, NVS first-run (nvs_open NOT_FOUND then created), watchdog, 1 Hz control task.
- BLE: NimBLE started, host synced, pairing window opened and closed after 60 s.
- Sensor absence detected correctly: MAX31855 reported not present, SENSOR_OPEN raised when a cook was started.
- Simulated probe drives the loop; CSV shows the sim temperature, the derived rate, and state transitions.
- SOFTWARE OVER-TEMP GUARD FIRED FOR REAL: sim ramped past PIT_SOFT_LIMIT_C 200 C, state went FAULT with
  PIT_OVER_LIMIT and the blower command went to 0.
- Fault latching works: startCook is refused while in FAULT until `k` clears it.

BUGS FOUND AND FIXED (console usability, all in src/ui/console.cpp):
- `c` and `m` were silently refused while in FAULT. They now print "refused: in FAULT (<fault>). Type k to clear it
  first."
- `x` printed "# stopped" even though StateMachine::stop() is a no-op in FAULT. It now refuses with the same message
  and otherwise reports the resulting state.
- `k` now reports the state it landed in.
- The parser accepted only LF as a line terminator; it now accepts CR, LF or CRLF.
- Added a raw-line echo (`# > <line>`) so what the board received can be compared with what was typed.

RESOLVED: `t 110 -1` parsing as +1.000 C/s was a character dropped in transit, not a parser bug. With the raw-line
echo in place the same command echoed `# > t 110 -1` and parsed -1.000. Retype and check the echo if a value looks wrong.

Full state-machine run on hardware (simulated probe, 2026-09-25), all as designed:
| Step | Observed |
|---|---|
| `c` with sim at 25 C | IGNITE at duty 50 (startup cap) |
| sim +1 C/s | RAMP after 65 s, at setpoint minus the 17 C margin, allowing for the median-of-5 lag |
| sim parked at setpoint | commanded duty 1 percent, applied 0 percent: long-PWM below the 30 percent min-start |
| `t 110 0` | HOLD 60 s later, matching HOLD_SETTLE_S |
| `t 110 -1` | LID_OPEN 2 s after the drop started, duty 0 |
| `t 95 1` | still LID_OPEN 17 s in, holding LID_OPEN_MIN_S 30 s |
| `x` | stopped, state IDLE |

Not yet observed on hardware: LID_OPEN -> RECOVERY -> HOLD (the run was stopped one second before the 30 s minimum
elapsed), FUEL_LOW, CREEP, FIRE_OUT, power-loss resume, BLE writes from a phone.

## Results on JASON-PC (2026-09-25 evening)
- pio run -e s3-n8r8-uart-max31855: SUCCESS. RAM 11.7 percent (38360 B of 327680), Flash 19.4 percent (647610 B of 3342336). Up from 630058 B on 2026-09-21: the console, the MAX31855 backend and the probe offset.
- pio test -e native: ERRORED, "'gcc' is not recognized as an internal or external command". PlatformIO's native platform needs a host gcc/g++ on PATH and JASON-PC has none (C:\msys64 exists but no mingw64, ucrt64 or usr gcc is installed). The 29/29 result stands from the container. Unblock: install a MinGW-w64 toolchain (for example via msys64 pacman, mingw-w64-ucrt-x86_64-gcc) and put its bin on PATH, or run the tests under WSL2. Same blocker applies to the simulation (sim/README.md).
- Test count in test/test_control/test_control.cpp: 29 RUN_TEST calls (27 on 2026-09-21, 29 at the 2026-09-25 commit).

## Next
1. DONE 2026-09-25: pio run on JASON-PC confirmed (above). pio test -e native is blocked locally, see above.
2. 2026-09-23 boards arrive: flash, confirm COM port, confirm CSV on Serial, confirm SMOKER-CTRL advertises (nRF Connect or similar), 10 characteristics.
3. Wire blower per 04-phase1-wiring.md, MANUAL mode over BLE, measure min-start duty and pick PWM frequency, write values into config.h. Watch the long-PWM pulsing at commands below min-start.
4. MAX31856 arrives 2026-09-29 to 10-02: boiling/ice sanity, then step-response cook with the fan-off draft test first (doc 01 open items).
5. First real cook: log CSV, check that LID_OPEN triggers on a real lid event and not on wind; tune LID_OPEN_DROP_C_PER_S, FUEL_LOW, CREEP from the log.
