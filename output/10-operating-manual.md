# 10. Operating manual: charcoal smoker controller

Written 2026-10-28 from the firmware source on `main` plus the working-tree sensor fix of 2026-10-27
(`src/hal/pit_sensor.cpp`, flashed to the bench S3, not yet committed). Every number below comes from
`firmware/include/config.h` or the code that uses it; file references are given so a value can be re-checked
when the code changes. Temperatures are Celsius with Fahrenheit in brackets.

## 1. What it does

The controller holds a drum charcoal smoker at a setpoint by blowing air into the fire with a 12 V fan. It reads
the pit with a K-type thermocouple, runs a PID loop once a second, and drives the fan speed. It cannot cool the
pit: the only lever is less or more air. Everything is reachable three ways: the phone/PC web page over
Bluetooth, the serial console over USB, and the same console over Bluetooth.

## 2. Hardware it needs

| Part | Job | Pin (S3, doc 04 map C) |
| --- | --- | --- |
| ESP32-S3 N8R8 dev board | controller, BLE, console on the CH343 USB port | |
| MAX31856 breakout (board 1) | thermocouple front end, 3.3 V, SPI | CS 10, SDI 11, SCK 12, SDO 13 |
| K-type EGT probe | pit temperature, into the MAX31856 T+/T- | |
| Delta BFB0712HH-AWPH | 12 V 4-wire blower: RED +12, BLACK GND, BLUE PWM, YELLOW tach | |
| IRLZ44N | fan supply cut, low side in the BLACK lead | gate via resistor to GPIO 6 (SUPPLY_EN) |
| 2N7000 | open-drain PWM driver on BLUE | gate via resistor to GPIO 4 (PWM) |
| 1N5822 + 10 kΩ | tach protection: stripe to YELLOW, plain end to a row with 10 kΩ to 3.3 V and the jumper to GPIO 5 | GPIO 5 (TACH) |
| 12 V supply, 1 A fuse | fan power | |
| Capillary thermostat, dial 270 C (518 F) | hardware backstop: its contact is in series with fan RED | |

Rules that keep the board alive:

- One common ground: 12 V supply −, S3 GND, MAX31856 GND, both MOSFET sources. Check with the meter's beep setting.
- Nothing from the 12 V side ever lands on the 3.3 V rail. 12 V on 3V3 killed the first S3 (2026-10-26).
- YELLOW never goes straight to a GPIO: with the fan supply cut it sits near 11 V. Always through the 1N5822.
- BLUE is driven open-drain only (the 2N7000). Never push-pull it from a GPIO, never put 12 V on it.
- Recommended, not yet fitted: 10 µF and 0.1 µF across the MAX31856 VIN/GND at the module (section 9, Q2).

## 3. Software it needs

- PlatformIO, env `s3-n8r8-uart` (the bench and smoker board). Build: `pio run -e s3-n8r8-uart`. Flash:
  `pio run -e s3-n8r8-uart -t upload --upload-port COM8`.
- `firmware/.env` with the 6-digit BLE passkey (`SMOKER_BLE_PASSKEY`, copy `firmware/.env.example`). The build
  refuses to run without it. The passkey never goes in git.
- Phone/PC UI: <https://jhk1m.github.io/charcoal-smoker/app.html> (Chrome on PC, Bluefy on iPhone). The same page
  is also at `index.html` and `ui.html`; `app.html` is the newest path, so Bluefy cannot serve an old cached copy.
  Debug mode (every raw CSV field, the console, the event log and the CSV download): tap the "Smoker" title 5 times,
  or open the page with `#debug` at the end of the address. Close it with its Close button. It is hidden, not
  locked: what protects the smoker is the BLE bond and passkey, which every write needs.
- Optional PC tools, system Python 3.12: `firmware/tools/ble.py` (console over BLE), `firmware/tools/pair.py`
  (re-bond the PC).

Before a reflash: stop the cook (`x`) and disconnect the phone. A cook that is running resumes through IGNITE
after the reboot, and a reflash with the phone connected has wiped the phone's bond before.

## 4. Cook states

One state at a time; shown as `state` in the CSV and on the web page.

| State | Fan | Enters when | Leaves when |
| --- | --- | --- | --- |
| IDLE | off, supply cut | boot, Stop, Clear fault, timer reaches zero | Start cook → IGNITE; manual → MANUAL |
| IGNITE | fixed 50 % | Start cook | pit ≥ setpoint − 17 C (31 F) → RAMP; 40 min with less than 10 C (18 F) rise → FAULT IGNITE_TIMEOUT |
| RAMP | PID, capped at 50 % | from IGNITE, or any setpoint change while cooking | pit within ±5 C (9 F) of setpoint for 60 s → HOLD |
| HOLD | PID, capped at max duty (60 %) | settled | lid event → LID_OPEN; setpoint change → RAMP |
| LID_OPEN | off | lid detected (section 6) | at least 30 s and the pit has recovered, or 4 min at most → RECOVERY |
| RECOVERY | PID, capped at 50 % | after LID_OPEN | settled 60 s → HOLD; 10 min at most → HOLD |
| MANUAL | fixed duty you set, capped at max duty | `m <pct>` or manual mode | `m` with no value, Stop |
| FAULT | off, supply cut | any fault (section 5) | Clear fault → IDLE only |

Notes:

- The PID integral is reset when entering RAMP from IGNITE, IDLE or MANUAL, and halved the first time RAMP
  crosses the setpoint (HeaterMeter startup rule). Until the setpoint is first reached the fan is held to the
  50 % startup cap; after that the cap is the max duty (60 %, `X`).
- Over-temp, sensor and fan faults are checked in every state except IDLE and FAULT, so MANUAL is guarded too.
- Power loss: if a cook was running (`cook_active` in NVS), the board restarts it through IGNITE at boot. A hot
  pit exits IGNITE to RAMP on the first tick. A fault, Stop, manual mode or the timer clears `cook_active`, so
  those never resume.

## 5. Faults

A fault latches: the fan stops and its supply is cut, `cook_active` is cleared, and nothing restarts on its
own. Only Clear fault (web page button, console `k`, BLE mode write 3) leaves FAULT, and it always goes to
IDLE. While in FAULT the console refuses `c`, `x` and `m <pct>` with `refused: in FAULT (...). Type k to clear
it first.`

| Fault | Trigger | What to check |
| --- | --- | --- |
| SENSOR_OPEN | thermocouple open-circuit bit, or the MAX31856 not answering (all registers 0) | probe plugged in, T+/T- screws, MAX31856 jumpers; console `i` |
| SENSOR_SHORT | MAX31856 over/under-voltage bit | probe wire shorted to the drum or a rail |
| SENSOR_RANGE | reading below −20 C or above 400 C, range bits, or NaN | probe, wiring, offset (`o`) |
| IGNITE_TIMEOUT | in IGNITE 40 min and the pit rose less than 10 C (18 F) | fire not lit, intake blocked, probe outside the pit |
| PIT_OVER_LIMIT | pit ≥ min(setpoint + 40 C, 240 C) | section 5.1 |
| FIRE_OUT | in RAMP/HOLD/RECOVERY: fan at its cap AND pit ≤ setpoint − 40 C (72 F) for 15 min | add charcoal / relight |
| BLOWER_STALL | fan driven and no tach pulses for 5 driven seconds (long-PWM off time does not count) | 12 V, fuse, thermostat contact open, fan plug, tach wiring |
| BLOWER_RUNAWAY | RPM above (commanded % × 4400 + 1100) for 5 s | BLUE lead broken or off the 2N7000 (fan then runs flat out) |
| WATCHDOG_RESET | control task hung for 5 s; the board resets | logged at boot as an event; the board does not sit in FAULT for it |

### 5.1 PIT_OVER_LIMIT and how to recover

Limit = setpoint + 40 C (72 F), but never above 240 C (464 F), which stays below the 270 C (518 F) thermostat.

| Setpoint | Over-limit trips at |
| --- | --- |
| 93 C (200 F) | 133 C (271 F) |
| 110 C (230 F) | 150 C (302 F) |
| 121 C (250 F) | 161 C (322 F) |
| 135 C (275 F) | 175 C (347 F) |
| 200 C (392 F) | 240 C (464 F) |

What happens: within one second of the reading crossing the limit, the fan stops and its supply is cut, state
goes to FAULT, the phone shows `PIT_OVER_LIMIT`. There is NO automatic recovery, by design: a fire that runs away
with the fan off is getting air from somewhere the controller cannot close, and restarting the fan would make it
worse.

Recovery:

1. Find the air leak. The fan is already off, so the extra air is coming through the intake duct (missing or
   stuck flap), a gap at the lid or the drum, or an exhaust opened too far. Close the intake and partly close
   the exhaust. Do not open the lid to cool it: on a drum, opening the lid makes it hotter.
2. Wait until the pit is back below the limit. Watch it on the web page or the CSV (`pit_filt_c`); the reading
   keeps updating in FAULT.
3. Clear the fault: Clear fault button, console `k`, or BLE mode 3. State → IDLE. Clearing also clears the timer.
4. Start the cook again. It goes through IGNITE and, since the pit is hot, straight on to RAMP.

If you clear and start while the pit is still above the limit, it faults again on the next tick. Raising the
setpoint raises the limit with it, but the setpoint does not clear a fault: you still clear it first.

## 6. Lid detection

On (`l 1`, saved) by default. Only armed after the setpoint has been reached once.

- Drop: pit below setpoint and falling at 0.3 C/s or faster over a 10 s window (the rate threshold scales up to
  1.2 C/s at a 260 C setpoint). Ignored when the fan's output average is at or above 90 % of max, because that
  looks like fuel running out, not a lid.
- Rise (drum): with the fan at or below 10 % duty, pit rising at 0.3 C/s or faster for 20 s. Sets the LID_RISING
  warning.
- In LID_OPEN the fan is off and the PID integral is frozen. Exit after at least 30 s when the pit is back at the
  setpoint, or has plateaued 11 C above its lowest point (rising slower than 0.06 C/s), or the rise variant has
  stopped rising; 4 min at most. Then RECOVERY.
- Turn it off in high wind (`l 0`) if gusts trigger false lid events.

## 7. Warnings (do not stop the cook)

A bitmask, `warnings` in the CSV and on the web page.

| Bit | Name | Meaning |
| --- | --- | --- |
| 0x01 | PIT_HIGH | pit ≥ setpoint + 14 C (25 F), after the setpoint was reached (1 C hysteresis) |
| 0x02 | PIT_LOW | pit ≤ setpoint − 14 C (25 F), after the setpoint was reached |
| 0x04 | FUEL_LOW | in HOLD (after 10 min there): fan output average ≥ 90 % and the pit fell 5 C or more over 10 min. Clears after 5 min without the condition. Add charcoal |
| 0x08 | CREEP | fan at 0 % and pit ≥ setpoint + 14 C for 15 min: air leak or too much lit fuel |
| 0x10 | LID_RISING | the current LID_OPEN was a rise (drum), not a drop |
| 0x20 | TIMER_DONE | the cook timer hit zero and stopped the cook. Stays up until a timer is set, a cook starts, Stop, or Clear fault |

## 8. Fan behaviour

- Speed range: duty 0 to the max duty (`X`, default 60 %, saved). 25 kHz PWM (21 to 28 kHz allowed, `f`).
- Start kick: from a stop the fan gets the max duty for 300 ms, then the requested duty.
- Below the min start duty (`n`, bench value 15 %) the fan cannot start reliably, so low requests are made by
  long-period on/off (long-PWM): about a 10 s period with at least 5 s on, stretched as needed, and the supply is
  cut in the off phase. A fan that is already spinning is kept running down to 10 % (hysteresis) instead.
  So at a low setpoint the fan pulsing on and off every few seconds is normal.
- Tach: 2 pulses per revolution (`p`, saved). Bench speeds on 2026-10-10: 30 % 1780 RPM, 50 % 2817, 60 % 3250.
  On 2026-10-27, after the YELLOW rewire, it read about 700 at 50 % and 870 at 60 %: either the fan is slower or
  pulses are being missed. Open item.
- `T 0` turns BLOWER_STALL and BLOWER_RUNAWAY off until the next reboot (bench only, not saved).

## 9. Questions and answers

**Q1. I press Start, the fan spins and stops, the page says SENSOR_OPEN.**
The board stopped the fan because the thermocouple reading failed. Clear (`k`), then type `i`. `present` with
`register readback PASS` and a sane temperature means the sensor is fine now; `NOT DETECTED` means the MAX31856
is not answering: check its VIN (3.3 V), GND and the four SPI jumpers. If the probe itself is unplugged, `i`
shows `OPEN`.

**Q2. The log shows `MAX31856 read all zero, re-initialised`. Is that a problem?**
It is the 2026-10-27 workaround doing its job. Switching the fan on dips the supply at the MAX31856 enough to
reset it; the chip comes back with its settings cleared and reads 0.00 on both channels. The board now sets it up
again, waits one conversion (200 ms) and carries on. One such line per fan start is the known hardware issue; the
fix is 10 µF + 0.1 µF across the MAX31856 VIN/GND. Lines while the fan is already running would be new: report
them. If the chip does not answer at all, it still faults SENSOR_OPEN.

**Q3. How do I recover from PIT_OVER_LIMIT?** See section 5.1: close the air leak, wait for the pit to drop below
the limit, clear the fault, start again.

**Q4. `refused: in FAULT` when I type `c`, `x` or `m 30`.** Clear the fault first with `k`.

**Q5. BLOWER_STALL at every start.** The board sees no tach pulses. In order: 12 V plugged in, fuse, thermostat
contact closed, fan connector the right way round (it was plugged in backwards once, 2026-10-24), YELLOW through
the diode to the GPIO 5 row with the 10 kΩ pull-up. For bench work with no fan, `T 0`.

**Q6. The fan clicks on and off at low heat.** Long-PWM (section 8). Normal.

**Q7. The pit sits above the setpoint with the fan off.** The fire is getting air elsewhere (no intake flap, lid
gap, exhaust wide open) or there is too much lit charcoal. After 15 min at +14 C the CREEP warning shows; at +40 C
the cook faults PIT_OVER_LIMIT.

**Q8. The power went out mid-cook.** On power-up the cook resumes through IGNITE at the saved setpoint, then RAMP.
Faulted, stopped or timed-out cooks do not resume.

**Q9. I changed the setpoint mid-cook and the state went back to RAMP.** Intended: a setpoint change re-enters
RAMP with the 50 % startup cap until the new setpoint is reached, which limits overshoot.

**Q10. The phone will not connect / pair.** Pairing is only accepted in the 120 s after the board boots. Reset the
board, forget SMOKER-CTRL on the phone, connect inside the window and type the passkey from `firmware/.env`.
Console `B` lists bonds, `B clear` deletes all of them (every device must then pair again). The PC re-bonds with
`firmware/tools/pair.py COM8`.

**Q11. The Timer button.** Sets minutes (0 clears). At zero the cook stops (IDLE, fan off) and TIMER_DONE shows.
Stop and Clear fault also clear the timer. In Bluefy it goes through the console channel (`a <min>`), because
Bluefy drops the native timer write.

**Q12. The temperature reads a few degrees off.** Set the probe offset with `o <C>` (saved, ±20 C). The current
+6.5 C comes from the ice and hot-water checks on 2026-10-04.

**Q13. Opening `pio device monitor` restarted the board.** The monitor toggles the reset line when it opens. A
running cook resumes through IGNITE; anything else comes back in IDLE. Check `d` after attaching.

**Q14. Can it cool the pit?** No. With the fan off the pit is only as cool as the leaks allow. Over-temp is caught
by PIT_OVER_LIMIT (software) and the 270 C thermostat (hardware, cuts fan power).

## 10. Console reference

Serial at 115200 on the CH343 port (COM8 on the bench), or the Console box on the web page. Every accepted line
is echoed as `# > <line>`.

| Command | Does | Range | Saved |
| --- | --- | --- | --- |
| `?` | help | | |
| `d` | dump settings, state, fault, duty, RPM, supply | | |
| `v` | CSV stream on/off (serial only; BLE always gets it) | | no |
| `s <C>` | setpoint | 60 to 200 C | yes |
| `c` | start cook | refused in FAULT | |
| `x` | stop cook, clears the timer | refused in FAULT | |
| `k` | clear fault → IDLE, clears the timer | | |
| `m <pct>` | manual fan duty | 0 to 100, capped at max duty; refused in FAULT | |
| `m` | leave manual (stop) | | |
| `a <min>` | cook timer, stops the cook at zero; `a 0` clears | | no |
| `g <kp> <ki> <kd>` | PID gains | defaults 4 / 0.02 / 0 | yes |
| `f <hz>` | fan PWM frequency | 21000 to 28000 | yes |
| `n <pct>` | min start duty | 1 to 99 (bench 15) | yes |
| `X <pct>` | max duty | 10 to 100 (default 60) | yes |
| `o <C>` | probe offset | −20 to 20 (bench 6.5) | yes |
| `l <0/1>` | lid detection | | yes |
| `p <n>` | tach pulses per revolution | 1 to 8 (Delta 2) | yes |
| `T <0/1>` | fan stall/runaway faults off/on | | no, on at every boot |
| `i` | sensor identity check, re-initialises the MAX31856 | | |
| `t <C> [C/s]` | simulate the probe (optionally ramping) | | no |
| `t off` | back to the real probe | | |
| `B` / `B clear` | list / delete BLE bonds | | |
| `r` | reset ALL settings to defaults and reboot | | wipes NVS |

`r` brings back the compile-time defaults (min start 30, offset 0, setpoint 110): re-enter `n 15` and `o 6.5`
afterwards.

CSV columns, once a second: `t_s, state, fault, pit_raw_c, pit_filt_c, cj_c, duty_cmd, duty_applied,
tc_fault_bits, warnings, out_ema, rate_c_s, fan_rpm, timer_s`. `pit_filt_c` is the 5-sample median the controller
uses. `cj_c` comes from the real chip even while `t` simulates the probe, so it shows a MAX31856 reset as 0.00.

## 11. Bluetooth reference

Device name `SMOKER-CTRL`, service `7a3b0000-6c1e-4f2a-9d5b-2c8e1f0a0001`. Writes need an encrypted (bonded) link.

| Char | Data | Access |
| --- | --- | --- |
| 0002 | pit °C, float32 | notify |
| 0003 | setpoint °C, float32, 60 to 200 (others rejected) | read / write |
| 0004 | fan duty %, uint8 | read / notify |
| 0005 | state, uint8 (0 IDLE, 1 IGNITE, 2 RAMP, 3 HOLD, 4 LID_OPEN, 5 FAULT, 6 MANUAL, 7 RECOVERY) | notify |
| 0006 | fault, uint8 (0 NONE, 1 SENSOR_OPEN, 2 SENSOR_SHORT, 3 SENSOR_RANGE, 4 IGNITE_TIMEOUT, 5 PIT_OVER_LIMIT, 6 WATCHDOG_RESET, 7 FIRE_OUT, 8 BLOWER_STALL, 9 BLOWER_RUNAWAY) | notify |
| 0007 | mode, uint8: 0 start cook, 1 manual, 2 stop, 3 clear fault | write |
| 0008 | manual duty %, uint8 (only in MANUAL) | write |
| 0009 | event log, one record per read | read |
| 000a | device info string | read |
| 000b | warnings bitmask (section 7) | read / notify |
| 000c | timer minutes, uint16, 0 clears | read / write (Bluefy drops it: use the console) |
| 000d | console line in, replies notified | write (bonded + authenticated) |
| 000e | CSV telemetry line, 1 Hz | notify |

## 12. Fixed limits (compile time, `config.h`)

| Item | Value |
| --- | --- |
| Control period | 1 s |
| Watchdog | 5 s |
| Setpoint range / default | 60 to 200 C (140 to 392 F) / 110 C (230 F) |
| Over-limit | setpoint + 40 C, cap 240 C |
| Valid sensor range | −20 to 400 C |
| IGNITE duty / exit / timeout | 50 % / setpoint − 17 C / 40 min with < 10 C rise |
| Startup cap (RAMP, RECOVERY) | 50 % |
| HOLD band / settle | ±5 C / 60 s |
| Recovery max | 10 min |
| Fire out | fan at cap, pit ≤ setpoint − 40 C, 15 min |
| Stall / runaway | 5 s no pulses / 5 s above commanded + 1100 RPM |
| Kick | max duty, 300 ms |
| Long-PWM | period 10 s (stretched), on-time ≥ 5 s, min run 10 % |
| BLE pairing window | 120 s after boot |

## 13. Before every cook

1. Fan 12 V plugged in, thermostat set, intake flap free.
2. `d`: state IDLE, fault NONE, setpoint right, `tach_faults=1`.
3. `i`: `present`, readback PASS, pit reading sane at ambient.
4. Phone connected and showing the pit temperature.
5. Light the fire, close the lid, Start cook.
