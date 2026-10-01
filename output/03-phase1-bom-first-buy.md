# Phase 1 - First Purchase List (bench bring-up)

Created: 2026-09-20. Purchase status updated 2026-09-20: ALL PARTS ORDERED.
Updated 2026-09-25: arrivals recorded, breadboard correction, items superseded by the Delta 4-wire blower decision marked.
Scope: everything needed to run bring-up steps 1 to 5 in 02-phase1-architecture.md. Enclosure, PCB, and meat probes are deferred. Parts added after the Delta decision are in doc 07, not here.
Already on hand: 6 V brushed blower, Gumsung capillary thermostat, drum smoker, jumper/hookup wires, 200 ohm and 10 k resistors, multimeter, USB-C cable, heat source.
CORRECTION 2026-09-24, corrected again 2026-09-25: this list said a breadboard was on hand. One exists, in use with the Freenove WROVER, but it is too short for the S3 by two pins. See item 14 and doc 07 item A13.

## Purchase status
ORDERED, Amazon, 2026-09-19:
- YEJMKJ ESP32-S3 N8R8 2-pack (order 111-7872990-3981853). RECEIVED 2026-09-23. 1 in use, 1 spare. Flashed and validated 2026-09-25 (doc 05).
- Generic MAX31856 breakout 2-pack. RECEIVED by 2026-10-01 (exact date not recorded). Board 1 chip marked MAX31856, passed the doc 09 section 2a inspection 2026-10-01; board 2 not yet tested.
- HUABAN MAX31855 2-pack (order 111-5592486-4043419). RECEIVED 2026-09-23. Default sensor for bench work; the firmware has a MAX31855 backend (build flag -DPIT_SENSOR_MAX31855, default env s3-n8r8-uart-max31855). BOTH BOARDS DEFECTIVE, BOTH RETURNED (recorded 2026-09-26): tested on the Freenove WROVER with T+ (yellow) shorted to T- (red), each board read a steady 2.0 to 2.5 C against a cold junction of 25.1 C, about 23 C low; a good MAX31855 agrees within about 2 C on a shorted input. SPI decode, open-circuit detection and the 3.2 V supply all checked out, so it is the chips (numbers in doc 05, "WROVER bench session"). No MAX31855 is on hand. The pit sensor for the bench is now the MAX31856 (received; board 1 inspection PASS 2026-10-01); the MAX31855 firmware backend stays untested on hardware.

ORDERED, Amazon, 2026-09-20:
| ASIN | Item | Price |
|---|---|---|
| B0FMYHCB6X | Bridgold IRLZ44N 10-pack | $4.49 |
| B00YX8XCZK | K-type EGT probe, 1/8 NPT compression. Junction type not stated; test on arrival (sheath-to-lead continuity = grounded) | $3.80 |
| B079KG1TN2 | Chanzon 1N5819 Schottky diodes | $7.99 |
| B0DBVYP91F | Seloky LM2596 buck 5-pack | $7.99 |
| B013HJI0Q6 | 12 V 2 A adapter, 5.5 x 2.1 mm, UL | $9.99 |
| B0BD46CP5Y | DaierTek 5.5 x 2.1 mm panel jack + plug, 6 sets | $9.99 |
| B0813Q4S6P | BOJACK inline 5 x 20 mm fuse holders + 150 fuses | $11.99 |
| B01G4POUAU | BAOMAIN 1/4 in female spade terminals, 100 pk | $6.99 |
| B0FW5C1381 | DROK K-type bench probes 3-pack | $6.99 |

Fallback pit probe if B00YX8XCZK tests grounded or reads noisy: Procon B09HSJ4PBB, $19.99, stated ungrounded, 5 x 80 mm, 1/8 NPT, 2 m braided lead.

SUPERSEDED 2026-09-25 (doc 07 A8 CLOSED): the 1N5819 on hand carries the real worst-case input of about 0.67 A (fan 0.42 A plus about 0.25 A for the 5 V rail) with margin, so no SS34 is required. Do NOT parallel two 1N5819s; paralleled diodes do not share current. The 1N5822 (Mouser 625-1N5822-E3) is ordered as optional insurance.

## A. Core electronics
| # | Item | Spec / part | Qty | Notes |
|---|------|-------------|-----|-------|
| 1 | MCU dev board | YEJMKJ ESP32-S3 N8R8. ESP32-S3-DevKitC-1 clone: WROOM-1-N8R8 module, dual USB-C, RGB LED, headers pre-soldered. | 1 (+1 spare) | RECEIVED. Follows the DevKitC-1 pinout. Two USB ports: the CH343 UART bridge (VID 1A86) is the one the console uses; the native port (VID 303A) is not. Detail in doc 05. |
| 2 | Thermocouple amplifier | Generic MAX31856 breakout, Adafruit 3263 layout. | 1 (+1 spare) | RECEIVED. Board 1 marked MAX31856, inspection PASS 2026-10-01; board 2 untested. Thermocouple must be ungrounded-junction (drum is metal). |
| 2a | Thermocouple amplifier, interim | HUABAN MAX31855 breakout | 1 (+1 spare) | RECEIVED. Firmware backend exists and detects sensor absence correctly on hardware. |
| 3 | Pit probe | K-type, stainless sheath, compression fitting | 1 | 1/8 NPT EGT probe ordered. Verify port thread and junction type. |
| 3a | Bench thermocouples | DROK 3-pack K-type 1 m | 1 | Boiling/ice water sanity checks only. |
| 4 | Blower switch | IRLZ44N, TO-220 | 2 of 10 | VERIFIED: Infineon part page. ROLE CHANGED: with the Delta 4-wire fan this is a fault-only supply cut in the fan's BLACK lead, not the PWM element. Doc 04. |
| 5 | Flyback diode | 1N5819 Schottky 1 A / 40 V | 2 of 100 | SUPERSEDED: not used with the Delta. A brushless fan has its own internal drive; no flyback diode across the fan. Keep for other inductive loads. |
| 6 | Gate resistor | 200 ohm 1/4 W (on hand) | 2 | GPIO to gate, both the IRLZ44N and the 2N7000 PWM driver. |
| 7 | Gate pull-down | 10 kohm 1/4 W (on hand) | 2 | Gate to GND; fan supply off during reset. Also the 10 k tach pull-up to 3.3 V. |
| 8 | Buck, blower rail | LM2596 module set to 6.0 V | 1 of 5 | SUPERSEDED 2026-09-24: deleted. The Delta runs on the 12 V input rail. Spare bucks stay in the box. |
| 9 | Buck, logic rail | LM2596 module set to 5.0 V | 1 of 5 | Feeds dev board 5V pin. Set and verify the voltage before connecting the board. |
| 10 | Adapter | 12 V 2 A, 5.5 x 2.1 mm center positive | 1 | |
| 11 | Barrel jack | DaierTek panel jack + plug | 1 of 6 | |
| 12 | Input fuse | 2 A fast-blow 5 x 20 mm in BOJACK inline holder | 1 | Plus a 1 A on the fan branch (doc 07 A7). |
| 13 | Reverse polarity protection | SS34 3 A Schottky or P-MOSFET | 1 | NOT BOUGHT. Bench: two 1N5819 in parallel or omit with care on polarity. |

## B. Bench and wiring
| # | Item | Qty | Notes |
|---|------|-----|-------|
| 14 | Breadboard, 830 pt | 2 | REASON CORRECTED 2026-09-25: the existing breadboard, used for the Freenove WROVER, is too SHORT for the S3. The ESP32-S3-DevKitC-1 is 70 x 28 mm with 44 pins, 22 per row, so it needs 22 rows of terminal strip (55.9 mm of pin span); two pins overhung the end of the old board and made no contact. Note this also proves the width is fine: the rest of the pins seated, so the board straddles the centre channel correctly. A full 830-point board has 63 rows (160 mm), so the S3 uses 22 and leaves about 41 free for the MAX31855, the MOSFETs and the fan header. Two BusBoard BB830 ordered 2026-09-25 (doc 07 A13), one for the S3 bench, one spare. Breadboards ON HAND as of 2026-09-25 evening (Jason). The WROVER setup stays as it is. |
| 15 | Jumper / hookup wires | 1 set | On hand for hookup wire; male-male and male-female Dupont jumpers are in doc 07 A13. |
| 17 | 6.3 mm female spade terminals | 6 of 100 | Thermostat's three tabs. |
| 18 | DC barrel plug to bare wire | 1 | No longer needed for the blower: the Delta terminates in a 4-pin fan connector (doc 07 A2). Keep for bench supplies. |
| 19 | USB-C data cable | 1 | On hand. Two are useful: the board has two USB ports. |
| 20 | Multimeter | 1 | On hand. |
| 21 | Heat gun or lighter | 1 | On hand. |

## C. Deferred (not needed for bring-up)
- Enclosure (IP-rated box), cable glands, panel LED, button for BLE pairing.
- Meat probes: now thermistor plus ADS1115, doc 07 section B.
- Custom PCB or protoboard for the final build.
- Current-sense shunt for blower stall detection: dropped, the Delta's tach covers it.
- Display.

## While waiting for parts (no hardware needed)
- DONE: framework decided (PlatformIO + Arduino-ESP32 3.x via pioarduino, doc 05).
- DONE: toolchain installed on JASON-PC; firmware builds, flashes and runs; no extra USB-UART driver was needed.
- DONE 2026-09-24: duct geometry measured (doc 01 section 7a).
- STILL OPEN: side port diameter, brass fitting thread OD, thermostat continuity check across the three spade tabs, exhaust stack damper, fan-off draft test.
