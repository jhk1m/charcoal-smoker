# Charcoal Smoker - HANDOFF

Project: convert a stainless drum charcoal smoker from a mechanical on/off capillary thermostat to a PID-controlled, Bluetooth-capable charcoal smoker on an ESP32-S3.
Owner: Jason. Machine: JASON-PC (Windows 11 Pro).
Repo: C:\Users\wl_de\projects\charcoal-smoker (git; initialised in firmware\ 2026-09-25 00:32, re-rooted to the project directory the same evening so docs, 3D-print files and firmware share one history)
Docs on disk: C:\Users\wl_de\projects\charcoal-smoker\output\
Docs in the Claude project "charcoal-smoker": claude/00-handoff.md through claude/09-open-items-and-gaps.md
Last updated: 2026-10-03

## 1. State in one paragraph
Firmware is written, unit-tested (29/29 native, container run), simulated closed-loop, and validated on the real ESP32-S3 board with no external circuit using a simulated probe. The control core (PID in HeaterMeter form, 8-state machine, lid detection, long-PWM, fault latching, 200 C software over-temp, NVS settings, BLE GATT server, bench serial console) all ran correctly on hardware. Nothing is wired to the S3 yet; bench work since 2026-09-30 is on the Freenove WROVER (doc 04 pin map A, env freenove-wrover). Breadboards are on hand; the Delta blower, the K-type EGT pit probe, two 1N5822 diodes, twenty 2N7000s, the ADS1115 breakout and the CPT-2416-85T buzzer arrived 2026-10-02 (all untested, doc 07); the jumper kit is ordered and not reported as received; the thermostat has not been continuity-checked. The blower-to-duct adapter print is in progress (doc 07 A3, STL in output\3d-print\). Both MAX31855 boards failed the shorted-input test on the WROVER bench (about 23 C low, doc 05) and were returned. The generic MAX31856 2-pack has arrived: board 1 (chip marked MAX31856) passed the doc 09 section 2a inspection (console `i`) on the WROVER 2026-10-01 and reads the bench probe correctly. Ice/boil calibration and board 2 are still owed. The blower decision is final (Delta BFB0712HH-AWPH, 4-wire PWM) but the firmware still drives the old 2-wire model: inverted PWM duty, the tach counter, and the BLOWER_STALL / BLOWER_RUNAWAY faults are specified and not built.

Decisions 2026-10-02/03: backdraft flap deferred for the first tests (duct routed away from the barrel; fan-off draft test decides); thermostat KEPT in series with the fan after a proposal to drop it, only the box and dial go; thermocouple through a second tapped hole in the port block; adapter v1 superseded by v6 direct on measured outlet dimensions.

## 1a. Results as of 2026-09-26 (JASON-PC)
| Check | Result |
|---|---|
| pio run -e s3-n8r8-uart (bench env, MAX31856) | SUCCESS 2026-09-26. RAM 11.7 percent (38408 B of 327680), Flash 19.5 percent (650662 B of 3342336) |
| pio run -e s3-n8r8-uart-max31855 (fallback) | SUCCESS 2026-09-26. RAM 11.7 percent (38360 B), Flash 19.4 percent (648630 B) |
| Console `i` identity check | Built 2026-09-26, both envs. NOT run on hardware, no sensor in hand |
| WROVER bench, MAX31855 shorted-input test | FAIL on both boards, about 23 C low; boards returned (doc 05) |
| pio test -e native | CANNOT RUN on JASON-PC: no host gcc/g++ on PATH (msys64 is present but has no compiler installed). Last pass 29/29 was in the clean container (doc 05). Unblock by installing a MinGW-w64 gcc or running the tests in WSL2 |
| output\3d-print\verify.py | 64000 triangles, 0 non-manifold edges, 0 degenerate triangles, signed volume +9.545 cm3 (outward normals). Cross sections match the KEY table: stub 23.80, bead 25.00, flange 36.00 x 29.00, lip 23.30 x 16.70 |
| Hardware bring-up (simulated probe) | PASS, see section 7 and doc 05 |
| Simulation (doc 08) | unchanged since 2026-09-21 |

## 2. Hardware decisions (fixed)
| Item | Decision |
|---|---|
| MCU | ESP32-S3-DevKitC-1 clone, WROOM-1-N8R8 (YEJMKJ 2-pack, received 2026-09-23) |
| Supply | one 12 V 2 A adapter; 5 V logic rail from an LM2596; no separate blower adapter |
| Blower | Delta BFB0712HH-AWPH, 12 V, 15.3 CFM free air, 0.98 inH2O max static, 4400 RPM, 4-wire PWM + tach. Outlet 24.1 x 17.5 mm (spec drawing), bare 300 mm leads RED 12 V, BLACK GND, BLUE PWM, YELLOW FG. RECEIVED 2026-10-02, label "BFB0712HH-A DC12V 0.42A"; -AWPH and four leads confirmed by Jason. Order the exact string, BFB0712H is a different 2-wire fan (doc 07 A1a) |
| Blower adapter | CURRENT 2026-10-02: v6 "direct", one piece, socket over the fan outlet (measured 39.90 x 25.00 mm outside, bore 35.90 x 17.50 mm, 2.25 mm off centre) and socket over the 23.81 mm duct, no hose; spec in output\3d-print\README-blower-adapter-v6.md. The v6 STL is NOT in the repo, its print status is unrecorded, and it leaves the backdraft flap with no location. SUPERSEDED v1 follows. Printed transition, fan outlet to a 23.8 mm OD stub so the same 1 in ID hose and worm clamps fit both ends. output\3d-print\blower-adapter-delta-BFB0712HH-to-1in-duct-v1.stl, submitted 2026-09-25 to Arlington Public Library The Shop in PLA, print in progress. PLA is fit-check and first-cook only; reprint PETG or ABS if the duct inlet runs warm |
| Fan drive | 2N7000 open-drain on the PWM lead at 25 kHz, duty inverted. IRLZ44N in the fan BLACK lead as a fault-only supply cut. Never chop a brushless fan's supply for speed |
| Over-temp backstop | existing Gumsung capillary thermostat (label 03-3203 5, catalog GS-320S) stays in series with the fan supply, dial above max cook target. RE-DECIDED 2026-10-03 (Jason, after a 2026-10-02 proposal to remove it): KEEP the thermostat switch, capillary and bulb in series with the fan supply; the stainless box and dial thermometer can go, the switch moves into the new enclosure. Reason: the software limit only sees the probe, so it cannot catch a dislodged probe, a hung firmware with PWM latched, or a broken PWM lead (full speed on this fan); the thermostat has its own bulb. Consequence: the bulb keeps the drum's side port; the K-type probe enters through a second 1/8-27 NPT hole tapped in the same stainless port block beside the brass fitting (decided 2026-10-03, conditions and fallback in doc 09 M6). M4 continuity check stays owed |
| Pit sensor | K-type ungrounded on a MAX31856. Barrel probe: the $3.80 EGT probe, 1/8 NPT compression, 3.2 in, received 2026-10-02, untested (continuity, ice/boil, reach), Jason considers it questionable; fallback Procon B09HSJ4PBB (doc 03). Mounts through a second tapped hole in the existing port block (doc 09 M6). Board 1 of the 2-pack PASSED the doc 09 section 2a inspection on the WROVER 2026-10-01 (register readback, open bit, shorted input 0.11 C; doc 05); board 2 not yet tested; ice/boil offset not yet set. NO MAX31855 ON HAND: both HUABAN boards failed the shorted-input test on the WROVER (read 2.0 to 2.5 C against a 25.1 C cold junction, about 23 C low, expected within about 2 C; wiring and supply verified) and were returned 2026-09-26 (doc 05 "WROVER bench session", doc 03, doc 09 M8). The MAX31855 backend stays as an untested fallback |
| Duct | 23.81 mm OD, 0.85 mm wall, 22.1 mm bore, about 20 in run, two 90 degree bends; couples with a 1 in ID sleeve, about 1 in engagement |
| Backdraft flap | required, and must crack at 1 to 2 gram-force (20 to 50 Pa). Spring and swing check valves cannot be used. DEFERRED by Jason 2026-10-02: the air duct is routed away from the barrel, so no flap for the first tests, and adapter v6 has none. The duct routing answers heat reaching the fan; whether natural draft through a stopped fan costs control authority is still decided by the fan-off draft test (section 8 item 5, doc 09 M3) |
| Meat probes (Phase 2) | ThermoWorks Pro-Series thermistors + ADS1115, not K-type |

## 3. Document map
| Doc | Contents |
|---|---|
| 00-handoff.md | this file |
| 01-current-system-baseline.md | as-built unit, vendor replies, section 7a air path (intake is the side duct, the fire door is charcoal/ash only) |
| 02-phase1-architecture.md | power tree, safety chain, blower drive, sensing, state machine, BLE, bring-up order, open items |
| 03-phase1-bom-first-buy.md | first purchase list, arrival status, items superseded by the Delta decision |
| 04-phase1-wiring.md | pin maps per board and the 4-wire fan wiring |
| 05-firmware-framework.md | toolchain, library versions, change log, first hardware bring-up record |
| 06-research-blower-edge-cases-cook-algorithm.md | the research base: proposals P1 to P9, charcoal rules, blower requirements R1 to R16 with the duct pressure maths, edge cases E1 to E29, meat science, Cook Manager spec, sources |
| 07-second-buy-parts.md | second purchase list (blower, fan drive parts, coupling, flap, breadboard, Phase 2 probes) |
| 08-blower-simulation.md | closed-loop simulation, six scenarios, the two firmware bugs it found |
| 09-open-items-and-gaps.md | gap register: what is done, what is measured, what is not built |
| 3d-print/README-blower-adapter-v1.md | adapter dimensions with sources, print settings, PLA caveat, fit procedure; gen_adapter.py regenerates the STL, verify.py checks it |

## 4. Build, flash, monitor
Windows PowerShell, PlatformIO CLI, from C:\Users\wl_de\projects\charcoal-smoker\firmware.

The board exposes two USB ports. The CH343 UART bridge (VID 1A86) is the console port; the native USB port (VID 303A) is not, because the working env sets ARDUINO_USB_CDC_ON_BOOT=0. Find the port first:

    pio device list

Build, flash and open the console (substitute the actual COM number, it changes after a manual download):

    pio run -e s3-n8r8-uart -t upload --upload-port COM6
    pio device monitor -e s3-n8r8-uart --port COM6

On the WROVER (on the bench as of 2026-10-01, COM3 on 2026-09-30 and 10-01). This env has no echo, so the monitor flags below are needed to see typed commands:

    pio run -e freenove-wrover -t upload --upload-port COM3
    pio device monitor -e freenove-wrover --port COM3 --echo -f time -f send_on_enter

If upload fails with "No serial data received": hold BOOT, tap RESET, release BOOT, then re-run pio device list because the port re-enumerates.

Unit tests (host, no board). Needs a host gcc/g++ on PATH; JASON-PC has none as of 2026-09-25, so this only runs in the container or WSL2:

    pio test -e native

Simulation: see firmware\sim\README.md (same host-compiler requirement).

Envs: freenove-wrover (MAX31856) is the env in use on the bench as of 2026-10-01. s3-n8r8-uart (MAX31856, added 2026-09-26) is the S3 bench env; s3-n8r8-uart-max31855 was the bench env while MAX31855 boards existed and still works with the simulated probe. Both put the console on the CH343. s3-n8r8 and s3-n8r8-max31855 route Serial to the native USB port. wroom32* and freenove-wrover-max31855 are fallbacks. native is the test env. default_envs = s3-n8r8-uart.

## 5. Bench console
Type into the monitor and press Enter. The env enables the send_on_enter filter and echo. Every accepted line is echoed back as `# > <line>`.

    ?            help
    d            dump settings and state
    v            toggle the CSV stream
    r            reset NVS to defaults and reboot
    s <C>        setpoint
    c            start cook       x  stop       k  clear fault
    m <pct>      manual mode at duty, m with no value exits to stop
    g <kp> <ki> <kd>   PID gains, saved to NVS
    f <hz>       blower PWM frequency
    n <pct>      blower min start duty
    X <pct>      blower max duty
    o <C>        probe offset (ice and boiling check)
    l <0|1>      lid detection on/off
    i            sensor identity check (doc 09 section 2a): run with the probe open, then with T+ shorted to T-
    t <C>        simulate the probe, no sensor needed;  t off  returns to the sensor
    t <C> <C/s>  simulate with a ramp, e.g.  t 25 0.5

CSV columns:

    t_s,state,fault,pit_raw_c,pit_filt_c,cj_c,duty_cmd,duty_applied,tc_fault_bits,warnings,out_ema,rate_c_s

Running the whole loop with no wiring: flash, then `t 25`, `s 110`, `c`, and watch IGNITE -> RAMP -> HOLD. `t 200` forces the software over-temp fault; `k` clears it.

## 6. Control design (what is implemented)
- 1 Hz PID on pit temperature, HeaterMeter form: derivative taken on a 60 s EMA of the measurement (alpha 2/61), conditional integration for anti-windup, integral clamped to the active duty cap, integral halved at the first setpoint crossing. Output EMA alpha 2/241 for the fuel and creep logic.
- States: IDLE, IGNITE, RAMP, HOLD, LID_OPEN, RECOVERY, FAULT, MANUAL.
- IGNITE at a fixed 50 percent until within 17 C of setpoint. RAMP and RECOVERY capped at 50 percent duty and integral until the setpoint is first reached. HOLD up to the 60 percent max duty cap.
- LID_OPEN on a falling-rate trigger (0.3 C/s, scaled with setpoint) with a fire-out guard, blower to 0, 30 s minimum, exit on setpoint, plateau or 240 s timeout into RECOVERY.
- Long-PWM: for demand below the blower's minimum spin duty, a 10 s period at min-start duty, plus a 300 ms full-duty kick at every start from rest.
- Warnings, which never touch the blower: PIT_HIGH, PIT_LOW, FUEL_LOW (latched, clears after 300 s clear), CREEP, LID_RISING. Faults: sensor open/short/range, over-temp, ignite timeout, FIRE_OUT.
- Charcoal-specific behaviour that drove these choices is in doc 06: lit-mass creep after a high-demand period, the forge effect, backdraft through the duct with no flap, fuel-exhaustion warning ahead of fire-out.

## 7. Validated on hardware 2026-09-25
Boot, LEDC, NVS load and save, task watchdog, NimBLE start and pairing window, sensor-absence detection, the simulated-probe loop, IGNITE -> RAMP -> HOLD -> LID_OPEN with correct timings, the 200 C software over-temp fault and its latching, and refusal of startCook while a fault is latched. Detail and the timing table are in doc 05.

## 8. Blocked, in the order that unblocks the most
Purchasing status 2026-09-25 (doc 07, confirmed by Jason): everything for the first cook is ordered, Mouser lines, local hose and clamps, flap sheet, and the adapter print. Nothing left to buy until a measurement forces it (doc 07 "Still blocked").
1. DONE: breadboards on hand (doc 07 A13). Bench step 2a, rails under load, can start now; the jumper kit is in transit.
2. Pit sensor: MAX31856 2-pack received. Board 1 PASSED the doc 09 section 2a inspection 2026-10-01 on the WROVER (env freenove-wrover, COM3; doc 05). Still owed: board 2 through the same inspection, then ice and boiling water and `o <C>` for the offset (section 2a step 7). The freenove-wrover env has no echo: open the monitor with `--echo -f time -f send_on_enter` to see typed commands. Fail = buy the Adafruit 3263. Bench probe polarity: crimped bare lead is T- (doc 04).
2b. Delta blower (doc 07 A1): RECEIVED 2026-10-02, untested. Fan drive 2N7000s (A2): RECEIVED 2026-10-02, 20 pieces. 1 in ID hose and clamps (A3), flap silicone sheet (A4): ordered, not reported as received. Printed adapter (A3): print in progress, STL in output\3d-print\. 2026-10-02: the outlet is NOT 24.1 x 17.5 mm (measured 39.90 x 25.00 outside, 35.90 x 17.50 bore), so the v1 adapter is superseded by v6 direct (README-blower-adapter-v6.md); get the v6 STL into the repo and decide where the flap goes. OSOYOO S3 IO breakout (doc 07 A14): proposed, order status unrecorded.
2c. Received 2026-10-02, untested: K-type EGT pit probe, 1/8 NPT compression fitting, 3.2 in (junction type unknown: check sheath-to-lead continuity before it goes in the drum, grounded = Procon fallback, doc 03); 1N5822 x2 (optional insurance for the reverse-polarity diode, doc 07 A8), ADS1115 breakout (Phase 2, waits on the ThermoWorks probes), CPT-2416-85T buzzer (doc 09 N3; needs a 2N7000 and a pin in pins.h).
3. Thermostat continuity check: which of the three spade tabs is the pair closed at room temperature, and where it actually trips.
4. Measurements still missing: probe port thread and diameter, insertion depth to reach grate level about 8 cm inboard, tach pulses per revolution, exhaust stack OD and whether it has a damper, fire door seal condition. Delta outlet dimensions are now taken from the spec drawing (doc 07 A1), pending the caliper check.
5. Fan-off draft test: light the fire, leave the blower unplugged, close the door, and see whether the pit climbs past 120 C on its own. This decides how urgent the flap and the exhaust damper are. The simulation says a missing flap costs all control authority (mean duty under 1 percent).
6. PID gains from a step-response cook. Current gains are placeholders that the simulation says are safe, not tuned.
7. Field-test path agreed 2026-10-03: the ordered bench and smoker list is doc 09 section 7 item 4b. The one item that blocks assembly is the 4-wire fan firmware (section 9), not parts. Board for the first smoker test (WROVER or S3) not yet chosen.

## 9. Not built (next firmware work, doc 09 section 6a)
- 4-wire fan support: inverted PWM duty, tach input on a pulse counter, BLOWER_STALL (commanded above min-start, no tach) and BLOWER_RUNAWAY (tach with zero command, action: drop the supply-cut MOSFET), fan-RPM in the dump and the CSV.
- BLOWER_MIN_RUN_DUTY_PCT, only after the real fan's start and run duties are measured, so a spinning fan is not stopped for a small request.
- Cook Manager (Phase 2): meat probes on an ADS1115, stall handling, done-and-hold, the juiciness logic in doc 06 section 6.
- BLE config characteristics for gains, caps, offset and lid detect. Currently console-only.
- Pairing button, buzzer, flash-persisted event log, OTA.

## 10. Standing constraints for anyone continuing this
- Approved directories: C:\Users\wl_de\projects\, C:\Users\wl_de\OneDrive\, C:\Temp\, D:\, J:\, X:\, Z:\. Temp output goes to C:\Temp.
- Never touch credential stores, .env files, keys, the registry, or system directories.
- Do not run git commands through the Claude device bridge. The bridge cannot delete files, so a stale .git\index.lock has to be cleared by hand. Run git in PowerShell or Claude Code CLI on the machine. The repo root is the project directory, not firmware\.
- Native unit tests and the simulation need a host g++ (doc 05). None is installed on JASON-PC.
- No invented commands, pins, part numbers, settings or UI labels. If something is unverified, say so and name where it would be verified.

## 11. Where the record lives (added 2026-09-26)
Remote: https://github.com/jhk1m/charcoal-smoker (branch main). The repo was re-rooted from firmware/ to the
project directory on 2026-09-25, so firmware, the docs in output/ and this file are all one repo.

Two Claude sessions work on this project: Claude Code CLI on JASON-PC, which runs the builds, flashes and bench
tests, and a Cowork session, which does research, part selection and doc maintenance. They do not share memory.
The repo is the only thing both can see, so anything learned in either session is worthless until it lands here.

Where each kind of finding goes:
- bench and hardware results, including parts that test bad: doc 05 (bring-up section) and doc 09.
- anything that changes what to buy or what a part is: doc 07, and doc 03 if it is a first-buy item.
- wiring and pin changes: doc 04.
- decisions: doc 02, and the one-line summary here.

Git is run by Jason or by the CLI session in PowerShell, never through the Cowork device bridge: that bridge
cannot delete files, so a stale .git\index.lock has to be cleared by hand. The Cowork session edits files in the
working tree and leaves committing alone.

CLOSED 2026-09-26: the MAX31855 finding is recorded. From Jason: BOTH HUABAN boards, tested on the Freenove WROVER
with T+ shorted to T-, read the thermocouple more than 21 C away from the cold-junction reading (a good board agrees
within about 2 C). Both returned. Doc 03 and doc 07 corrected, doc 09 M8 closed. The bench pit sensor is now the
MAX31856 when it arrives; env s3-n8r8-uart added for it.
