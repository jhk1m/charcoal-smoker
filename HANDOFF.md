# Charcoal Smoker - HANDOFF

Project: convert a stainless drum charcoal smoker from a mechanical on/off capillary thermostat to a PID-controlled, Bluetooth-capable charcoal smoker on an ESP32-S3.
Mission (Jason, 2026-10-22): perfect BBQ every time, juicy every time, whether indirect low-and-slow convection or short direct cooks of 20 to 60 min (pork belly class). Sear DROPPED 2026-10-22 (Jason); short cooks stay: dynamic PIT_OVER_LIMIT and a hard cutoff decided, numbers confirmed and the firmware side built 2026-10-22 (SETPOINT_MAX 200, limit = setpoint + 40 capped at 240, cook timer `a <min>` / BLE 000c; doc 05 "Short cooks build", doc 06 section 6a, doc 09 N8); dial to 270 C and M4 still owed; G4 thermal fuse decided yes (doc 07 A14, M10 wall temperature first); manual mode set aside (N9).
Owner: Jason. Machine: JASON-PC (Windows 11 Pro).
Repo: C:\Users\wl_de\projects\charcoal-smoker (git; initialised in firmware\ 2026-09-25 00:32, re-rooted to the project directory the same evening so docs, 3D-print files and firmware share one history)
Docs on disk: C:\Users\wl_de\projects\charcoal-smoker\output\
Docs in the Claude project "charcoal-smoker": claude/00-handoff.md through claude/09-open-items-and-gaps.md
Last updated: 2026-10-22

## 0. Start here (2026-10-22)
Bench: S3 on COM6, all four Delta leads wired per doc 04 map C, MAX31856 board 1 with the EGT probe live, fan 12 V plugged in, board IDLE, `n 15`, `X 60`, `o 6.5`. Flashed build = the 2026-10-22 BLE debug working tree on top of commit 993933e (doc 05 "BLE debug channels": console over BLE 000d, CSV telemetry 000e, the WRITE-flag fix that makes BLE writes work for the first time, timer minutes-left published on 000c; BLE passkey in the git-ignored `firmware\.env` since 2026-10-23, the build refuses to run without it, see `firmware\.env.example`; `firmware/tools/pair.py` and `ble.py` are the PC client, `tools/ui.html` the Web Bluetooth UI, proven from Chrome on JASON-PC 2026-10-22, copy at `docs/index.html` for GitHub Pages so the iPhone can open it in Bluefy over https; JASON-PC is bonded), on top of the short-cooks build (doc 05 "Short cooks build": SETPOINT_MAX 200, over-limit setpoint + 40 capped 240, cook timer `a <min>`), which carries: BLOWER_MIN_RUN_DUTY_PCT = 10 with long-PWM hysteresis, long-PWM on-phase at least 5 s, long-PWM off-phase cuts the fan supply, FAN_STALL_S 5. Reason: a false BLOWER_STALL at setpoint, traced on the bench to the Delta restarting late from a PWM-lead stop (doc 05 "S3 bench session 2026-10-22"). Bench-checked: min-run pass-through, three cold starts in 235 s at setpoint all on the first tick, full state-machine pass, 41/41 tests, all five envs build. Fan 12 V is PLUGGED IN. Everything in doc 09 section 6a is done except the kick on a scope; BLOWER_STALL under the supply-cut model re-proven 2026-10-22 by three pulls (dead fan, lost tach at steady 10 percent, lost tach inside long-PWM `m 5`; doc 05 "Short cooks build" item 6). EGT tests and the `o 6.5` compensation are treated as final (Jason, 2026-10-22).
Next, in order: (1) DONE 2026-10-22, BLOWER_STALL re-proven; (2) SKIPPED by Jason 2026-10-22 (board 2 is a spare, inspect only if board 1 dies); (3) thermostat continuity, doc 09 M4; (4) port block solid or hollow, doc 09 M6; (5) first fire: fan-off draft test, offset check at the grate, step-response cook for gains.
Console is scripted from a pyserial helper (DTR/RTS held low, commands with 1 s gaps) kept in the CLI session scratchpad, not the repo; `pio device monitor` works the same by hand. Since 2026-10-22 the same console runs over BLE: `python firmware\tools\ble.py d` from JASON-PC (bonded; a new PC or phone pairs through `tools/pair.py` or by typing the passkey within 60 s of a reset). Full history follows.

## 1. State in one paragraph
Firmware is written, unit-tested (29/29 native, container run), simulated closed-loop, and validated on the real ESP32-S3 board with no external circuit using a simulated probe. The control core (PID in HeaterMeter form, 8-state machine, lid detection, long-PWM, fault latching, 200 C software over-temp, NVS settings, BLE GATT server, bench serial console) all ran correctly on hardware. The bench and first-smoker board is the S3-N8R8 as of 2026-10-04 (Jason; doc 04 pin map C, env s3-n8r8-uart). The Freenove WROVER was the bench board 2026-09-30 to 10-01 and is no longer used: give no WROVER pins or commands. MAX31856 board 1 is wired to the S3 pins and passed console `i` there 2026-10-04, open and shorted input (doc 05 "S3 bench session"); the 4-wire build boots on the bare S3. Breadboards are on hand; the Delta blower, the K-type EGT pit probe, two 1N5822 diodes, twenty 2N7000s, the ADS1115 breakout and the CPT-2416-85T buzzer arrived 2026-10-02 (doc 07; the blower and the EGT probe have been bench-tested since, see below; the rest untested); the jumper kit is ordered and not reported as received; the thermostat has not been continuity-checked. The blower-to-duct adapter print is in progress (doc 07 A3, STL in output\3d-print\). Both MAX31855 boards failed the shorted-input test on the WROVER bench (about 23 C low, doc 05) and were returned. The generic MAX31856 2-pack has arrived: board 1 (chip marked MAX31856) passed the doc 09 section 2a inspection (console `i`) on the WROVER 2026-10-01 and reads the bench probe correctly. Ice and hot-water points were taken on the S3 2026-10-04 with the EGT probe: 3.5 C high in ice, 4 to 5 C low at 88.9 C, offset set to +6.5 C as an extrapolation (section 2, doc 05). Board 2 is still owed. The blower decision is final (Delta BFB0712HH-AWPH, 4-wire PWM) and since 2026-10-04 the firmware drives that model: inverted PWM duty, the SUPPLY_EN cut, the tach counter, and the BLOWER_STALL / BLOWER_RUNAWAY faults are built, build-verified on all five ESP32 envs and unit-tested (39/39 under WSL2), and since 2026-10-10 are VALIDATED ON THE FAN (doc 05 "S3 fan bench"): all four leads on the S3, 4615 RPM at 100 percent, ppr 2 confirmed, duty sweep 10 to 100 percent taken, BLOWER_RUNAWAY and BLOWER_STALL both proven with the fan running; M5 current still owed. With the supply cut the fan's YELLOW and BLUE leads sit at 10.85 V (doc 09 M9), so the tach lead needs a fix before it meets a GPIO; DECIDED 2026-10-09 (Jason): blocking diode in the YELLOW lead, pull-up on the GPIO side (doc 04 blower section), not yet built. On a bare board a cook now stops on BLOWER_STALL unless `T 0` is typed first. 2026-10-09: two driver bugs fixed (doc 05 "Two driver fixes"): the start kick now ends at 300 ms, and an unpowered MAX31856 reads NOT DETECTED instead of a false `present`; flashed 2026-10-10. Second bench session 2026-10-10 (doc 05): M5 DONE, 0.27 A at 100 percent; 10 percent start 4 of 4, `n 15` saved; the sensor fix was incomplete for a chip unpowered AFTER boot (still `present`, 0.00 C taken as valid), fixed again in `selfTest()` and `read()`, flashed, proven: `i` NOT DETECTED, cook faults SENSOR_OPEN at once. Bench left: sensor live, fan 12 V unplugged.

Decisions 2026-10-02/03: backdraft flap deferred for the first tests (duct routed away from the barrel; fan-off draft test decides); thermostat KEPT in series with the fan after a proposal to drop it, only the box and dial go; thermocouple through a second tapped hole in the port block; adapter v1 superseded by v6 direct on measured outlet dimensions.

## 1a. Results as of 2026-09-26 (JASON-PC)
| Check | Result |
|---|---|
| pio run -e s3-n8r8-uart (bench env, MAX31856) | SUCCESS 2026-09-26. RAM 11.7 percent (38408 B of 327680), Flash 19.5 percent (650662 B of 3342336) |
| pio run -e s3-n8r8-uart-max31855 (fallback) | SUCCESS 2026-09-26. RAM 11.7 percent (38360 B), Flash 19.4 percent (648630 B) |
| Console `i` identity check | Built 2026-09-26. Run on the WROVER 2026-10-01 and on the S3 2026-10-04, board 1 PASS both times (doc 05). Flaw FIXED 2026-10-09 (type K readback required in `begin()`), flashed 2026-10-10; bench showed it incomplete for VIN pulled after boot; FIXED AGAIN and PROVEN 2026-10-10 (doc 05 "second session") |
| WROVER bench, MAX31855 shorted-input test | FAIL on both boards, about 23 C low; boards returned (doc 05) |
| pio test -e native | CANNOT RUN on JASON-PC: no host gcc/g++ on PATH (msys64 is present but has no compiler installed). Last pass 29/29 was in the clean container (doc 05). Unblock by installing a MinGW-w64 gcc or running the tests in WSL2 |
| output\3d-print\verify.py | 64000 triangles, 0 non-manifold edges, 0 degenerate triangles, signed volume +9.545 cm3 (outward normals). Cross sections match the KEY table: stub 23.80, bead 25.00, flange 36.00 x 29.00, lip 23.30 x 16.70 |
| Hardware bring-up (simulated probe) | PASS, see section 7 and doc 05 |
| Simulation (doc 08) | unchanged since 2026-09-21 |
| 2026-10-04, 4-wire fan support: pio run | SUCCESS on s3-n8r8-uart (RAM 38440 B, Flash 658118 B, 19.7 percent), s3-n8r8, s3-n8r8-uart-max31855, freenove-wrover, wroom32. No warnings |
| 2026-10-04, unit tests | 39 Tests 0 Failures under WSL2 Ubuntu (g++ 13.3.0, compiled directly, not through pio; doc 05). `pio test -e native` on Windows still cannot run |
| 2026-10-04, 4-wire build on the bare S3 (COM6, env s3-n8r8-uart) | Flash SUCCESS; boots, LEDC on pin 4, `d` prints the fan line, supply=0 in IDLE. Tach faults not exercised (`T 0`) |
| 2026-10-04, MAX31856 board 1 on the S3 pins, console `i` | PASS open and shorted (delta -0.20 / -0.23 C). All zeros at first; cause: sensor VIN and GND not properly connected |
| 2026-10-04, EGT probe on the S3 | Ungrounded, yellow = T+. Ice 3.8 C against a 0.3 C reference; hot 83.7 to 84.9 C against 88.9 C. Offset +6.5 C set (extrapolated, not measured at cook temperature) |
| 2026-10-05, Delta on straight 12 V | Runs with BLUE and YELLOW open. Running current taken 2026-10-10 under firmware: 0.27 A at 100 percent |
| 2026-10-05, doc 09 M9 | YELLOW and BLUE at 10.85 V against ground with the supply cut, the same into 10 k. The tach lead cannot go straight to a GPIO; fix decided 2026-10-09, built and proven 2026-10-10 (two rows down) |
| 2026-10-10, 4-wire fan support driving the Delta (doc 05 "S3 fan bench") | RUNS. All four fan leads on the S3 per doc 04 map C. 4615 RPM at 100 percent with ppr 2 (spec 4400 +/- 10 percent, so 2 pulses per rev CONFIRMED). Duty sweep: 60 3250, 50 2817, 40 2338, 30 1780, 20 1160, 10 480, 5 stops. Starts from rest at 10 percent with the kick. BLOWER_RUNAWAY PASS (BLUE opened at 30 percent, fault in 5 s, supply cut) and BLOWER_STALL PASS (YELLOW pulled at 30 percent, fault in 3 s, supply cut). 4-wire firmware validated on the Delta. Owed: M5 current, lower `n` |
| 2026-10-09, driver fixes (kick length, unpowered MAX31856): pio run | SUCCESS on s3-n8r8-uart (RAM 38440 B, Flash 657962 B, 19.7 percent), s3-n8r8, s3-n8r8-uart-max31855, freenove-wrover, wroom32. No warnings. Flashed 2026-10-10; unit tests not affected (doc 05) |
| 2026-10-10, second bench session (doc 05) | M5: 0.27 A at 100 percent, 4585 RPM. 10 percent start from rest 3 of 3 (4 of 4 total), 5 percent dies in 5 s; `n 15` saved. Sensor unpowered after boot: FAIL on the 10-09 build (present, 0.00 C valid); fixed in `selfTest()` (always re-probe) and `read()` (all-zero reading = SENSOR_OPEN, present_ cleared); pio run SUCCESS on all five envs (s3-n8r8-uart Flash 657994 B), no warnings; flashed; `i` NOT DETECTED, cook faults SENSOR_OPEN at once, `i` after VIN restored brings the sensor back |

## 2. Hardware decisions (fixed)
| Item | Decision |
|---|---|
| MCU | ESP32-S3-DevKitC-1 clone, WROOM-1-N8R8 (YEJMKJ 2-pack, received 2026-09-23) |
| Supply | one 12 V 2 A adapter; 5 V logic rail from an LM2596; no separate blower adapter |
| Blower | Delta BFB0712HH-AWPH, 12 V, 15.3 CFM free air, 0.98 inH2O max static, 4400 RPM, 4-wire PWM + tach. Outlet 24.1 x 17.5 mm (spec drawing), bare 300 mm leads RED 12 V, BLACK GND, BLUE PWM, YELLOW FG. RECEIVED 2026-10-02, label "BFB0712HH-A DC12V 0.42A"; -AWPH and four leads confirmed by Jason. Order the exact string, BFB0712H is a different 2-wire fan (doc 07 A1a) |
| Blower adapter | CURRENT 2026-10-02: v6 "direct", one piece, socket over the fan outlet (measured 39.90 x 25.00 mm outside, bore 35.90 x 17.50 mm, 2.25 mm off centre) and socket over the 23.81 mm duct, no hose; spec in output\3d-print\README-blower-adapter-v6.md. The v6 STL, its generator gen_v6.py and blower-adapter-v6-SPEC.md are in output\3d-print\ (committed 2026-10-05); its print status is unrecorded, and it leaves the backdraft flap with no location. SUPERSEDED v1 follows. Printed transition, fan outlet to a 23.8 mm OD stub so the same 1 in ID hose and worm clamps fit both ends. output\3d-print\blower-adapter-delta-BFB0712HH-to-1in-duct-v1.stl, submitted 2026-09-25 to Arlington Public Library The Shop in PLA, print in progress. PLA is fit-check and first-cook only; reprint PETG or ABS if the duct inlet runs warm |
| Fan drive | 2N7000 open-drain on the PWM lead at 25 kHz, duty inverted. IRLZ44N in the fan BLACK lead as a fault-only supply cut. Never chop a brushless fan's supply for speed. MEASURED 2026-10-05 (doc 09 M9): with that low-side cut off, the fan's YELLOW and BLUE leads sit at 10.85 V, so the tach lead must not go straight to a GPIO. DECIDED 2026-10-09 (Jason): blocking diode in the YELLOW lead, cathode toward the fan, 10 k pull-up and 10 k series on the GPIO side (doc 04); meter check of the GPIO-side node before GPIO5 is connected. BUILT 2026-10-10 (1N5822 in YELLOW, 3.2 V at the tach row with the supply cut), in use on the bench |
| Over-temp backstop | existing Gumsung capillary thermostat (label 03-3203 5, catalog GS-320S) stays in series with the fan supply, dial above max cook target. RE-DECIDED 2026-10-03 (Jason, after a 2026-10-02 proposal to remove it): KEEP the thermostat switch, capillary and bulb in series with the fan supply; the stainless box and dial thermometer can go, the switch moves into the new enclosure. Reason: the software limit only sees the probe, so it cannot catch a dislodged probe, a hung firmware with PWM latched, or a broken PWM lead (full speed on this fan); the thermostat has its own bulb. Consequence: the bulb keeps the drum's side port; the K-type probe enters through a second 1/8-27 NPT hole tapped in the same stainless port block beside the brass fitting (decided 2026-10-03, conditions and fallback in doc 09 M6). M4 continuity check stays owed |
| Pit sensor | K-type ungrounded on a MAX31856. Barrel probe: the $3.80 EGT probe (1/8 NPT compression, 3.2 in, received 2026-10-02) is KEPT FOR THE FIRST TESTS, replacement to follow (Jason, 2026-10-04, after first rejecting it the same day): ungrounded and correct polarity, but on the S3 it read 3.5 C high in ice and 4 to 5 C low at 88.9 C against a reference thermometer (doc 05 "S3 bench session"). Probe offset set to +6.5 C in NVS on the S3 (`o 6.5`), an extrapolation to a 110 C cook, NOT measured there: check against the reference thermometer at the grate on the first fire. The documented replacement is the Procon B09HSJ4PBB (doc 03), not yet ordered. NOT confirmed that the probe and not the board is at fault: the bench-probe control in the same bath was not run. Mounts through a second tapped hole in the existing port block (doc 09 M6). Board 1 of the 2-pack PASSED the doc 09 section 2a inspection on the WROVER 2026-10-01 (register readback, open bit, shorted input 0.11 C; doc 05); board 2 not yet tested; ice/boil offset not yet set. NO MAX31855 ON HAND: both HUABAN boards failed the shorted-input test on the WROVER (read 2.0 to 2.5 C against a 25.1 C cold junction, about 23 C low, expected within about 2 C; wiring and supply verified) and were returned 2026-09-26 (doc 05 "WROVER bench session", doc 03, doc 09 M8). The MAX31855 backend stays as an untested fallback |
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

Build, flash and open the console (the S3 was COM6 on 2026-10-04; substitute the actual COM number, it changes after a manual download):

    pio run -e s3-n8r8-uart -t upload --upload-port COM6
    pio device monitor -e s3-n8r8-uart --port COM6

On the WROVER (no longer the bench board since 2026-10-04; COM3 on 2026-09-30 and 10-01). This env has no echo, so the monitor flags below are needed to see typed commands:

    pio run -e freenove-wrover -t upload --upload-port COM3
    pio device monitor -e freenove-wrover --port COM3 --echo -f time -f send_on_enter

If upload fails with "No serial data received": hold BOOT, tap RESET, release BOOT, then re-run pio device list because the port re-enumerates.

Unit tests (host, no board). Needs a host gcc/g++ on PATH; JASON-PC has none as of 2026-09-25, so this only runs in the container or WSL2:

    pio test -e native

Simulation: see firmware\sim\README.md (same host-compiler requirement).

Envs: s3-n8r8-uart (MAX31856, added 2026-09-26) is the env in use on the bench since 2026-10-04; freenove-wrover (MAX31856) was the bench env 2026-09-30 to 10-01; s3-n8r8-uart-max31855 was the bench env while MAX31855 boards existed and still works with the simulated probe. Both put the console on the CH343. s3-n8r8 and s3-n8r8-max31855 route Serial to the native USB port. wroom32* and freenove-wrover-max31855 are fallbacks. native is the test env. default_envs = s3-n8r8-uart.

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
    p <n>        fan tach pulses per revolution (default 2, confirmed on the Delta 2026-10-10), saved to NVS
    T <0|1>      fan tach faults off/on; NOT saved, on again at every boot. T 0 = board with no fan
    i            sensor identity check (doc 09 section 2a): run with the probe open, then with T+ shorted to T-
    t <C>        simulate the probe, no sensor needed;  t off  returns to the sensor
    t <C> <C/s>  simulate with a ramp, e.g.  t 25 0.5

CSV columns:

    t_s,state,fault,pit_raw_c,pit_filt_c,cj_c,duty_cmd,duty_applied,tc_fault_bits,warnings,out_ema,rate_c_s,fan_rpm

Running the whole loop with no wiring: flash, then `T 0` (no fan means no tach, and without this the cook stops on BLOWER_STALL after 3 s), `t 25`, `s 110`, `c`, and watch IGNITE -> RAMP -> HOLD. `t 200` forces the software over-temp fault; `k` clears it.

## 6. Control design (what is implemented)
- 1 Hz PID on pit temperature, HeaterMeter form: derivative taken on a 60 s EMA of the measurement (alpha 2/61), conditional integration for anti-windup, integral clamped to the active duty cap, integral halved at the first setpoint crossing. Output EMA alpha 2/241 for the fuel and creep logic.
- States: IDLE, IGNITE, RAMP, HOLD, LID_OPEN, RECOVERY, FAULT, MANUAL.
- IGNITE at a fixed 50 percent until within 17 C of setpoint. RAMP and RECOVERY capped at 50 percent duty and integral until the setpoint is first reached. HOLD up to the 60 percent max duty cap.
- LID_OPEN on a falling-rate trigger (0.3 C/s, scaled with setpoint) with a fire-out guard, blower to 0, 30 s minimum, exit on setpoint, plateau or 240 s timeout into RECOVERY.
- Long-PWM: for demand below the blower's minimum spin duty, a 10 s period at min-start duty, plus a 300 ms full-duty kick at every start from rest. Since 2026-10-22: on-phases at least 5 s (period stretched to keep the average), a spinning fan passes through down to BLOWER_MIN_RUN_DUTY_PCT 10, and the off-phase cuts the fan supply (the Delta restarts late from a PWM-lead stop, at once from a supply cut).
- Warnings, which never touch the blower: PIT_HIGH, PIT_LOW, FUEL_LOW (latched, clears after 300 s clear), CREEP, LID_RISING. Faults: sensor open/short/range, over-temp, ignite timeout, FIRE_OUT, and since 2026-10-04 BLOWER_STALL (driven, no tach pulses for 5 driven seconds; 3 until 2026-10-22) and BLOWER_RUNAWAY (RPM above the commanded share of 4400 plus 1100 for 5 s).
- 4-wire fan drive (2026-10-04): GPIO duty inverted for the 2N7000 on the PWM lead; SUPPLY_EN low (fan unpowered) in IDLE and FAULT, high otherwise, raised only after the PWM value is set; PWM GPIO held low while the supply is cut. `-DBLOWER_LEGACY_2WIRE` builds the old model.
- Charcoal-specific behaviour that drove these choices is in doc 06: lit-mass creep after a high-demand period, the forge effect, backdraft through the duct with no flap, fuel-exhaustion warning ahead of fire-out.

## 7. Validated on hardware 2026-09-25
Boot, LEDC, NVS load and save, task watchdog, NimBLE start and pairing window, sensor-absence detection, the simulated-probe loop, IGNITE -> RAMP -> HOLD -> LID_OPEN with correct timings, the 200 C software over-temp fault and its latching, and refusal of startCook while a fault is latched. Detail and the timing table are in doc 05.

## 8. Blocked, in the order that unblocks the most
Purchasing status 2026-09-25 (doc 07, confirmed by Jason): everything for the first cook is ordered, Mouser lines, local hose and clamps, flap sheet, and the adapter print. Nothing left to buy until a measurement forces it (doc 07 "Still blocked").
1. DONE: breadboards on hand (doc 07 A13). Bench step 2a, rails under load, can start now; the jumper kit is in transit.
2. Pit sensor: MAX31856 board 1 PASSED the doc 09 section 2a inspection on the WROVER 2026-10-01 and again on the S3 2026-10-04 (env s3-n8r8-uart, COM6; doc 05 "S3 bench session"). EGT probe on it: ungrounded, yellow = T+, but 3.5 C high in ice and 4 to 5 C low at 88.9 C; kept for the first tests with `o 6.5`, replacement to follow. Still owed: the bench probe in the same bath as a control (clears or blames the board), a check of the offset at the grate on the first fire, board 2 through the inspection, the replacement probe. Board fail = buy the Adafruit 3263. Bench probe polarity: crimped bare lead is T- (doc 04).
2b. Delta blower (doc 07 A1): RECEIVED 2026-10-02; ran on straight 12 V 2026-10-05; running current 0.27 A at 100 percent under firmware 2026-10-10 (M5 done). Fan drive 2N7000s (A2): RECEIVED 2026-10-02, 20 pieces. 1 in ID hose and clamps (A3), flap silicone sheet (A4): ordered, not reported as received. Printed adapter (A3): print in progress, STL in output\3d-print\. 2026-10-02: the outlet is NOT 24.1 x 17.5 mm (measured 39.90 x 25.00 outside, 35.90 x 17.50 bore), so the v1 adapter is superseded by v6 direct (README-blower-adapter-v6.md); the v6 STL is in the repo since 2026-10-05; decide where the flap goes. OSOYOO S3 IO breakout (doc 07 A14): proposed, order status unrecorded.
2c. Received 2026-10-02: K-type EGT pit probe, 1/8 NPT compression fitting, 3.2 in, TESTED 2026-10-04 (ungrounded, inaccurate, kept for the first tests, item 2). Untested: 1N5822 x2 (optional insurance for the reverse-polarity diode, doc 07 A8), ADS1115 breakout (Phase 2, waits on the ThermoWorks probes), CPT-2416-85T buzzer (doc 09 N3; needs a 2N7000 and a pin in pins.h).
3. Thermostat continuity check: which of the three spade tabs is the pair closed at room temperature, and where it actually trips.
4. Measurements still missing: probe port thread and diameter, insertion depth to reach grate level about 8 cm inboard, tach pulses per revolution, exhaust stack OD and whether it has a damper, fire door seal condition. Delta outlet dimensions are now taken from the spec drawing (doc 07 A1), pending the caliper check.
5. Fan-off draft test: light the fire, leave the blower unplugged, close the door, and see whether the pit climbs past 120 C on its own. This decides how urgent the flap and the exhaust damper are. The simulation says a missing flap costs all control authority (mean duty under 1 percent).
6. PID gains from a step-response cook. Current gains are placeholders that the simulation says are safe, not tuned.
7. Field-test path agreed 2026-10-03: the ordered bench and smoker list is doc 09 section 7 item 4b. The one item that blocked assembly, the 4-wire fan firmware, was BUILT 2026-10-04 and VALIDATED on the Delta 2026-10-10 (section 9). Board DECIDED 2026-10-04 (Jason): the S3-N8R8 (doc 04 map C, env s3-n8r8-uart) for the breadboard fan circuit and the first smoker test; the WROVER is no longer the bench board. MAX31856 board 1 passed inspection on the WROVER only, so re-run console `i` once it is moved to the S3 pins.
8. New 2026-10-04, before the fan's YELLOW or BLUE lead touches a GPIO: doc 09 M9. The supply cut sits in the fan's ground lead, so with SUPPLY_EN low the fan's own ground floats; measure both signal leads against controller GND first. MEASURED 2026-10-05: both leads sit at 10.85 V with the supply cut, even into a 10 k load (doc 09 M9). YELLOW must not go straight to the tach GPIO; BLUE is fine on the 2N7000 drain. Fix DECIDED 2026-10-09: blocking diode in YELLOW (doc 04), build it and meter the GPIO-side node (3.3 V with the supply cut, under 0.8 V with the fan running and FG low). DONE 2026-10-10 (doc 05 "S3 fan bench"): both M9 10 k loads cut out, 1N5822 in YELLOW (3.2 V at the tach row with 12 V on the fan and the supply cut), all four leads on the S3 per map C, fan runs under firmware control, ppr 2 confirmed, duty sweep taken. Runaway and stall both PASS with the fan running. DONE 2026-10-10 second session: M5 0.27 A at 100 percent, `n 15` saved after 10 percent started 4 of 4. Owed: kick length on a scope. BLOWER_MIN_RUN_DUTY_PCT built 2026-10-22 (section 9).

## 9. Not built (next firmware work, doc 09 section 6a)
- VALIDATED ON THE FAN 2026-10-10 (doc 05 "S3 fan bench"): 4-wire fan support. Inverted PWM duty, tach input on a pulse counter, BLOWER_STALL and BLOWER_RUNAWAY to the doc 09 section 6a definitions (action: drop the supply-cut MOSFET), fan-RPM in the dump and the CSV. Bench list done: M9 diode fix, ppr 2 confirmed, start and run duty (starts at 10 percent, stops at 5), RPM against duty 10 to 100 percent (near linear), both faults fired and cut the supply. DONE 2026-10-10 second session: M5 0.27 A at 100 percent, 10 percent start 4 of 4, `n 15` saved.
- FIXED 2026-10-09, flashed and PROVEN 2026-10-10 (doc 05 "Two driver fixes" and "second session"): the start kick now ends at BLOWER_KICK_MS (the control task services the blower every 10 ms between ticks), and a MAX31856 unpowered at boot OR after boot reads NOT DETECTED on `i` and SENSOR_OPEN on read (`selfTest()` always re-probes; an all-zero reading clears `present_`). After the sensor comes back, `i` or a reboot re-probes it; a cook never does.
- DONE and BENCH-CHECKED 2026-10-22: BLOWER_MIN_RUN_DUTY_PCT = 10 (long-PWM hysteresis), BLOWER_LONG_PWM_MIN_ON_S = 5 (period stretched), FAN_STALL_S 5, long-PWM off-phase cuts the supply. 41/41 tests under WSL2, all five envs build, s3-n8r8-uart Flash 658150 B, flashed. Sim plant fan moved to the measured Delta (doc 08).
- Cook Manager (Phase 2): meat probes on an ADS1115, stall handling, done-and-hold, the juiciness logic in doc 06 section 6.
- DONE 2026-10-22: bench console over BLE (000d) and CSV telemetry notify (000e); all BLE writes fixed (plain WRITE flag was missing since the first build, so no phone write had ever worked); bonded and proven from JASON-PC with bleak (doc 05 "BLE debug channels").
- BLE config characteristics for gains, caps, offset and lid detect. Console over 000d covers them; dedicated characteristics are Phase 2.
- Pairing button, buzzer, flash-persisted event log, OTA.

## 10. Standing constraints for anyone continuing this
- Approved directories: C:\Users\wl_de\projects\, C:\Users\wl_de\OneDrive\, C:\Temp\, D:\, J:\, X:\, Z:\. Temp output goes to C:\Temp.
- Never touch credential stores, .env files, keys, the registry, or system directories.
- Do not run git commands through the Claude device bridge. The bridge cannot delete files, so a stale .git\index.lock has to be cleared by hand. Run git in PowerShell or Claude Code CLI on the machine. The repo root is the project directory, not firmware\.
- Native unit tests and the simulation need a host g++ (doc 05). None is installed on Windows on JASON-PC; WSL2 Ubuntu there has g++ 13.3.0 and runs both when they are compiled directly (doc 05, 2026-10-04).
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
