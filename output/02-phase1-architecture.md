# Charcoal Smoker - Phase 1 Architecture (ESP32-S3 + PWM + BLE)

Created: 2026-09-20
Updated: 2026-09-25 (blower DECIDED as the Delta BFB0712HH-AWPH 4-wire; duct measured; firmware validated on hardware)
Status: Sections 1 to 9 are current. Decisions marked DECIDED are fixed; OPEN items need a measurement or a choice before build.
Platform: stainless drum smoker, no PSU / lights / motor. Only loads: blower + controller.

## 1. Fixed decisions
- DECIDED: single external DC adapter, 12 V 2 A, on hand. All rails derived on the controller board.
- DECIDED: existing Gumsung capillary thermostat (label 03-3203 5, catalog equivalent GS-320S) stays in series in the blower supply as the hardware over-temp cutoff. Dial set above max cook target. It never feeds the controller's supply.
- DECIDED 2026-09-24: the 6 V brushed blower is REPLACED by a Delta BFB0712HH-AWPH, 12 V, 4-wire (logic PWM + tach). Reason in doc 06 section 3: with the measured 22.1 mm duct bore, static pressure decides delivered airflow, not the CFM rating. The old blower stays as the bench load until the Delta arrives.
- DECIDED: ESP32-S3 as MCU (PWM via LEDC, BLE 5 LE; no Bluetooth Classic on S3). Bench board in hand; the WROVER and WROOM maps in doc 04 are fallbacks.
- DECIDED 2026-09-24: bench console over the UART bridge (env s3-n8r8-uart-max31855), not the native USB port. See doc 05.

## 2. Power tree (revised for the Delta)
```
12 V 2 A adapter
  |-- reverse-polarity Schottky (SS34) + 2 A input fuse
  |-- +12 V node
  |       -> thermostat contact (series, hardware cutoff)
  |       -> 1 A fuse -> fan RED (+12 V)
  |       fan BLACK -> IRLZ44N drain -> GND        [fault-only supply cut, NOT PWM]
  |       fan PWM lead  <- 2N7000 open-drain driver <- MCU (25 kHz, duty inverted)
  |       fan TACH lead -> 10 k pull-up to 3.3 V -> MCU pulse counter
  |-- Buck B -> 5 V logic rail (>= 1 A)
          -> ESP32-S3 module (budget 0.5 A peak during BLE TX)
          -> sensor front end, status LED, buzzer (Phase 2)
```
Budget: fan 0.42 A max at 12 V = 5.0 W; logic 0.5 A at 5 V with losses ~3 W. The 12 V 2 A adapter has ample margin.
The 6 V buck (LM2596 A) is deleted: the fan runs on the input rail. Spare bucks stay in the box.

## 3. Safety chain
Hardware, no software in the loop:
1. Thermostat contact in series with the fan supply. Opens above dial setpoint, re-closes 5 to 15 C below (10 +/- 5 C differential). OPEN: identify the pair closed at room temperature and confirm the trip point.
2. Input fuse at the adapter rating; 1 A fuse on the fan branch.
3. MOSFET gate pull-downs so the fan is unpowered while the ESP32 is in reset, booting or unpowered.
4. Optional for unattended cooks: a fixed-temperature bimetal switch or thermal fuse on the drum wall.

Software on top, all implemented and unit-tested:
- max duty clamp (default 60 percent) and a startup cap (50 percent) until the setpoint is first reached;
- sensor fault, watchdog reset, and pit over software limit (200 C) all force the blower OFF;
- FIRE_OUT fault; FUEL_LOW, CREEP and pit-deviation warnings;
- VALIDATED ON HARDWARE 2026-09-25: the over-temp path fired at 200 C and latched, and startCook is refused until the fault is cleared.

New with the 4-wire fan, BUILT 2026-10-04, driving the fan on the bench since 2026-10-10 (doc 05 "S3 fan bench"): this fan runs at FULL speed if its PWM lead is disconnected (datasheet). Detection is the tach; the action is dropping the supply-cut MOSFET, with the thermostat as the backstop. Faults BLOWER_STALL and BLOWER_RUNAWAY are specified in doc 09 section 6a. As built, the supply-cut MOSFET is also off in IDLE, so an idle controller cannot have a running fan whatever the state of the PWM lead. The low-side cut was measured 2026-10-05 (doc 09 M9): with it off, the fan's FG and PWM leads sit at 10.85 V, so the tach lead cannot reach the MCU as drawn in the diagram above. DECIDED 2026-10-09 (Jason): blocking diode in the tach lead, pull-up on the MCU side (doc 04 blower section); not yet built.

## 4. Blower drive
- Delta BFB0712HH-AWPH: 12 V, 0.34 A rated / 0.42 A max, 15.3 CFM free air, 0.98 inH2O max static, 4400 RPM, 75.7 x 75.7 x 25 mm, -10 to 70 C.
- PWM: logic input, 21 to 28 kHz per the Intel 4-wire spec, 25 kHz chosen. Driven open-drain by a 2N7000; the fan pulls the line up internally to at most 5.25 V. Duty is inverted in firmware. Never chop this fan's supply for speed control.
- Rotor stops at 0 percent duty. Minimum start duty is at most 30 percent (datasheet), so the long-PWM path (10 s period at min-start) stays in use for low demand. VALIDATED ON HARDWARE with the simulated probe: at setpoint the loop commanded 1 percent and the HAL applied 0 percent between pulses.
- Tach: open collector, 5 mA max, 4 poles. 2 pulses per revolution CONFIRMED 2026-10-10 (4615 RPM read at 100 percent with ppr 2, spec 4400 +/- 10 percent; doc 05 "S3 fan bench"). Through a 1N5822 and a 10 k pull-up on the MCU side (doc 04, M9).
- The 25 kHz vs 200 Hz frequency comparison in the old plan no longer applies; it was a brushed-motor question.
- Airflow through the measured duct: about 5.4 CFM at full duty, about 53 percent duty to hold 110 C (doc 06 section 3.1a).

## 5. Temperature sensing
Primary: chamber probe at the side port beside the dial thermometer.
- Backend selectable at build time: MAX31856 (default) or MAX31855 (-DPIT_SENSOR_MAX31855). No MAX31855 on hand: both boards failed the shorted-input test and were returned 2026-09-26 (doc 05). MAX31856 received; board 1 passed the doc 09 section 2a inspection (console `i`) on the WROVER 2026-10-01 (doc 05). Board 2 not yet inspected.
- K-type, ungrounded junction required (the drum is metal).
- Sampling 1 Hz, median-of-5, front-end fault bits mapped to SENSOR_OPEN / SENSOR_SHORT / SENSOR_RANGE.
- Probe offset in NVS, set from the ice and boiling check with the console `o <C>` command.
- Mechanical OPEN: probe port thread, and the insertion depth needed to sit at grate level about 8 cm inboard.
Secondary (Phase 2): two ThermoWorks Pro-Series thermistor probes on an ADS1115. Not in the control loop.

## 6. Control loop and state machine
- 1 Hz PID on pit temperature, output = blower duty, in the HeaterMeter form: derivative on a 60 s EMA of the measurement, conditional-integration anti-windup, integral clamped to the active duty cap, integral halved at the first setpoint crossing.
- States: IDLE, IGNITE, RAMP, HOLD, LID_OPEN, RECOVERY, FAULT, MANUAL.
- IGNITE: fixed 50 percent duty until the pit is within 17 C of setpoint, or timeout with no rise -> FAULT.
- RAMP and RECOVERY: PID with duty and integral capped at 50 percent until the setpoint is first reached.
- HOLD: PID up to the 60 percent max duty.
- LID_OPEN: rate trigger (0.3 C/s at cook setpoints, scaled to 1.2 C/s at 260 C) with a fire-out guard, plus a drum-specific rising-lid variant; blower to 0; 30 s minimum; exit on setpoint, plateau or 240 s timeout, into RECOVERY.
- Warnings (never touch the blower): PIT_HIGH, PIT_LOW, FUEL_LOW, CREEP, LID_RISING.
- VALIDATED ON HARDWARE 2026-09-25 with the simulated probe: IGNITE -> RAMP -> HOLD -> LID_OPEN with the correct timings (doc 05).
- Tuning: gains are still placeholders. Derive them from a step-response cook.

## 7. BLE
- ESP32-S3 as GATT server, phone as client. One service, 10 characteristics (pit, setpoint, duty, state, fault, mode, manual duty, event log, device info, warnings). Byte formats in the firmware README.
- Security: bonding + MITM + SC with a static passkey, 60 s pairing window, boot-opened until the pairing button exists. Writes require an encrypted, authenticated link.
- Verified on hardware: NimBLE starts, host syncs, pairing window opens and closes. Phone-side reads and writes NOT yet tested.
- No config characteristics yet: PWM frequency, gains, min-start, max duty, probe offset and lid-detect are console-only. BLE equivalents are Phase 2.

## 8. Firmware stack
- PlatformIO + Arduino-ESP32 3.x (pioarduino). NimBLE-Arduino 2.5.1, Adafruit_MAX31856 1.2.8. Versions and verification in doc 05.
- Modules: sensor driver, PID, state machine, long-PWM, blower driver, BLE service, event log (RAM ring buffer; flash is Phase 2), NVS settings, serial bench console.
- Task watchdog 5 s on the control task; on reset the gate pull-downs guarantee the fan is off.
- Under git as of 2026-09-25.

## 9. Enclosure and mounting
- Controller in a sealed box clamped to the left post beside the thermostat box. Keep the thermostat box; only its contact wiring changes.
- Fan cable: 4 conductors to the duct inlet at the top of the post. Route the PWM and tach lines away from the switched supply lead.
- Duct coupling: 1 in ID hose over the 23.81 mm OD duct with worm clamps, plus a transition from the fan outlet and the backdraft flap inline (doc 04).
- Probe entry via cable gland; thermocouple leads routed away from the fan supply.

## 10. Bring-up order (status 2026-09-25)
1. DONE: flash and run the firmware with no external circuit, using the console's simulated probe.
2. Breadboards on hand (2026-09-25). Rails under load can start now. Blower on the MOSFET, real min-start duty and running current: both arrived 2026-10-02. 2026-10-05: the 12 V input chain is on the breadboard, the Delta runs on straight 12 V (current not reported) and the IRLZ44N supply cut is fitted; no fan lead is on the S3 yet (doc 05 "S3 bench session").
3. Identity check with console `i`: DONE 2026-10-01 on MAX31856 board 1 (WROVER, doc 05). DONE again 2026-10-04 on the S3, which is the bench and first-smoker board from that date (Jason's decision; the WROVER is retired from the bench). Ice and hot-water points taken with the EGT probe: 3.5 C high in ice, 4 to 5 C low at 88.9 C; kept for the first tests with `o 6.5`, replacement to follow (doc 05). Next: board 2 through `i`.
4. BLOCKED: thermostat continuity check and trip test.
5. DONE in console form (MANUAL duty and logging); BLE manual control not yet exercised from a phone.
6. TODO: step-response cook, derive PID gains.
7. TODO: full cook with IGNITE / LID_OPEN / FUEL_LOW / FIRE_OUT observed against a real fire.

## 11. Open items
- Thermostat terminal pair and trip temperature.
- Tach pulses per revolution (spec drawing suggests 2, count it on the bench).
- CLOSED 2026-09-25 from the Delta spec drawing: outlet 24.1 x 17.5 mm, leads RED/BLACK/BLUE/YELLOW bare-ended. Transition piece designed and submitted for print (doc 07 A3, output\3d-print\). Caliper check of the outlet on arrival still owed.
- Blower min-start and minimum RUN duty on the real fan.
- Probe fitting thread, port diameter, insertion depth.
- Exhaust stack: does it have a damper.
- Fan-off draft test result.
- PID gains from a step-response cook.
- CLOSED: framework choice (doc 05), adapter rating, blower choice (doc 06), duct geometry (doc 01), bench console and probe offset (doc 05, doc 09).
