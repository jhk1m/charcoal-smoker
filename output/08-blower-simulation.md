# Closed-Loop Blower Simulation (2026-09-21)

Status: SIMULATION. The plant is assumed, not measured. Results test controller logic and compare design choices; they are not tuning data.
Code: C:\Users\wl_de\projects\charcoal-smoker\firmware\sim\ (README there has build steps). Plots in sim\out\.
Builds on: 05 (firmware), 06 (research, proposals), 07 (parts).

## What runs
- Real firmware control core, unmodified: src/control state machine, PID, LongPwm, MedianFilter<5>; thresholds built from config.h the same way main.cpp does.
- HAL blower logic re-implemented without Arduino: long-PWM below min-start (30 percent placeholder) and the 300 ms kick at every start from OFF.
- Plant at 10 Hz, controller at 1 Hz. Brushed blower with start and stall thresholds and a 0.6 s spin-up; duct efficiency 0.8; seam leak 0.3 CFM; optional stack-draft leak through the duct when there is no flap (2.5 CFM at 90 C rise, square-root law); charcoal bed with lit mass, forge-effect ignition spread (1.2 kg lit per kg burned), air-limited burn at 0.075 kg carbon per m3 air, 90 s glow time constant, 29 MJ/kg, 45 percent of heat into the chamber; drum 20 kJ/K, 12 W/K wall loss, exhaust enthalpy loss; 4 kg meat from 4 C; K-type probe with 8 s lag and 0.25 C noise.
- Timeline: 12 h cook from 12 lit briquettes (0.25 kg) in a 4 kg basket, lid open 90 s at 3 h and 20 s at 6 h, wind doubling wall loss for 20 min at 5 h, fuel runs out near the end.

## Results

| Scenario | Reach setpoint | Startup overshoot | HOLD rms / max (quiet) | Worst in wind | Mean HOLD duty | Lid detected / false | FUEL_LOW lead before FIRE_OUT |
|---|---|---|---|---|---|---|---|
| nominal (defaults, Delta 15.3 CFM, flap) | 37 min | 3.4 C | 0.51 / 3.4 C | 6.2 C | 20 % (99 % of HOLD in long-PWM) | 2 / 0 | 43 min |
| no_flap, 110 C | 40 min | 6.7 C | 6.0 / 8.3 C | 7.9 C | 0.6 % | 2 / 0 | 35 min |
| no_flap, 93 C | 28 min | 7.0 C | 18.6 / 24.7 C | 23.4 C | 0.4 % | 1 / 0 | 43 min |
| flap, 93 C | 26 min | 4.0 C | 0.42 / 4.0 C | 4.9 C | 15 % (100 % long-PWM) | 2 / 0 | fuel lasted |
| caps_removed (Phase 1 defaults) | 45 min | 10.0 C | 1.33 / 10.4 C | 6.2 C | 20 % | 2 / 0 | 34 min |
| fan_20cfm, current caps | 40 min | 4.9 C | 0.58 / 4.9 C | 5.1 C | 15 % (99 % long-PWM) | 2 / 0 | 39 min |

Re-run 2026-10-22 with the plant's fan moved to the measured Delta (free air 15.3 CFM, stall 7 percent, start 10 percent; were 10 CFM, 22, 28) and the firmware's long-PWM changes (min-run hysteresis at 10, minimum on-phase 5 s with the period stretched). The 2026-09-21 numbers (10 CFM plant) were: nominal 40 min, 1.1 C, 0.47 / 3.8 C, 7.2 C, 29 % (75 % long-PWM), 48 min; flap 93 C 0.33 / 3.1 C; fan_20cfm 0.58 / 5.2 C. Read: with `BLOWER_MIN_START_DUTY_PCT` still 30 in config.h and a 15 CFM fan, HOLD lives almost entirely in long-PWM; the bench runs with `n 15`, which the sim does not use. The 2026-09-21 PNGs in sim/out were not regenerated (no matplotlib on the host).

Burn rate in HOLD: 0.35 kg/h at 110 C, 0.27 kg/h at 93 C (inside the 0.3 to 0.6 kg/h planning range by construction).

## Findings
1. Backdraft flap is the single largest effect. Without it the controller has no authority: mean duty under 1 percent, the pit runs where the draft puts it. At 93 C the pit sits at 110 to 118 C for the whole cook; PIT_HIGH and CREEP fire as designed. Confirms P2 (doc 06) and the fan-off draft test as a pre-cook priority.
2. Startup cap works: overshoot 1.1 C vs 7.9 C with the Phase 1 defaults, same time to setpoint. Tradeoff seen: after the 90 s lid event RECOVERY takes about 10 min and times out into HOLD at about 9 C low instead of about 7 min with caps removed. Acceptable for low-and-slow; RECOVERY_MAX_S can be revisited with real data.
3. Lid detection: 4 of 4 lid events caught in every flap scenario, 0 false positives, including through the wind gust.
4. FUEL_LOW gives 38 to 48 min of warning before FIRE_OUT, above the 30 min target (doc 06 section 1b item 5).
5. BUG FOUND AND FIXED: FUEL_LOW chattered (on/off four times in one minute) during the wind gust. Now latched; clears only after FUEL_LOW_CLEAR_S = 300 s without the condition.
6. BUG FOUND AND FIXED: with the caps removed, FUEL_LOW fired falsely 9 min after the lid event, because the blower pegs in RECOVERY while the 10 min look-back still sees the pre-lid temperature. FUEL_LOW is now evaluated only in settled HOLD (after one look-back window). Regression unit test added and verified to fail against the old logic.
7. Remaining FUEL_LOW false positive: a strong wind gust that pegs the blower and drops the pit 5 C looks the same as fuel running out. It clears after the gust. The app text should read "cannot hold temperature: add charcoal or shield from wind".
8. Long-PWM behaviour near the min-start duty: at 10 CFM the PID settles at 27 to 29 percent, just below the 30 percent placeholder, so the HAL drops the blower for 1 s every 10 to 40 s and kicks it back at 60 percent. Harmless to temperature (the coal bed time constant is about 90 s) but it is a stop-start duty cycle on the motor. A 20 CFM blower spends 99 percent of HOLD in long-PWM. Recommendation for bench step 2: measure both the start duty and the lower run duty of the blower; if run is well below start, add BLOWER_MIN_RUN_DUTY_PCT so a spinning motor is not stopped for small requests. Not implemented until measured.
9. Gain sweep (Kp 2/4/8, Ki 0.005/0.02/0.05, Kd 0/5) on this plant: Ki 0.05 is clearly worse (HOLD rms 1.7 to 4.1 C); Kp 8, Ki 0.02 was best (0.34 to 0.37 C rms); current defaults Kp 4, Ki 0.02 are within 0.1 C of best. Because the plant is assumed, this only says the defaults are not dangerous; real gains come from the step-response cook.
10. A 20 CFM blower costs 4 C of extra startup overshoot with the same caps and pushes the whole cook into long-PWM. Supports the 8 to 15 CFM requirement R7 in doc 06 section 3.1a.

## Firmware changes made from the sim (built and tested 2026-09-21)
- state_machine: FUEL_LOW latched with fuel_low_clear_s; evaluated only in HOLD after state_elapsed >= fuel_low_window_s; kept (not cleared) during LID_OPEN.
- config.h: FUEL_LOW_CLEAR_S = 300. main.cpp passes it.
- Tests: 29/29 native pass (added latch test and the post-lid regression). pio run -e s3-n8r8 SUCCESS, 0 project warnings.

## Next
- Replace plant parameters with measurements as they arrive (sim README lists which measurement feeds which parameter), then re-run and compare against the logged cook. DONE for the fan 2026-10-22; the rest wait on the first fire.
- Consider lowering `BLOWER_MIN_START_DUTY_PCT` from 30 to the bench value 15 (Jason's call) and re-running: at 30 the sim sits in long-PWM for 99 percent of HOLD.
- Add the rising-lid drum behaviour to the plant once a real lid event is logged (the current plant models a lid as a pure temperature drop).
