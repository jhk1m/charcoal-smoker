# Open Items and Gap Register (audit 2026-09-22, updated 2026-09-25)

Purpose: one place that answers "are we done". Honest status, not a summary of what exists. Every line is either DONE, OPEN (with what unblocks it), or NOT BUILT (with the decision needed).
Audit method: read the firmware on JASON-PC, grepped for the features docs 06 and 07 claim, checked purchase status in doc 03 and 07, checked what the simulation exercised (doc 08).

## 0. Short answer
Updated 2026-09-25. The control LOGIC is implemented, unit-tested, simulated, and now validated on the real ESP32-S3 against a simulated probe (doc 05). Nothing is wired: no breadboard, no replacement blower, no sensor connected, no thermostat check. No threshold in the firmware is a measured value. Several features the docs promise are not built, including all 4-wire fan support. It is not "done".

Docs currency as of 2026-09-25: 00 handoff created; 01 (air path, duct), 02 (rewritten for the Delta and the hardware bring-up), 03 (arrivals, breadboard correction, superseded items), 04 (4-wire wiring), 05 (bring-up record), 06, 07, 08, 09 all current.

Evening 2026-09-25 additions: blower adapter designed, verified and submitted for print (doc 07 A3, output\3d-print\); parts ordering consolidated at Mouser, breadboards and 2N7000s ordered; local firmware build confirmed (doc 05); native tests found NOT runnable on JASON-PC (new gap B4); doc 07 order status contradictory (new gap B5); git repo re-rooted from firmware\ to the project directory so the docs are versioned too.

## 1. Blocking the bench bring-up (fix first)

| # | Gap | Evidence | Fix | Effort |
|---|---|---|---|---|
| B1 | DONE 2026-09-24: serial bench console added (src/ui/console.*). `?` lists the commands; `t <C> [C/s]` runs the state machine against a simulated probe with nothing wired. Originally: no runtime way to change PWM frequency, min-start duty, PID gains, max duty, or lid detect. `Blower::setFrequency`, `StateMachine::setGains` and `setLidDetectEnabled` exist but are never called; `main.cpp` has no Serial input; BLE exposes none of them. Every bench value change means editing config.h and reflashing, and bring-up steps 2, 3 and 6 are iterative by nature. | grep: no callers of those methods; no `Serial.read` in main.cpp; 10 BLE characteristics, none for config | Serial command console on the existing 115200 link (set freq, duty, gains, min-start, max, setpoint, lid-detect on/off, dump settings), and/or the same as BLE config characteristics | half a day |
| B2 | DONE 2026-09-24: probe_offset_c in NVS, applied in PitSensor::read, set with the console `o <C>`. Originally: no probe offset or calibration storage. Doc 06 E14 calls for a per-probe offset (Traeger allows -15 to +15 F, HeaterMeter has `Offset`); nothing in the firmware has one, so the ice/boiling check in bring-up step 3 has nowhere to land. | grep: no "offset" in src/ | Add `probe_offset_c` to NVS settings, apply in `PitSensor::read`, expose in the console from B1 | 1 hour |
| B3 | DONE 2026-09-25: git init run in firmware/, first commit 5393feb. Evening: the .git directory was moved up to the project root so output\ (docs, spec PDFs, 3d-print) and HANDOFF.md are versioned with the firmware; .gitignore paths adjusted (firmware/sim/sim, firmware/sim/out/*.csv). Git is run locally (PowerShell or Claude Code CLI on JASON-PC), never through the Claude device bridge, because the bridge cannot delete files and a stale .git\index.lock then has to be cleared by hand. | no .git in firmware/ at audit time | commit per bench session | 5 minutes |
| B4 | OPEN 2026-09-25: `pio test -e native` cannot run on JASON-PC, "'gcc' is not recognized". No host g++ installed (msys64 present, no compiler). The 29/29 pass is from the container only. Same blocker for the simulation. | pio test output, doc 05 | install MinGW-w64 gcc (msys64 pacman mingw-w64-ucrt-x86_64-gcc) and add its bin to PATH, or run under WSL2 | 15 minutes |
| B5 | CLOSED 2026-09-25: Jason confirmed everything in doc 07 is ordered (blower, 1N5822, jumper kit, ADS1115, buzzer, breadboards, 2N7000s, local hose, clamps, flap sheet). The contradictory line in doc 07 was removed. | Jason, 2026-09-25 evening | none; wait for delivery | - |

## 2. Blocked on physical measurements (nothing has been measured yet)

| # | Measurement | Unblocks |
|---|---|---|
| M1 | DONE 2026-09-24: duct OD 23.81 mm, wall 0.85 mm, bore 22.1 mm, run 20 in with two bends; the existing blower couples with a 1 in ID sleeve, about 1 in of engagement. 2026-09-25: Delta outlet 24.1 x 17.5 mm taken from the spec drawing; adapter designed and print submitted (doc 07 A3). Still owed: caliper check of the outlet on arrival | Blower DECIDED on static pressure (doc 06 R8 and 3.3): Delta BFB0712HH-AWPH; 75 mm Same Sky and FireBoard ruled out |
| M2 | Exhaust stack: is there a cap, damper, or nothing | Every vendor's setup rule is "exhaust nearly closed"; without a damper the pit is at the mercy of weather |
| M3 | Fan-off draft test (fire lit, blower unplugged, door shut: does the pit climb past 120 C) | Whether the flap is mandatory. Doc 08 shows the controller loses authority entirely without it |
| M4 | Thermostat: which terminal pair is closed cold, dial setting that trips, and that it re-closes | The hardware over-temp cutoff is unverified. Must trip above PIT_SOFT_LIMIT_C 200 C and above SETPOINT_MAX_C 180 C |
| M5 | Delta running current at 12 V and its real min-start and min-run duties (datasheet: 0.34 A rated, 0.42 A max, start at or below 30 percent) | Power budget, the fuse value, BLOWER_MIN_START_DUTY_PCT and the future BLOWER_MIN_RUN_DUTY_PCT |
| M6 | Probe port thread; probe length needed to sit at grate level about 8 cm inboard | Probe placement error is the top cause of bad control (doc 06 E12) |
| M7 | Basket weight before and after two cooks | Burn rate, which calibrates the fuel gauge in P9 |

## 3. Parts still to buy (doc 07)

Superseded 2026-09-25 by doc 07 "Order of purchase": one Mouser cart is the rule. Ordered: BB830 breadboards x2, 2N7000 x20 (Mouser order 1); blower per doc 07 STATUS, but see B5. Local trip: 1 in ID heater hose, four worm clamps, 0.4 to 0.5 mm silicone sheet, gasket tape. ThermoWorks: probes x2 once the plug is measured (B3). Closed without purchase: A8 diode (1N5819 on hand is adequate), B4 resistors (10 k 1 percent on hand). Still blocked on measurements: A10 exhaust damper (M2), B3 jacks, C1 to C4 enclosure.
Received: ESP32-S3 boards and MAX31855 boards 2026-09-23. In transit: MAX31856 2026-09-29 to 10-02. The MAX31855 already on hand covers bench step 3 once the breadboard lands.

## 4. Built but not validated on hardware
- Every threshold in config.h is a design value from doc 06, not a measured one: PID gains, lid rate 0.3 C/s, FUEL_LOW 5 C over 10 min, FIRE_OUT 40 C for 15 min, CREEP 14 C for 15 min, startup cap 50 percent, max duty 60 percent.
- The rising-lid check (drum lid open raises the pit) is a proposal of ours. No commercial controller documents it and it has not been seen on this drum.
- The simulation (doc 08) used an assumed plant. It found two real bugs, which is what it is for; it cannot confirm tuning.

## 5. Not built, spec'd only

| # | Feature | Where spec'd | Needed for |
|---|---|---|---|
| N1 | Cook Manager: meat state machine, stall detect, wrap prompt, ramp-down, hold, rest, food-safety watchdogs | doc 06 section 6 | Any meat-probe behaviour. Needs B1 probes + ADS1115 |
| N2 | Fuel gauge, FUEL_LOW lead time from basket load, REFUEL state, lump vs briquette selector | doc 06 P9, section 1b | Charcoal-specific handling, the thing that differentiates this build |
| N3 | Local alarm (buzzer) | doc 07 B5; no pin assigned in pins.h | Unattended or overnight cooks. Today a FIRE_OUT at 2 am is silent unless the phone is in range and awake |
| N4 | Event log persistence in flash | doc 05 (RAM ring buffer, flash is Phase 2) | A power cut loses the cook trace, which is exactly what tuning needs |
| N5 | OTA update | doc 02 section 8 | Otherwise every firmware change is a USB cable at the smoker |
| N6 | Pairing button; BLE passkey is a constant in source and the pairing window opens at boot | doc 05 OPEN items | Anyone in range during the first 60 s after power-up can bond |
| N7 | Blower stall detection | doc 06 E17 | Only exists if a 4-wire fan with tach is chosen. On a 2-wire fan a dead blower shows up as FIRE_OUT 15 min later |

## 6. Edge cases in doc 06 with no implementation yet
- E9 ash choke / lit-mass creep detector (rising average duty over hours): CREEP covers the duty-0 case only.
- E13 stuck sensor: a thermocouple that returns a plausible constant value is not detected. The MAX31856 fault bits catch open, short and range, not a frozen reading. NEW: add a "no change greater than 0.5 C for 10 min while duty is above 20 percent" check. Not in the catalog before this audit.
- E20 water pan gain set: one gain set exists, not two.
- E28 fuel type selector: not implemented (part of N2).
- E23 phone away: see N3.

## 6a. New firmware work created by the blower decision (2026-09-24)
The Delta is a 4-wire brushless fan, so the drive model changes. None of this is built yet.
- PWM polarity inverts (open-drain driver): flag plus inverted LEDC duty.
- PWM frequency fixed at 25 kHz (Intel 4-wire range 21 to 28 kHz). The 25 kHz vs 200 Hz bench comparison in doc 02 no longer applies once the Delta is fitted.
- Datasheet minimum start duty is at most 30 percent, so the long-PWM path stays in use. Measure the real start duty and the lower RUN duty.
- Tach input: pulse counter, RPM estimate. The datasheet FG waveform shows four equal intervals per revolution on a 4-pole motor, which reads as 2 pulses per revolution; NOT confirmed by measurement, so count it on the bench before trusting an RPM figure.
- New fault BLOWER_STALL: duty above 0 with no tach pulses for 3 s.
- New fault BLOWER_RUNAWAY: measured RPM far above the commanded speed for 5 s, which is what a broken PWM lead looks like on this fan. Action: drop SUPPLY_EN (cuts the fan through the IRLZ44N) and fault.
- The IRLZ44N moves from carrying PWM to a fault-only supply cut on its own GPIO.
- pins.h gains TACH and SUPPLY_EN for all three board maps (doc 04).

## 7. Recommended order
1. DONE 2026-09-24/25: B1 bench console, B2 probe offset, B3 git init. Firmware validated on the S3 with the
   simulated probe: IGNITE, RAMP, HOLD, LID_OPEN, over-temp FAULT and fault latching all behaved as designed
   (doc 05, "First hardware bring-up"). Four console bugs found and fixed in the process.
1b. Breadboards ON HAND (2026-09-25 evening). Rails under load (bench step 2a) can start now. Blower work waits on the Delta and the 2N7000s, all ordered 2026-09-25 (doc 07); adapter print in progress, STL in output\3d-print\. Until they arrive the board can still be flashed and driven with the console's simulated probe. Meanwhile: resolve B5, fix B4 so the native tests run locally, and build the 4-wire fan support in section 6a so it is ready when the Delta lands.
2. DONE 2026-09-23/25: boards arrived, bench step 1 complete (flash and run, simulated probe). Bench step 2 (rails and blower under load, M5, real min-start duty) is blocked on the breadboard and the Delta. M5 is now the Delta's running current at 12 V, not the old 6 V blower's.
3. Any time: measure M1, M2, M6; run M3 with the next fire; order the ready-now parts in doc 07 section A/B.
4. When MAX31856 arrives: ice/boiling calibration into B2, then the step-response cook, then gains.
5. Before the first real cook: flap, gasket, exhaust damper, thermostat verification M4, and N3 buzzer if the cook will be unattended.
6. Phase 2: N1 and N2 once the probes and ADS1115 are in hand.
