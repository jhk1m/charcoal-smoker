# Phase 1 - Firmware Framework (DECIDED)

Created: 2026-09-20
Updated: 2026-09-25 (first hardware bring-up recorded; local build results and the native-test blocker added at the end)
Status (2026-10-22, evening): bench console and CSV telemetry available over BLE (000d, 000e), bonded from JASON-PC; the same test found and fixed BLE writes that had never worked ("BLE debug channels" below). Earlier the same day: long-PWM off-phase now cuts the fan supply and on-phases are at least 5 s, after a false BLOWER_STALL at setpoint on the Delta was traced to its slow restart from a PWM-lead stop ("S3 bench session 2026-10-22" below). Earlier: control core validated on the ESP32-S3 with a simulated probe; MAX31856 board 1 on the S3 pins; the 4-wire fan drive validated on the Delta (PWM, supply cut, tach, both faults; "S3 fan bench" below); M5 current taken, `n 15`, and a sensor that loses power after boot now reads NOT DETECTED and faults a cook ("second session" below). Native tests last passed 39/39 under WSL2 (g++ direct); `pio test -e native` cannot run on Windows until a host g++ is installed.
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
- PIT_SOFT_LIMIT_C 240 since 2026-10-22 (absolute cap; the live limit is setpoint + PIT_OVER_MARGIN_C 40): dial to be fixed at 270 C, M4 proves the trip.
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
MAX31855 pins (in the 3.0 to 3.6 V range). LM2596 A set to 6.0 V for the legacy blower stage. K-type probe: the
connector was cut off, leaving two bare uninsulated leads with no colour code. Polarity found by the magnet test
(alumel, the negative lead, is magnetic). The magnetic lead is marked with a CRIMP. Crimped lead = T- (negative),
plain lead = T+ (positive). That mark is the only polarity reference on the probe; keep it.

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
session and was NOT done (Jason, 2026-09-26). LM2596 A is set to 6 V but nothing hangs off it. The stage is moot for
the Delta (12 V rail, 2N7000 on the PWM lead) except as a test of the IRLZ44N supply-cut path.

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

## Results on JASON-PC (2026-09-25 evening, refreshed 2026-09-26)
- 2026-09-26, after adding env s3-n8r8-uart and the console `i` identity check: s3-n8r8-uart SUCCESS, RAM 11.7 percent (38408 B), Flash 19.5 percent (650662 B); s3-n8r8-uart-max31855 SUCCESS, RAM 11.7 percent (38360 B), Flash 19.4 percent (648630 B). Neither change has run on hardware: `i` needs a sensor chip, none in hand.
- 2026-09-25: pio run -e s3-n8r8-uart-max31855: SUCCESS. RAM 11.7 percent (38360 B of 327680), Flash 19.4 percent (647610 B of 3342336). Up from 630058 B on 2026-09-21: the console, the MAX31855 backend and the probe offset.
- pio test -e native: ERRORED, "'gcc' is not recognized as an internal or external command". PlatformIO's native platform needs a host gcc/g++ on PATH and JASON-PC has none (C:\msys64 exists but no mingw64, ucrt64 or usr gcc is installed). The 29/29 result stands from the container. Unblock: install a MinGW-w64 toolchain (for example via msys64 pacman, mingw-w64-ucrt-x86_64-gcc) and put its bin on PATH, or run the tests under WSL2. Same blocker applies to the simulation (sim/README.md).
- Test count in test/test_control/test_control.cpp: 29 RUN_TEST calls (27 on 2026-09-21, 29 at the 2026-09-25 commit).

## WROVER boot log (2026-09-30, COM3, env freenove-wrover, MAX31856 backend, nothing wired)

Raw log kept in the Cowork transcript. Banner: `charcoal-smoker fw 0.1.0 hw=freenove-wrover tc=MAX31856`,
ESP32 boot ROM `ets Jul 29 2019`, reset reason 1 (POWERON_RESET).

PASS, unchanged from the S3 session:
- boot, no crash, no watchdog reset over the 45 s captured;
- LEDC attached to pin 2, channel 0, 10-bit, which is the WROVER PWM pin in doc 04 pin map A;
- NimBLE started, host synced, pairing window opened;
- console banner printed and the CSV header emitted twice (boot, then again from the control task);
- CSV streaming at 1 Hz, state IDLE, fault NONE, duty 0.

SENSOR: not responding. Every sample reads
`pit_raw_c=-0.01  pit_filt_c=-0.01  cj_c=256.00  tc_fault_bits=0xff`.

Interpretation (inference, not measured): this is the signature of SPI reads returning all ones, i.e. MISO idle
high with no device driving it.
- `0xff` is every fault bit set at once, CJRANGE, TCRANGE, CJHIGH, CJLOW, TCHIGH, TCLOW, OVUV and OPEN together.
  No real fault condition asserts all eight.
- `-0.01` is one LSB negative on the linearized TC register, which is what 0xFFFFFF decodes to at 2^-7 C.
- `256.00` is 65535/256, the cold-junction bytes read as 0xFFFF.
All three are consistent with each other and with nothing being connected.

Consistent with the 2026-09-26 note above: no sensor chip in hand, so there is nothing on the SPI bus. This is
NOT evidence that any board is bad. When a chip is wired, these three numbers changing away from
-0.01 / 256.00 / 0xff is the first thing to check, before any calibration.

Benign: `Preferences.cpp begin(): nvs_open failed: NOT_FOUND` on a first boot, before the namespace exists. It
should not reappear once settings have been saved once.

## WROVER MAX31856 inspection (2026-10-01, COM3, env freenove-wrover, board 1 of the 2-pack)

Wiring per doc 04 pin map A (SCK 14, SDI 13, SDO 33, CS 32, VIN on EXT-3.3V). Doc 09 section 2a steps 3 to 6 all PASS:
- SPI alive: the 2026-09-30 all-ones signature (-0.01 / 256.00 / 0xff) is gone; cj_c reads 26.6 to 26.8 C at room temperature.
- Probe on, at ambient: pit_raw_c 25.1 to 25.2 C, noise about 0.1 C, fault 0x00. About 1.5 C under the cold junction,
  read as the tip sitting in air cooler than the board (inference).
- Polarity: pinching the tip took pit_raw_c from 25.2 to 35.4 C in 6 s and it fell back on release; cj_c unchanged.
  Crimped lead on T- confirmed correct.
- Open input: pit_raw_c 1372.05 C, tc_fault_bits 0x41 (TCRANGE + OPEN); pit_filt_c held its last good value.
- Shorted input, console `i` run twice: `present`; `register readback (type J then K): PASS, writable CR1, real
  MAX31856`; tc 26.94 / 26.89 C against cj 26.83 / 26.78 C, delta 0.11 C both runs, fault 0x00, shorted-input PASS.
  The MAX31855s failed this same test at about 23 C low.

Console note: freenove-wrover has no send_on_enter or echo in platformio.ini, so typed commands are invisible.
`pio device monitor -e freenove-wrover --port COM3 --echo -f time -f send_on_enter` shows them (`# > i`).

Chip marking: MAX31856 (Jason, 2026-10-01; logo not recorded).

Not done: section 2a step 2 (3Vo with 5 V in) not recorded; step 7 ice/boil and offset; board 2.

## 4-wire fan support (2026-10-04, built and unit-tested, NOT run on hardware)
Implements doc 09 section 6a for the Delta BFB0712HH-AWPH. No fan, tach or supply-cut circuit has been connected to this code yet.

What changed:
- include/config.h: `BLOWER_4WIRE` (true by default; build with `-DBLOWER_LEGACY_2WIRE` for the old 6 V blower), the 21 to 28 kHz limits, `FAN_TACH_PULSES_PER_REV` 2, `FAN_RATED_RPM` 4400, `FAN_STALL_S` 3, `FAN_RUNAWAY_MARGIN_PCT` 25, `FAN_RUNAWAY_S` 5, `FAN_TACH_GLITCH_NS` 10000.
- src/hal/blower.*: inverted duty for the open-drain 2N7000 (GPIO high = fan sees low). SUPPLY_EN handling: cut in `begin()` and `off()` (so in IDLE and FAULT, not only on a fault), raised by `setDutyPercent()` after the PWM value is written. With the supply cut the PWM GPIO is low, which releases the fan's PWM lead. The supply is never raised if LEDC failed to attach, and it is cut if a later `ledcWrite` fails. VERIFIED in cores/esp32/esp32-hal-ledc.c (3.3.12): `ledcWrite` turns a duty at or above 1023 into 1024, a constant level, so 0 and 100 percent are glitch-free in both polarities.
- src/hal/fan_tach.*: PCNT unit counting rising edges on FAN_TACH with a 10 us glitch filter, read and cleared every control tick. VERIFIED in framework-arduinoespressif32-libs 3.3.12, esp_driver_pcnt/include/driver/pulse_cnt.h; `pcnt_new_channel` enables the internal pull-up on the edge pin (header comment), the external 10 k in doc 04 is still required (WROOM-32 GPIO34 has no internal pull-up).
- src/control/fan_monitor.h (pure C++): RPM = 60 x pulses / (pulses per rev x dt). BLOWER_STALL: driven above 0 percent with zero pulses for 3 driven seconds (long-PWM off time does not count, a driven second with pulses clears it). BLOWER_RUNAWAY: RPM above (commanded percent of 4400 + 25 percent of 4400) for 5 s. Not judged while the supply is cut.
- src/control/state_machine.*: faults BLOWER_STALL = 8 and BLOWER_RUNAWAY = 9, and `raiseFault()` for a fault found outside `tick()`. It latches FAULT from any active state; main.cpp then calls `Blower::off()`, which cuts the supply.
- src/main.cpp: step 2b reads the tach and feeds the monitor each tick. CSV gains a last column `fan_rpm`.
- Console: `p <n>` pulses per revolution (NVS key fan_ppr, 1 to 8), `T <0|1>` tach faults off or on (RAM only, on again at every boot), `d` prints a fan line, `f <hz>` is held to 21000 to 28000 on the 4-wire build. A stored PWM frequency outside that range is replaced by 25000 at load.

Consequence for the no-wiring bench loop: with nothing on FAN_TACH a cook stops on BLOWER_STALL 3 s after the fan is first driven. Type `T 0` first when running on a bare board.

Results 2026-10-04 on JASON-PC:
- pio run: s3-n8r8-uart SUCCESS (RAM 38440 B, Flash 658118 B of 3342336, 19.7 percent), s3-n8r8 SUCCESS, s3-n8r8-uart-max31855 SUCCESS, freenove-wrover SUCCESS, wroom32 SUCCESS. No compiler warnings. s3-n8r8 also compiled once with PLATFORMIO_BUILD_FLAGS=-DBLOWER_LEGACY_2WIRE: SUCCESS.
- Unit tests: 39 Tests 0 Failures (29 existing, 2 for raiseFault, 8 for the fan monitor). Run under WSL2 Ubuntu, g++ 13.3.0, by compiling test_control.cpp, pid.cpp, state_machine.cpp and Unity's unity.c directly. `pio test -e native` itself still cannot run on Windows (no host gcc on PATH, gap B4).
- Simulation: sim_main.cpp compiles under the same g++. Not re-run; the plant model has no tach.

Owed on the bench before this is trusted (doc 09 section 6a):
1. With SUPPLY_EN low, measure the YELLOW (FG) and BLUE (PWM) leads against controller GND before either is connected to a GPIO (doc 09 M9). DONE 2026-10-05: 10.85 V on both, the same into 10 k; the tach lead needs a fix before it meets a GPIO ("S3 bench session" below).
2. Count tach pulses per revolution and set `p <n>`. At `m 100` (after `X 100`) the CSV fan_rpm should read about 4400.
3. Start duty and the lower run duty, with `m <pct>` and fan_rpm in the CSV.
4. RPM against duty at 30, 40, 50, 60 percent: the runaway margin assumes roughly linear.
5. Pull the tach lead with the fan running: BLOWER_STALL within 3 s. Open the BLUE lead: the fan goes to full speed, BLOWER_RUNAWAY within 5 s, and the fan loses its supply.

Found while reading the driver, FIXED 2026-10-09 (section below): `Blower::service()` ran once per 1 Hz control tick, so the start kick lasted until the next tick (about 1 s) instead of BLOWER_KICK_MS (300 ms). In a long-PWM pulse of one second the whole pulse was therefore at the max duty, not at min-start.

## S3 bench session (2026-10-04 and 10-05, COM6, env s3-n8r8-uart)
Board decided by Jason: the S3-N8R8 is the breadboard and first-smoker board (doc 04 pin map C).

- Flash: `pio run -e s3-n8r8-uart -t upload --upload-port COM6` SUCCESS, CH343 on COM6 (VID:PID 1A86:55D3). First run of the 4-wire build on hardware, bare board, no fan circuit: boots, `LEDC attached to pin 4`, NimBLE starts, `d` prints `fan=4-wire tach_pin=5 supply_en_pin=6 supply=0 ppr=2 rpm=0`, state IDLE. `T 0` typed, so the tach faults were not exercised.
- MAX31856 on the S3 pins (VIN 3V3, SCK 12, SDI 11, SDO 13, CS 10). Board 1 of the 2-pack, the one inspected 2026-10-01 (Jason).
- First three `i` runs read all zeros (tc 0.00, cj 0.00, fault 0x00, readback FAIL). Cause (Jason): power and ground were not properly feeding the MAX31856; GND had been on a breadboard rail not tied to the S3. Fixed by wiring VIN and GND to the S3's own 3V3 and GND pins. All zeros on this backend = sensor unpowered.
- Open input: cj 24.98 C, readback PASS, tc 1372.05 C, fault 0x41 (TCRANGE + OPEN), as expected.
- Shorted input, `i` twice: readback PASS, tc 24.84 / 24.79 C against cj 25.03 / 25.02 C, delta -0.20 / -0.23 C, fault 0x00, shorted-input PASS.
- Found, FIXED 2026-10-09 (section below): on the all-zero readings `i` still printed `present` and `shorted-input PASS` (0.00 against 0.00 is within 2 C; the Adafruit `begin()` does not detect a missing chip). The readback line was the one to trust.
- EGT probe (received 2026-10-02) on the terminal, yellow on T+, red on T-. At ambient: pit_raw_c 24.0 to 24.1 C against cj 25.6 to 25.7 C, noise about 0.1 C, fault 0x00. Polarity: tip pinched, pit_raw_c rose from 24.24 to about 27.4 C in 30 s and levelled, cj unchanged at 25.7 C, so yellow = T+ is correct. The rise is much slower than the bare bench probe's (25.2 to 35.4 C in 6 s on 2026-10-01); read as the sheath's thermal mass (inference, not measured).
- EGT sheath-to-lead continuity: tested ungrounded (Jason).
- Ice point, two baths: run 1 settled 3.1 to 3.5 C and still creeping; run 2 (repacked, stirred) last 30 s avg 3.81, min 3.53, max 3.99 C, cj 25.4 C, fault 0x00. A separate thermometer read 32.5 F (0.3 C) (Jason; taken here as a reading of the same ice bath, its accuracy unverified). So the EGT probe plus board reads about 3.5 C high at the ice point. Offset NOT set: one point cannot tell a fixed offset from stem conduction or a weak signal; the boil point decides.
- Response: in water the probe covered about two thirds of a 21 C step in roughly 23 s (run 1, read off the 10 s samples).
- Hot water, NOT a valid boil point: the water was off the boil and cooling. EGT probe rose 27.6 -> 70.25 C in 60 s (peak), then fell to 66.0 to 67.8 C over the last 30 s (avg 66.75), cj 25.7 C, fault 0x00. Reference thermometer: 173.8 F (78.8 C) at its highest (Jason); the two readings were not taken at the same instant, so the gap (up to 8.5 C low) is an upper bound, not a measurement.
- Pattern so far: about 3.5 C HIGH at the ice point and LOW in hot water, i.e. pulled toward the 25 C cold junction at both ends, roughly 8 to 15 percent short on signal. A probe offset cannot correct that. Candidate causes, none confirmed: thermocouple wire that is not true K, non-K leads spliced inside the probe (the junction to those leads then acts as the cold junction), or stem conduction at shallow immersion. Offset left at 0.
- Magnet test on the EGT lead ends: red is magnetic (Jason), as the K negative lead should be, so the leads are thermocouple wire and the spliced-plain-lead cause is unlikely.
- Hot point, preheated container at the bench: EGT probe peaked at 84.89 C (60 s after immersion), then fell slowly, 83.7 C at about 90 s, last 30 s avg 83.40 C (82.57 to 84.16), cj 25.9 C, fault 0x00. Reference thermometer 192 F (88.9 C) (Jason; the exact second it was read is not recorded, asked for at about 90 s). EGT reads about 4 to 5 C LOW here, against about 3.5 C HIGH at the ice point. Straight line through the two points: reading = about 0.91 x true + 3.5 C; extrapolated (NOT measured) that is about 6 to 7 C low at 110 C. The reference thermometer's own accuracy is unverified. Offset left at 0: a single offset cannot correct both ends, and the firmware has no gain term.
- DECISION 2026-10-04 (Jason): first rejected, then KEPT for the first tests with an offset; a replacement pit probe follows when convenient (doc 03 names the Procon B09HSJ4PBB; nothing ordered yet). `o 6.5` typed on the S3, `d` shows `offset=6.50 C`, saved to NVS. The 6.5 C is the two-point line extrapolated to a 110 C cook, not a measurement; with it the reading is about 10 C high at the ice point, which does not matter for a cook. Verify at the grate against the reference thermometer on the first fire. The bench-probe control was not run, so the board is not yet cleared as the cause.
- 2026-10-05, fan bench, no S3 connected. Stage 1: Delta on straight 12 V (jack, 2 A fuse, 1N5819, fan fuse per doc 04), BLUE and YELLOW open: runs (Jason). Running current NOT reported. Stage 2 (doc 09 M9): IRLZ44N in the BLACK lead, gate 10 k to GND; YELLOW and BLUE each 10.85 V against ground, unchanged with a 10 k load to ground. The tach lead cannot go straight to GPIO5; fix not yet chosen.
- Not done: the bench probe as a control in the same bath, the Delta's running current, the tach-lead fix, any fan lead on the S3. (Tach-lead fix and all four fan leads DONE 2026-10-10, "S3 fan bench" below.)

## Two driver fixes (2026-10-09, built; flashed to the S3 2026-10-10; the sensor fix was INCOMPLETE, see the second bench session below)
- Start kick length. The control task now sleeps to its 1 Hz tick in BLOWER_SERVICE_MS (10 ms) slices and calls `Blower::service()` in each, so the 100 percent kick ends at BLOWER_KICK_MS (300 ms, plus up to one slice) instead of at the next tick. Everything stays in the control task: no timer, no lock, no change to `Blower`. The tick is scheduled from a fixed `tick_end`, so the 1 Hz cadence does not drift; an overrunning tick catches up the same way `vTaskDelayUntil` did. Files: include/config.h (`BLOWER_SERVICE_MS`), src/main.cpp (the sleep loop; the per-tick `service()` call is removed).
- Unpowered MAX31856 reported present. `PitSensor::begin()` on the MAX31856 backend now writes type K and requires it to read back before setting `present_`; the Adafruit `begin()` returns true with no chip answering, and an unpowered board reads 0x00 everywhere (type B). Consequence: `i` prints `NOT DETECTED (check VCC, CS, CLK, DO/SDO wiring)` and `read()` reports SENSOR_OPEN for an unpowered sensor, instead of `present` and a false `shorted-input PASS`. `i` still re-runs `begin()` when the sensor was absent at boot, so a board powered after boot is found. File: src/hal/pit_sensor.cpp. The MAX31855 backend already rejected all-zero frames.
- Results on JASON-PC: pio run SUCCESS on s3-n8r8-uart (RAM 38440 B, Flash 657962 B, 19.7 percent), s3-n8r8, s3-n8r8-uart-max31855, freenove-wrover, wroom32. No warnings. Unit tests not re-run: neither change touches a file the native tests compile (pid, state_machine, fan_monitor). Not flashed.
- Bench check owed: with the sensor unpowered, `i` must print NOT DETECTED; with it powered, PASS as on 2026-10-04. The kick length is only visible with the fan or a scope on GPIO4 (300 ms at full duty from a cold start, then the commanded duty).

Decision 2026-10-09 (Jason), doc 09 M9: blocking diode in the fan's YELLOW lead, pull-up on the GPIO side (doc 04 blower section). Built and measured 2026-10-10, next section.

## S3 fan bench (2026-10-10, COM6, env s3-n8r8-uart, first run of the 4-wire firmware against the Delta)
Circuit as wired (doc 04 map C): RED on the 12 V board (jack, 2 A fuse); BLACK on the IRLZ44N drain, gate 10 k to GND and 200 ohm to GPIO6; BLUE on a 2N7000 drain, gate 10 k to GND and 200 ohm to GPIO4; YELLOW through a 1N5822 (stripe toward the fan) into a breadboard row with a 10 k pull-up to the S3 3V3 pin and a jumper to GPIO5. S3 on USB-C (CH343, COM6), not on the LM2596. 12 V negative and S3 GND on one rail. The two M9 10 k meter loads that had been soldered inline on YELLOW and BLUE were cut out first (doc 09 M9).
- Diode check: 12 V on the fan, board IDLE (supply cut), GPIO5 jumper out: tach row 3.2 V against S3 GND (Jason). Diode blocks the 10.85 V. An earlier 3.3 V reading taken with no 12 V on the fan was not a test and is discounted.
- Flash of today's build (driver fixes section): SUCCESS. Console driven from a pyserial script (DTR and RTS held low so the board is not reset on open); `v` CSV stream was already on.
- `m 100` with no probe connected: SENSOR_OPEN fault at once, fan never driven. Manual mode needs a valid reading; `t 25` used for the rest.
- First `m 100` run, before RED was on 12 V: supply enabled, zero tach pulses, BLOWER_STALL after 3 driven seconds, supply cut. The stall path works end to end.
- `m 100` with 12 V: fan_rpm 3393 in the first second, then 4615 steady (4585 to 4645 over 30 s) with ppr 2. Spec 4400 +/- 10 percent, so 2 pulses per revolution is CONFIRMED; `p 2` stays.
- Duty sweep, `n 5` so manual duties below 30 are applied directly (restored to `n 30` after), `X 100` (restored to `X 60`), tach faults off (`T 0`). Steady fan_rpm, stepping down with the fan running: 100 percent 4615, 60 3250, 50 2817, 40 2338, 30 1780, 25 1490, 20 1160, 15 830, 10 480, 5 stops (0 within 4 s). Starts from rest, each with the 300 ms kick: 10, 15, 20, 25 and 30 percent all started and settled to the same RPM as above within 4 s. One trial per point.
- Read: RPM against duty is close to linear, about 44 RPM per percent plus 450, from 10 to 60 percent. The BLOWER_RUNAWAY threshold (commanded share of 4400 plus 1100) clears the measured RPM at every point by at least 640 RPM. Min start duty is at most 10 percent with the kick, min run duty is between 5 and 10 percent. `n` left at 30 for Jason to lower (15 is a safe margin on one trial each); BLOWER_MIN_RUN_DUTY_PCT can be set from this once repeated.
- BLOWER_RUNAWAY PASS: `T 1`, `m 30`, fan steady at 1770 to 1800 RPM; BLUE pulled from the 2N7000 drain row about 6 s in (Jason); fan_rpm 2008, 4346, then 4615 to 4645; BLOWER_RUNAWAY latched 5 s after the first over-threshold second, duty 0, supply cut, fan_rpm 0 the next second. Threshold at 30 percent commanded is 1320 + 1100 = 2420.
- Seen in the CSV: the start kick is written at the max duty cap (`applied=60` on the first tick with `X 60`, `applied=100` with `X 100`), not at 100 percent. That is `Blower::apply()` writing `max_pct_`; the doc 06 wording "100 percent kick" is only true when the cap is 100.
- BLOWER_STALL with the fan running PASS: `m 30`, fan steady 1768 to 1798 RPM; YELLOW pulled from the tach row about 11 s in (Jason); fan_rpm 30, 0, 0; BLOWER_STALL latched after the third driven second with no pulses, duty 0, supply cut. Fault cleared with `k`, board left IDLE.
- Doc 09 section 6a bench list is now complete except M5 (running current) and the kick length on a scope. The 4-wire firmware is validated on the Delta: PWM, supply cut, tach, both faults.

## S3 fan bench, second session (2026-10-10, COM6, env s3-n8r8-uart; rig unchanged from the section above)
- Board state at open: IDLE, 22 min up, `n 30`, `X 60`, `o 6.5`, tach faults on, CSV on, MAX31856 powered (cold junction 27 C), `t 25` active.
- 10 percent start from rest, three trials (`n 5`, `m 10`, 6 s, `m`, 6 s): every trial started on the kick (1593 RPM in the first second, the kick at the 60 cap) and settled to 480 RPM by the fourth second. Four of four with the 2026-10-10 trial. 5 percent from rest, one trial: kick to 1577 RPM, then 1060, 599, 270, 30, 0: the fan does not run at 5. Min run duty is above 5 and at most 10. `n 15` SET and saved (margin over the measured 10). `n` restored from 5 before the change.
- M5 running current, meter in series on RED (10 A range), temporary, out again after: `X 100`, `m 100` 12 s, `m 30` 12 s, `m`. Fan 4585 RPM at 100 percent, 1768 at 30. Jason: 0.27 A at 100 percent, "then it dropped to 0.01A"; whether the 0.01 A was the 30 percent hold or after `m` was not pinned down. Label says 0.42 A, datasheet 0.34 A rated: 0.27 A free air at 4585 RPM is under both. `X 60` restored. The fan's 12 V was unplugged after this run and stayed off for the sensor checks.
- Unpowered MAX31856, `i` with VIN pulled AFTER boot (chip powered at boot): FAIL on the 2026-10-09 build. Printed `present`, `register readback FAIL ... relabeled or dead chip`, tc 0.00 cj 0.00, and a false `shorted-input PASS`. Cause: `selfTest()` only re-ran `begin()` when `present_` was false, and `present_` had latched true at boot; `read()` saw fault bits 0x00 and 0.00 C on both channels and called it a valid reading (0.00 plus the 6.5 offset would have fed the PID). The 2026-10-09 fix only covered a chip unpowered at boot.
- FIX (src/hal/pit_sensor.cpp, both backends): `selfTest()` always re-runs `begin()`. MAX31856 `read()`: fault bits 0x00 with exactly 0.000 C on both the thermocouple and the cold junction is the unpowered signature (a live chip never reads exactly zero on both), so it sets `present_` false and returns SENSOR_OPEN; `present_` then stays false until `i` or a reboot re-probes, which is intended: the cook path never re-probes a sensor that has gone away. Built on all five envs (s3-n8r8-uart RAM 38440 B, Flash 657994 B, 19.7 percent; s3-n8r8 650614 B; s3-n8r8-uart-max31855 655914 B; freenove-wrover and wroom32 688247 / 688135 B of 1310720), no warnings. Unit tests not affected (pit_sensor.cpp is not compiled by the native tests). Flashed to the S3; the first upload hung in esptool with no output for over five minutes and was killed, the retry wrote and verified in 17 s.
- Results on the new build: boot with VIN pulled, `i` prints NOT DETECTED. VIN restored, `i` prints present, readback PASS, tc 26.83 C cj 26.66 C, CSV reads the probe (about 33 C with the 6.5 offset). VIN pulled with the board running: CSV goes to 0.00 on all three channels, `i` prints NOT DETECTED, `c` at 110 C faults SENSOR_OPEN on the first tick (IDLE does not latch sensor faults by design, so the CSV shows fault NONE until a cook is started). `k` clears. VIN restored, `i` re-probes, readings return. Bench left: sensor live, fan 12 V unplugged, `n 15`, `X 60`, IDLE.
- Still owed: the kick length on a scope. BLOWER_MIN_RUN_DUTY_PCT DONE 2026-10-22, next section; the bench-probe control was dropped (Jason, 2026-10-22: EGT compensation final).

## S3 bench session 2026-10-22 (COM6, env s3-n8r8-uart; rig as left 2026-10-10, fan 12 V found plugged in)
Three builds flashed in turn; everything below is on the last one unless marked.

1. BLOWER_MIN_RUN_DUTY_PCT. `cfg::BLOWER_MIN_RUN_DUTY_PCT = 10` (doc 09 M5 data: runs at 10, dies at 5), passed into `control::LongPwm` as a hysteresis threshold: while the last output is non-zero the pass-through threshold is min-run, from rest it is min-start. Not NVS-tunable. Bench: `m 20` then `m 12` held applied 12, 599 RPM, no stop-start. `m 12` from rest: kick, then 12, 599 RPM.
2. FINDING, pre-existing (not caused by item 1): false BLOWER_STALL at setpoint. Simulated probe frozen at the setpoint puts the PID at 1 percent, so long-PWM gave one on-tick (300 ms kick at 60 then 15) every 10 s with the fan stopped through its PWM lead in between. The Delta restarted from a PWM-lead stop 0 to more than 4 s late, or not at all inside the on-tick, so `FanMonitor` (driven, no pulses, counted across the long-PWM gaps by design) reached 3 s after three failed on-ticks and faulted. Restart test, 15 percent after a PWM-0 hold (`m 15`, `m 0`, rest, `m 15`): rest 12 s started at once; rest 3 s, 4 s and 25 s started 2 to 3 s late; two 70 s rests in a cook started 3 s late and more than 4 s late. Cold starts after a supply cut started on the first tick every time (6 of 6 this session, 4 of 4 on 2026-10-10).
3. FIX A: `cfg::BLOWER_LONG_PWM_MIN_ON_S = 5`. `LongPwm` stretches the period instead of shortening the on-phase when on-time would be under 5 s, so the average duty is still requested divided by min-start (1 percent at min-start 15 = 5 s on, 70 s off). `FAN_STALL_S` raised 3 to 5 (restart delay 3 s plus the one-tick tach lag plus margin). Unit test `test_long_pwm_min_on_stretches_period`.
4. FIX B: `hal::Blower`: an applied duty of 0 now cuts the fan supply (SUPPLY_EN low, PWM gate low) instead of holding the PWM lead at 0 percent; the next non-zero duty re-arms it, PWM written before the supply is raised as before. So every long-PWM on-phase is a cold start. `armed()` is now "powered and driven", which is what `FanMonitor` wants. Fix A alone (one flash earlier) held 225 s at setpoint with no fault, but one 5 s on-phase still saw no rotation at all and another started 3 s late; with Fix B three on-phases in 235 s each spun on the first tick (1558 to 1588 RPM after the kick), no fault.
5. Sim plant moved to the measured Delta: `fan_cfm_max` 15.3 (datasheet free air), `fan_stall_pct` 7, `fan_start_pct` 10 (were 10, 22, 28, generic guesses). Sim metrics changed accordingly (doc 08). Sim's `SimBlower` takes min-run and min-on from config.h too; it has no supply model, so Fix B does not apply there.
6. Regression on the final build, tach faults on: console `i` PASS (board 1, delta -0.17 C); simulated-probe cook IGNITE at 50 to RAMP at 93 C (2847 RPM at 50 percent), HOLD after the settle window at a frozen 110 C, LID_OPEN on a -0.6 C/s drop (blower 0, supply cut), RECOVERY, `t 200` PIT_OVER_LIMIT with the supply cut, `k` clears. No BLOWER_STALL or BLOWER_RUNAWAY in about 20 minutes of fan running across the runs.
7. Checks: unit tests 41 Tests 0 Failures under WSL2 (g++ 13.3.0, compiled directly, -Wall -Wextra clean). pio run SUCCESS on all five envs, no warnings: s3-n8r8-uart RAM 38440 B, Flash 658150 B (19.7 percent); s3-n8r8 650770 B; s3-n8r8-uart-max31855 656070 B; freenove-wrover 688399 B; wroom32 688287 B. Sim scenarios and `--sweep` re-run; the sim's plot.py did not run (no matplotlib in WSL2 or in the Windows Python; PNGs in sim/out are still the 2026-09-21 set).
8. DONE later the same day, see "Short cooks build" item 6. Was: BLOWER_STALL with the supply-cut model has NOT been re-proven by pulling YELLOW (wiring step; the logic holds because the monitor judges the previous tick before the new command, so 5 driven ticks without pulses fault at the end of a 5 s on-phase). Kick length on a scope. Bench left: sensor live, fan on 12 V, IDLE, `n 15`, `X 60`, `o 6.5`, flashed build = this one.

## Short cooks build 2026-10-22 (doc 06 section 6a, Jason confirmed the numbers)
1. `SETPOINT_MAX_C` 180 to 200. `PIT_SOFT_LIMIT_C` 200 to 240 and is now the absolute cap; new `PIT_OVER_MARGIN_C` 40; `StateMachine::overLimitC()` = min(setpoint + margin, cap) replaces the fixed compare in `tick()`. At setpoint 110 the fault line is 150 (was 200), at 200 it is 240. `d` prints `limit=`.
2. Cook timer: `StateMachine::setTimer(seconds)`, counts down in `tick()` in any state, raises `Warning::TIMER_DONE` (0x20) at zero and holds it until the timer is set again or `stop()`. Console `a <min>` (`a 0` clears), BLE characteristic `000c` (uint16 minutes, write enc, read). The warning reaches the phone on `000b` as before. Not saved to NVS; a reboot forgets it.
3. Manual mode with a timer set aside (doc 09 N9). Hard cutoff: thermostat dial to 270 C, proven in M4 (doc 06 section 6a).
4. Checks: 42 Tests 0 Failures under WSL2 (new `test_sm_timer_warns_at_zero_and_clears_on_stop`; `test_sm_over_limit_faults` now covers the margin and the cap); sim nominal unchanged (reach 37 min, overshoot 3.4, HOLD rms 0.51, duty 19.8); pio run SUCCESS on all five envs, no warnings (s3-n8r8-uart RAM 38464 B, Flash 658806 B; s3-n8r8 651422; s3-n8r8-uart-max31855 656718; freenove-wrover 689123; wroom32 689011).
5. Bench, COM6, flashed and proven with the simulated probe, tach faults on: `d` prints `limit=150 C` at setpoint 110 and `limit=240 C` at 200; cook at 110 with `t 149` stays RAMP (PIT_HIGH only), `t 150` faults PIT_OVER_LIMIT at once, supply cut, `k` clears; `a 1` shows `timer=58 s` two ticks later, warnings 0x20 after 60 s in IDLE, `a 0` clears it. Board left IDLE, setpoint 110, `t off`.
6. BLOWER_STALL re-proven under the supply-cut model, same session, three pulls by Jason (ask-first wiring, nothing else moved):
   - `m 10` (steady 10 percent, 480 RPM, min-run pass-through so no long-PWM): BLACK pulled by mistake. Fan unpowered, rpm 480, 210, 0, five driven seconds with no pulses, BLOWER_STALL, duty 0, supply cut. Counts as the dead-fan case.
   - `m 10` again, YELLOW pulled from the tach row (fan kept spinning): rpm 450, 182, 0, five driven seconds, BLOWER_STALL at the sixth tick, supply cut stopped the fan about 5 s after the pull (Jason heard it). Lost-tach case.
   - `m 5` (long-PWM: 60 kick, four ticks at 15, ten ticks at 0 with the supply cut, 15 s period as designed; 1588, 1169, 869, 839, 809 RPM on every on-phase): YELLOW pulled and left out; the next on-phase ran its five driven ticks with rpm 0 and BLOWER_STALL latched at the end of that on-phase. Driven seconds across the long-PWM gap are counted as intended.
   `k` cleared each fault. Doc 09 N7 and section 6a: stall detection fully proven on hardware, nothing owed there except the kick on a scope.

## BLE debug channels and the dead-writes fix (2026-10-22, COM6, env s3-n8r8-uart; bench rig unchanged)
Jason asked for Bluetooth with debugging. Built, flashed and proven from JASON-PC (Intel Wireless Bluetooth, Python 3.12, bleak 3.0.1):
1. `UUID_CONSOLE` 000d (utf8, write enc+authen, notify): a written line is queued (`Console::inject`, FreeRTOS queue of 4) and run inside `Console::poll()` on the control task, so BLE and serial share one parser and the single-writer rule holds. Console output now goes through a `Print` member that mirrors to Serial and, while a remote line runs, to `Service::publishConsole`. `UUID_TELEMETRY` 000e (utf8, notify): the CSV line is formatted once in `main.cpp`, printed to serial when `v` allows it, and notified on 000e whenever a peer is connected. `Service::notifyText` chunks at the peer MTU minus 3 (`getPeerInfo(0).getMTU()`), because the NimBLE host truncates longer notifications silently. First peer only.
2. BUG FOUND by the first PC-side write: `Write Not Permitted` on 000d. Cause, verified in `ble_gatts.c` (`ble_gatts_chr_properties`): the ATT write permission comes only from `BLE_GATT_CHR_F_WRITE`; `WRITE_ENC` and `WRITE_AUTHEN` are modifiers. Every write characteristic (0003, 0007, 0008, 000c) had been `WRITE_ENC | WRITE_AUTHEN` since the first build, so no BLE write has ever worked; 0009 had the same shape for reads. Fix: `P::WRITE` added to all five write characteristics and `P::READ` to 0009. Doc 02 section 7's "phone-side writes NOT yet tested" is why it survived.
3. Pairing from Windows: bleak's `pair()` is Just-Works only, which the firmware's MITM requirement rejects. `tools/pair.py` uses WinRT custom pairing with `DevicePairingKinds.PROVIDE_PIN` and `accept_with_pin(passkey)` (passkey read from config.h, never on the command line). First attempt was rejected by my handler (wrong method name, `accept(pin)` is not the API), two retries returned status 19 while the board's window state was uncertain; the run with the board log captured alongside paired at once: Windows status 0, protection level 3 (encryption and authentication), board `onAuthenticationComplete ... bonded=1 new_bond=1`, MTU 256. The bond survived the next reflash (app partition only, NVS kept): `bonds stored: 1`, `new_bond=0`.
4. Proven over BLE, bonded link: `d` (four reply lines notified back and echoed on serial), `a 1` then `d` shows `timer=57 s`, native uint16 write of 5 on 000c reads back 5, `a 0`, `v` toggles the serial CSV, 000e streams the CSV at 1 Hz throughout. Board log shows the three subscriptions and clean disconnect reason 531. One connect attempt right after the reflash was canceled by Windows mid-read; the retry 8 s later was clean.
5. Checks: 42 Tests 0 Failures under WSL2 (g++ direct; control code untouched); pio run SUCCESS on all five envs, no warnings: s3-n8r8-uart RAM 38528 B, Flash 660046 B (19.7 percent); s3-n8r8 652642; s3-n8r8-uart-max31855 657954; freenove-wrover 690435; wroom32 690323. Flashed to COM6. Board left IDLE, setpoint 110, serial CSV on, fan 12 V plugged in, bonded to JASON-PC.
6. Web UI, same evening: `tools/ui.html`, Web Bluetooth, no dependencies (canvas chart drawn by hand). Loaded in Chrome through a throwaway localhost server (the Claude-in-Chrome bridge cannot open file:// URLs): renders, script parses, Connect runs and fails with `Web Bluetooth API globally disabled`. The browser the bridge drives is Brave 156 (navigator.brave confirmed), which ships Web Bluetooth off: brave://flags, "Web Bluetooth API", Enabled, relaunch. Plain Chrome also runs on JASON-PC and needs nothing. Not yet connected to the board.
7. Timer over BLE, found by Jason from the web UI ("5 min set" shown, nothing counting): the 000c read value was never written by the firmware, only by the phone, so the UI showed the last write. Now `Service::publishTimer` sets and notifies minutes left (rounded up) whenever it changes, 000c has NOTIFY, the UI subscribes and shows `N min left` or `off`. Proven over serial after the reflash: `a 1`, `timer=56 s` 4 s later, `warnings=0x20` at 60 s. Not re-proven over BLE: the board was not advertising (a browser held the link) and Windows' GATT cache was stale after the handle shift from the new CCCD; `tools/ble.py` now passes `use_cached_services: False`. All five envs build (s3-n8r8-uart Flash 660126 B); flashed.
8. Passkey out of source (2026-10-23, before the repo goes public): `firmware/.env` holds `SMOKER_BLE_PASSKEY`, `load_env.py` (PlatformIO `extra_scripts = pre:`) turns every line into a `-D`, `config.h` errors on Arduino builds without it and defaults to 0 for host builds. `.env` is git-ignored, `.env.example` is the template, `tools/pair.py` reads `.env`. A fresh random value was generated into `.env` by PowerShell and never printed. The define was confirmed in the verbose compile line (value masked), all five envs build, s3-n8r8-uart flashed. The PC bond from earlier still works, as expected: bonds keep their own keys.
9. Not done: phone client (nRF Connect would pair the same way, with the passkey typed), BLE config characteristics for gains, caps and offset (the console over 000d covers them for now), pairing button (doc 09 N6, window still opens at boot), passkey change before the enclosure build.

## Next
1. DONE 2026-09-25: pio run on JASON-PC confirmed (above). pio test -e native is blocked locally, see above. 2026-10-04: the tests run under WSL2 with g++ directly (section above).
2. 2026-09-23 boards arrive: flash, confirm COM port, confirm CSV on Serial, confirm SMOKER-CTRL advertises (nRF Connect or similar), 10 characteristics.
3. Wire blower per 04-phase1-wiring.md, MANUAL mode over BLE, measure min-start duty and pick PWM frequency, write values into config.h. Watch the long-PWM pulsing at commands below min-start.
4. MAX31856 received; board 1 inspection PASS 2026-10-01 and on the S3 2026-10-04 (above). Ice and hot-water points taken 2026-10-04 with the EGT probe (S3 bench session). Next: step-response cook with the fan-off draft test first (doc 01 open items).
5. First real cook: log CSV, check that LID_OPEN triggers on a real lid event and not on wind; tune LID_OPEN_DROP_C_PER_S, FUEL_LOW, CREEP from the log.
