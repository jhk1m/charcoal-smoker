# Phase 1.5 / Phase 2 - Second Purchase List

Created: 2026-09-21
Scope: everything doc 06 proposes that is not in doc 03. Nothing below is needed for bench steps 1 to 5 (boards arrive 2026-09-23); all of it is for the first real cook and after.
Prices: VERIFIED means fetched from the vendor page on 2026-09-21. "price NV" means not verified; spec is what matters.
Blocked items: marked WAIT, they depend on measurements not yet taken (duct inlet ID, current nozzle OD, exhaust stack OD, whether the stack has a damper, fire door seal).

## A. Before the first real cook (Phase 1.5)

| # | Item | Spec | Qty | Price | Status |
|---|---|---|---|---|---|
| A1 | Replacement blower DECIDED 2026-09-24 | Delta BFB0712HH-AWPH: 12 V, 0.34 A rated / 0.42 A max, 15.3 CFM free air, 0.977 inH2O max static, 4400 RPM +/- 10 percent, 75.7 x 75.7 x 25 mm, 90 g, two ball bearings, 4-wire PWM (preferred point 25 kHz, signal 0 to 5.5 V, high 2.8 V min) + FG tach (open collector, 5 mA max, 4 poles), -10 to 70 C, min start duty at most 30 percent, rotor stops at 0 percent, runs FULL speed if the PWM lead is open. MECHANICAL, VERIFIED from the Delta spec sheet 2026-09-25: outlet opening 24.1 x 17.5 mm rectangular, TWO mounting holes only, 4.5 mm dia (7.7 mm bosses), diagonal; leads are 300 mm of bare-ended UL1061 AWG24 with NO connector fitted, RED +12 V, BLACK GND, BLUE PWM, YELLOW FG | 1 | listed at Digi-Key, Mouser and Arrow; price and stock NOT VERIFIED | ORDER |
| A1a | LOOKALIKE PART WARNING 2026-09-25 | Order the exact string BFB0712HH-AWPH. Both the second H and the -AWPH suffix matter. The cheap marketplace listings for BFB0712H are a different fan: 2900 RPM, 11.30 CFM, max static 0.480 inH2O, TWO wires, no PWM input and no tach (Delta spec BFB0712H, issue feb.20.2004). Against this duct it intersects the system curve at about 3.8 CFM and would need roughly 74 percent duty to hold the 2.8 CFM reference, against 53 percent for the HH, and it cannot be speed-controlled at all without supply-chopping a brushless motor, which R3 in doc 06 forbids. BFB0712H-A (4000 RPM, 25 mm) and BFB0712H-F00 (2900 RPM, 30 mm, 3 lead) are also not the part | - | - | DO NOT SUBSTITUTE |
| A2 | Fan drive parts | Small-signal switch for the PWM lead, x2: 2N7000 TO-92 (breadboard friendly), or BSS138 / 2N7002 in SOT-23, or a 2N3904 class NPN with a 4.7 k base resistor as an open-collector inverter. This is NOT the same job as the IRLZ44N and cannot be done by it, see A2a. 10 k pull-up and 200 ohm gate resistors (on hand). NOTE 2026-09-25: the fan ships with bare wire ends, not a plug, so no fan header is needed. Terminate the four AWG24 leads with a 4-pin JST-XH or Dupont crimp set, or solder and heatshrink direct to the board | 1 set | a few dollars, price NV | ORDER |
| A2a | Why the IRLZ44N cannot drive the PWM lead (added 2026-09-25) | Two different jobs. The IRLZ44N is the fault-only SUPPLY CUT in the fan BLACK lead: 12 V, 0.42 A, static on or off, and it is the right part for that. The PWM lead is a 25 kHz logic line sinking about 1 mA, and it must be open-drain because the fan pulls it up internally to as much as 5.5 V (Delta spec, signal range 0 to 5.5 V) while the ESP32-S3 IO pins are not 5 V tolerant, so no direct GPIO connection. Speed rules the IRLZ44N out as well: Qg 48 nC max, Qgd Miller 25 nC max, Ciss about 1700 pF typ (Infineon IRLZ44N datasheet). Through a 200 ohm gate resistor from 3.3 V the Miller plateau current is about 6 mA, so the edge takes roughly 4 us; even with no gate resistor and the pin at its maximum drive it is still hundreds of ns. Delta permits 500 ns maximum rise and fall on the PWM input and recommends 50 ns. At a 40 us period a microsecond-class edge is a duty error, not a rounding error. The 2N7000 is Ciss 60 pF max with 10 ns turn-on and turn-off (onsemi 2N7000 datasheet), which is three orders of magnitude easier. One caveat: 2N7000 VGS(th) is specified 0.8 to 3.0 V and its on-current is specified at VGS 4.5 V, so a 3.3 V gate is not a guaranteed-on condition at the worst-case part. At the roughly 1 mA this load needs it works in practice. If that margin is unwanted, BSS138 has VGS(th) 0.8 to 1.5 V (onsemi BSS138 datasheet) but is SOT-23, or use the NPN option, which is fully specified at 3.3 V drive | - | - | INFORMATIONAL |
| A3 | Blower-to-duct coupling | Copy the existing pattern: a 1 in ID sleeve slides about 1 in over the 23.81 mm OD duct. Buy 1 in ID silicone or EPDM hose (300 mm) and four worm clamps. The Delta has a bare rectangular outlet and Delta offers no adapter, so a transition piece must be made: 24.1 x 17.5 mm rectangle (outlet, VERIFIED from the spec drawing) to a round stub about 24 mm OD and 25 mm long, so the same 1 in ID hose clamps over it. Outlet area 422 mm2 against the 384 mm2 duct bore, so the transition is close to area-neutral; keep it under about 30 mm long. DESIGNED 2026-09-25: output\3d-print\blower-adapter-delta-BFB0712HH-to-1in-duct-v1.stl, 36 x 29 x 61.5 mm, 9.54 cm3, locating lip 23.3 x 16.7 into the fan outlet, 36 x 29 x 3 mm flange, 25 mm transition, stub OD 23.8 / ID 21.0 with a 25.0 mm retention bead. Prints stub-down with no supports. Submitted to Arlington Public Library The Shop in PLA (about 12 g, about $0.60 at $0.05/g). PLA is a fit-check and first-cook part only: it softens near 55 to 65 C, so measure the duct inlet temperature on the first cook (R12) and reprint in PETG or ABS if it runs warm. Confirm the 24.1 x 17.5 outlet with calipers on the real fan; gen_adapter.py regenerates the STL from measured numbers. SUPERSEDED 2026-10-02: the real outlet measures 39.90 x 25.00 mm outside with a 35.90 x 17.50 mm bore, 2.25 mm off centre, so the 24.1 x 17.5 figure and the v1 plug-in design are wrong. Current design is blower-adapter-v6-direct.stl, one piece, socket over the fan outlet and socket over the duct, no hose (output\3d-print\README-blower-adapter-v6.md). The v6 STL is not in the repo, and v6 leaves the backdraft flap (A4) with no location; both open | 1 | HPS coupler $9.47 to $11.85 VERIFIED at hpsperformanceproducts.com 2026-09-25; local heater hose about $10 to $15, price NV | ORDER |
| A3a | Hose sourcing decided 2026-09-25 | Two routes, either works. LOCAL SAME DAY: 1 in ID automotive heater hose, 1 ft, from any auto parts counter, plus two stainless worm clamps; cheapest and immediate, typical heater hose is rated near 125 C which is ample on the cold side of the duct. MAIL ORDER, tighter part: HPS 4-ply reinforced silicone straight coupler, 1 in (25 mm) ID, 350 F (177 C) rated: HTSC-100-BLK is 3 in long at $9.47, HTSC-100-L4-BLK is 4 in at $11.85, HTSC-100-L6-BLK is 6 in at $15.60 (hpsperformanceproducts.com, VERIFIED 2026-09-25). Take the 4 in or 6 in: each male end takes about 25 mm of engagement, so a 3 in coupler leaves almost no gap for the flap. Clamps must close on the hose OD over a 23.8 mm stub, about 35 mm on the 4-ply silicone, so pick a clamp whose range covers 30 to 40 mm and buy four | 1 | see above | ORDER |
| A4 | Backdraft flap | Fits inside the A3 coupling. MUST be near-zero cracking: at 2 to 3 CFM the duct runs at only 20 to 50 Pa, which is 1 to 2 gram-force on a 25 mm disc, so spring and swing check valves (tens of kPa) will never open. NO off-the-shelf part has been found that cracks this low at this diameter; it is a made part. MATERIAL DECIDED 2026-09-25 by calculation (cantilever 22 x 22 mm, silicone E about 3 MPa, uniform load): 0.4 to 0.5 mm silicone sheet lifts 19 to 37 mm at the tip under 20 Pa and carries 7 to 8 times its own weight at 50 Pa, so it opens fully and still closes by gravity. 0.8 mm lifts only 4.6 mm at 20 Pa, and 1.5 mm is useless at 0.7 mm. Buy silicone sheet 0.4 to 0.5 mm (0.016 to 0.020 in); 2 to 4 mil PET or FEP film with a tape hinge is the near-weightless alternative. The salt-weighted ping-pong ball check stays the fallback. Verify by blowing through it by mouth | 1 | under $10, price NV | ORDER material |
| A5 | (merged into A2) | | | | |
| A6 | (merged into A2) | | | | |
| A7 | Blower rail fuse | 1 A fast-blow 5 x 20 mm (check whether the BOJACK 150-fuse set on hand includes 1 A) | 2 | on hand if included | Check kit |
| A8 | Reverse-polarity diode | SS34 Schottky 3 A 40 V (doc 03 item 13, still not bought) | 2 | price NV | Buy |
| A9 | Gasket for fire door and lid | High-temp fiberglass or Nomex BBQ gasket tape, self-adhesive, 1/2 to 3/4 in wide, rated 260 C or higher | 1 roll | price NV | Buy; E2 draft control |
| A10 | Exhaust damper | Stove-pipe style butterfly damper matching the stack OD, or a slide cap | 1 | price NV | WAIT: stack OD and whether one exists |
| A11 | Pit probe reach | EGT probe RECEIVED 2026-10-02, untested. 2026-10-03: it enters through a second 1/8 NPT hole tapped in the existing port block beside the thermostat bulb (doc 09 M6); 3.2 in reaches roughly 7 cm inboard, short of the 8 cm target, accepted for the first tests. If the 1/8 NPT EGT probe is too short to reach 8 cm inboard at grate height, Procon B09HSJ4PBB 5 x 80 mm (doc 03 fallback) | 1 | $19.99 (doc 03) | Decide after the probe port is measured |
| A13 | Breadboard and jumpers | 830-point solderless breadboard plus male-male and male-female Dupont jumpers. Doc 03 listed one as on hand; the one that exists is too short for the S3 by two pins, see doc 03 item 14. The S3 needs 22 rows of terminal strip and an 830-point board has 63 | 2 | 854-BB830 at $8.95 plus $3.40 tariff each, ORDERED 2026-09-25 | DONE, breadboards ON HAND (Jason, 2026-09-25 evening) |
| A12 | Kitchen or luggage scale | For weighing the charcoal basket before and after cooks (fuel gauge calibration, doc 06 section 1b) | 1 | on hand? | Confirm |

## B. Cook Manager (Phase 2, meat probes)

Decision: thermistor probes plus an external ADC, not K-type. Food-rated K-type probes cost $69 to $151 each (ThermoWorks smokehouse $69 to $79, Cooper-Atkins 50361-K $151.47, VERIFIED). ThermoWorks Pro-Series thermistor probes are $21 each with the same HeaterMeter-proven Steinhart-Hart coefficients (7.3431401e-4, 2.1574370e-4, 9.5156860e-8, 10 k pull-up; hmcore.cpp VERIFIED). (The two MAX31855 boards both failed the shorted-input test and were returned, 2026-09-26; there are no spares.)

| # | Item | Spec | Qty | Price | Status |
|---|---|---|---|---|---|
| B1 | Meat probes | ThermoWorks Pro-Series High Temp Cooking Probe TX-1001X-OP, thermistor, cable rated 370 C, +/- 1 C from -20 to 120 C | 2 | $21.00 each VERIFIED | Buy |
| B2 | ADC | ADS1115 16-bit 4-channel I2C breakout (Adafruit 1085 $14.95 VERIFIED; clones exist, price NV) | 1 | $14.95 | Buy; the ESP32-S3 internal ADC is usable but nonlinear, ADS1115 removes that risk |
| B3 | Probe jacks | 2.5 mm mono panel jacks (ThermoWorks plug size NOT VERIFIED; measure the plug before ordering) | 2 | price NV | WAIT: measure plug |
| B4 | Divider resistors | 10 k 0.1 percent (or 1 percent, then calibrate in ice and boiling water) | 4 | price NV | Buy |
| B5 | Buzzer | Piezo active buzzer 3 to 5 V, panel mount, for local alarms when the phone is away | 1 | price NV | Buy |
| B6 | Pairing button | Momentary pushbutton, panel mount, IP-rated | 1 | price NV | Buy |

## C. Enclosure and final build (Phase 2, after the first cooks)

| # | Item | Spec | Qty | Price | Status |
|---|---|---|---|---|---|
| C1 | Enclosure | IP65 ABS or polycarbonate box, about 150 x 100 x 70 mm, clear lid optional, clamps to the left post beside the thermostat box | 1 | price NV | Buy after PCB/protoboard size is known |
| C2 | Cable glands | PG7 (probe leads, 3 to 6.5 mm) x3, PG9 (blower cable, adapter cable) x2 | 5 | price NV | Buy with C1 |
| C3 | Panel LED | 5 mm or 8 mm 12 V panel indicator (state / fault) | 1 | price NV | Optional |
| C4 | Protoboard or PCB | Perfboard 70 x 90 mm for the MOSFET stage, bucks, ADS1115 | 1 | price NV | After bench |
| C5 | Servo damper (Phase 3) | HeaterMeter RotoDamper style: micro servo + printed damper; only if the flap in A4 is not enough | 1 | price NV | Defer |

## D. Not needed (removed or covered)
- 3/4 in NPT bung and drilling: removed 2026-09-21, the duct inlet is the interface.
- Second MAX31856 for meat: replaced by B1 + B2.
- Current-sense shunt for the blower: not needed, the Delta's tach covers stall and runaway detection.
- LM2596 A as a 6 V blower rail: deleted, the Delta runs on the 12 V input rail. Keep the spare bucks.
- FireBoard Drive blower, its ball valve adapter and nozzle adapter: ruled out (20 CFM unusable through a 22 mm bore, static pressure unpublished, rectangular outlet, no tach).
- Display: still deferred; BLE app is the display.

## Order of purchase (rewritten 2026-09-25: consolidate, do not source item by item)

SOURCING RULE, adopted 2026-09-25 after ordering was fragmented across vendors: every electronic part in this
project is ordered from ONE distributor in ONE cart. Mouser is the distributor, because it stocks the Delta
blower, which is the part that dictates the choice. Only items Mouser does not carry go elsewhere, and those
are grouped into as few orders as possible. Do not quote a part from a marketplace listing when the same part
is a line item at Mouser.

STATUS 2026-09-25: ALL ORDERED. Blower ordered 2026-09-24; breadboards and 2N7000s 2026-09-25; the remaining Mouser lines, the local hose, clamps and flap material all placed. Nothing further to buy until a measurement or a bench result forces it (A10 damper, B3 jacks, C1 to C4, and the four known risks in doc 09).

### Order 1, Mouser, one cart (part numbers and prices VERIFIED at mouser.com 2026-09-25)

| Mouser P/N | Item | Maps to | Qty | Unit | Stock at check |
|---|---|---|---|---|---|
| 108-BFB0712HH-AWPH | Delta BFB0712HH-AWPH blower | A1 | 1 | $23.59 | 514 |
| 512-2N7000 | 2N7000 N-MOSFET TO-92, onsemi. ORDERED 2026-09-25, qty 20 at $0.293. Cheaper than the Microchip 689-2N7000-G ($0.50) quoted first; same 60 V / 5 ohm part | A2 | 20 | $0.293 | ordered |
| 625-1N5822-E3 | 1N5822 Schottky 3 A 40 V, DO-201AD axial, Vishay | A8 | 2 | $0.91 | 3,716 |
| 854-BB830 | BusBoard BB830 solderless breadboard, 830 tie points. ORDERED 2026-09-25, qty 2 at $8.95 plus $3.40 each tariff | A13 | 2 | $8.95 | ordered |
| 854-KIT-ZW-20X3 | ZipWire jumper kit, 120 wires, 20 cm assorted | A13 | 1 | $15.55 | 328 |
| 392-333095 | Soldered ADS1115 16-bit 4-channel ADC breakout | B2 | 1 | $11.80 | 32 |
| 490-CPT-2416-85T | Same Sky piezo audio indicator with built-in drive, 12 V, 85 dB, 23.8 mm | B5 | 1 | $4.62 | 944 |

Cheaper jumper alternative: 392-100865, Soldered male-female 40-piece set, $3.75, 406 in stock.
NOTE on A8: Mouser stocks SS34 only in surface-mount packages. The 1N5822 is the through-hole equivalent at the
same 3 A / 40 V and is the correct part for breadboard work, so SS34 is superseded.
B5 buzzer chosen 2026-09-25: 490-CPT-2416-85T, piezo so there is no inductive kick to clamp; drive it low-side
from a 2N7000. B6 the panel pushbutton is deliberately NOT in this order: its cutout and IP rating have to match
the enclosure, so it is bought with C1.
A8 CLOSED 2026-09-25: the reverse-polarity diode already on hand is the Chanzon 1N5819, 1 A 40 V, not 3 A. Worst
case input current is fan 0.42 A plus about 0.25 A for the 5 V rail through the buck, so about 0.67 A, which the
1 A part carries with roughly a third of margin. One 1N5822 (625-1N5822-E3, $0.91) is optional insurance, not a
requirement. Two 1N5819 in parallel is NOT the fix: paralleled diodes do not share current evenly.
B4 CLOSED: 10 k 1 percent resistors are on hand. Use them and calibrate in ice and boiling water, the HeaterMeter
method. Do not buy 0.1 percent parts.
Mouser's US free-shipping threshold is NOT VERIFIED; their shipping help page was unavailable when checked.

ORDER 1 STATUS 2026-09-25 (confirmed by Jason, evening, twice): ALL lines placed, including the blower, 1N5822, jumper kit and ADS1115. Breadboards are on hand. The fan adapter print (A3) is in progress; STL in output\3d-print\. An earlier note saying only the breadboards and 2N7000s had gone in was wrong and keeps reappearing from an older mirror of this doc; do not restore it.

RECEIVED 2026-10-02 (Jason, read off the bag labels): 1N5822-E3/73 (Vishay, 40 V 3 A; A8, the /73 suffix is as labeled, the order line above reads 625-1N5822-E3), Soldered 333095 ADS1115 breakout (B2), Same Sky CPT-2416-85T buzzer (B5). 1N5822 count: 2, as ordered. Also received 2026-10-02: the Delta blower (A1). Its label reads "BFB0712HH-A DC12V 0.42A" (Jason); CONFIRMED by Jason the same day: it is the -AWPH part and has four leads, so it is the ordered 4-wire fan, not the 2-wire one in A1a. 0.42 A matches the datasheet maximum current. Outlet caliper check (24.1 x 17.5 mm) still owed. Also received 2026-10-02: onsemi 2N7000 x20 (A2), 60 V 5 ohm TO-92 per the label. NOT reported as received: ZipWire jumper kit. None of the arrivals has been powered or tested.

### Order 2, local, one trip, no shipping
1 in ID heater hose (1 ft) and four worm clamps from an auto parts counter; silicone sheet 0.4 to 0.5 mm for the
flap (A4); high-temp gasket tape (A9) if the local store has it. See A3a for the mail-order hose alternative,
which is only worth it if the local hose is unavailable.

### Order 3, ThermoWorks only
B1 probes x2, TX-1001X-OP. No distributor carries them. Place with B3 jacks once the plug is measured.

### Free
3D printed adapter: Arlington Public Library The Shop, about $0.60 in PLA.

### Still blocked
A10 exhaust damper (needs the stack OD), B3 probe jacks (measure the ThermoWorks plug), C1 to C4 (after the first two cooks).

### Proposed in the Cowork session 2026-09-26, order status NOT recorded
A14 OSOYOO ESP32-S3 IO breakout board: 2x22 socket for the 44-pin DevKitC-1, on-board 5 V/3 A and 3.3 V/3 A from a 7 to 18 V barrel jack, screw terminal per GPIO, SI2302 reverse-polarity protection, V_Selector jumper (keep on 3.3 V). If bought it replaces the LM2596 5 V rail (doc 03 item 9) and moves pin map C from breadboard rows to S/V/G terminals; the 12 V fan branch taps the input screw terminal (thermostat, 1 A fuse, fan RED) and sits outside the board's protection, which the Delta tolerates (spec section 5-2). The fan drive parts still need a breadboard or perfboard. Confirm whether it was ordered and record it here.

## Measurements that unblock the WAIT items
- DONE 2026-09-24: duct 23.81 mm OD, 0.85 mm wall, 22.1 mm bore, 20 in run with two bends; existing blower couples with a 1 in ID sleeve, about 1 in of engagement.
- DONE 2026-09-25 from the spec drawing: Delta outlet 24.1 x 17.5 mm, two 4.5 mm mounting holes, 300 mm bare leads. Re-check the outlet with calipers on arrival before printing the transition.
- Exhaust stack OD and whether it has a cap, slide, or nothing.
- Fire door: does it close on a gasket surface, or metal on metal with gaps?
- ThermoWorks probe plug diameter (2.5 mm expected, not verified).
- Fan-off draft test result (doc 01 open items).
