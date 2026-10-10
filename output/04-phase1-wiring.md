# Phase 1 - Electrical Wiring (bench circuit)

Created: 2026-09-20. Updated 2026-09-24: pin maps for the Freenove ESP32-WROVER (camera) board, plain WROOM-32, and ESP32-S3.
Updated 2026-09-24 (later): blower DECIDED as the Delta BFB0712HH-AWPH, 12 V 4-wire PWM + tach. The power and blower sections are rewritten for it; the 6 V brushed wiring is kept as [LEGACY] for the first bench session.
Diagram (artifact, S3 pin labels, still shows the legacy brushed blower): https://claude.ai/artifact/Ru4Xnd4WuaMavozzZwdGZH
Source file kept in session: smoker-wiring.html (republish to same URL to update).
Updated 2026-09-25: the bench board is now the ESP32-S3-DevKitC-1 clone (map C), flashed and validated (doc 05). The WROVER and WROOM maps are fallbacks. Bench env was s3-n8r8-uart-max31855; both MAX31855 boards failed the shorted-input test and were returned 2026-09-26 (doc 03), so the bench env becomes s3-n8r8-uart (MAX31856 on the CH343 console) when the MAX31856 arrives. Breadboards are on hand (2026-09-25). Update 2026-10-01: bench work is on the Freenove WROVER (map A, env freenove-wrover); the only thing wired is MAX31856 board 1 per map A, which passed inspection (doc 05). Update 2026-10-04: the bench board is the S3-N8R8 again (map C, env s3-n8r8-uart, Jason's decision) and the WROVER is no longer used; MAX31856 board 1 is wired to the S3 per map C and passed console `i` there. Update 2026-10-05: the 12 V input chain, the fan on straight 12 V and the IRLZ44N supply cut are on the breadboard; no fan lead is connected to the S3.

## Power
12 V 2 A adapter -> 5.5x2.1 panel jack -> 2 A fuse -> reverse-polarity Schottky (SS34; on the bench a single 1N5819, never two in parallel, doc 03) -> +12 V node
+12 V -> thermostat closed pair -> 1 A fuse -> fan RED (+12 V). LM2596 A is DELETED for the Delta: the fan runs straight off the input rail.
+12 V -> LM2596 B (set 5.0 V) -> ESP32 5V / VIN pin (Freenove extension board: the 5V rail terminals)
ESP32 3V3 -> thermocouple board VCC/VIN (Freenove extension board: EXT-3.3V terminals)
Common ground: adapter -, buck GND, ESP32 GND, thermocouple board GND, MOSFET sources, 10k pull-downs. Star at the buck output.
[LEGACY, 6 V brushed blower only] +12 V -> LM2596 A (set 6.0 V) -> thermostat closed pair -> blower +

## Blower loop, Delta BFB0712HH-AWPH (4-wire brushless)
Lead colours, VERIFIED from the Delta spec sheet 2026-09-25 (300 mm UL1061 AWG24, bare ends, no connector fitted): RED = +12 V, BLACK = GND, BLUE = PWM, YELLOW = FG (tach). FG is open collector, 5 mA max, 4 poles; the FG waveform in the drawing shows four equal intervals per revolution, and 2 pulses per revolution was CONFIRMED on the bench 2026-10-10 (doc 05 "S3 fan bench"). PWM signal range 0 to 5.5 V, high 2.8 V minimum, preferred 25 kHz. Mechanically the fan has only TWO mounting holes, 4.5 mm diameter on 7.7 mm bosses, set diagonally; the outlet is a bare 24.1 x 17.5 mm rectangle with no flange or adapter.
Rule: never chop the supply of a brushless fan for speed control. Speed comes from the PWM input only; the MOSFET becomes a fault-only supply cut.
- fan RED -> +12 V through the thermostat contact and the 1 A fuse.
- fan BLACK -> IRLZ44N drain; IRLZ44N source -> GND. SUPPLY_EN GPIO -> 200 ohm -> gate, 10 k gate -> GND (fan unpowered while the MCU is in reset or during a fault).
- fan PWM lead -> open-drain driver: 2N7000 (or 2N7002 / BSS138) drain to the PWM lead, source to GND, PWM GPIO -> 200 ohm -> gate, 10 k gate -> GND. Firmware duty is INVERTED (drain low = 0 percent). The fan pulls this line up internally to at most 5.25 V. Do not drive it push-pull from 3.3 V and never apply 12 V to it.
- fan TACH (FG) lead -> 10 k pull-up to 3.3 V -> TACH GPIO (pulse counter). Open collector, 5 mA max. NOT TO BE WIRED AS DRAWN since 2026-10-05: with the supply cut off this lead sits at 10.85 V (doc 09 M9) and must not reach a GPIO directly. DECIDED 2026-10-09 (Jason): blocking diode in the YELLOW lead. Build it as proposed: one 1N5819 in series, cathode (stripe) toward the fan, with the 10 k pull-up and a further 10 k series resistor on the GPIO side; prove it with a meter first (GPIO-side node at about 3.3 V with the fan cut, and at or below 0.8 V with the fan running and FG low, the ESP32-S3 VIL ceiling of 0.25 x 3.3 V) before GPIO5 is connected. BUILT AND MEASURED 2026-10-10 with a 1N5822 (the one on hand, same job): 3.2 V at the GPIO-side row with 12 V on the fan and the supply cut; the tach then counted correctly under firmware (doc 05 "S3 fan bench"). The second 10 k series resistor was NOT fitted: the row goes to GPIO5 by a jumper, with only the 10 k pull-up. The two 10 k meter loads that had been soldered inline on YELLOW and BLUE for M9 were cut out first.
- No flyback diode on this fan; it has its own commutation electronics.
- Datasheet facts that drive the firmware: rotor stops at 0 percent duty; with the PWM lead DISCONNECTED the fan runs at FULL speed; minimum start duty at 12 V and 25 C is at most 30 percent (MEASURED 2026-10-10: starts at 10 percent with the kick, runs at 10, stops at 5); 4400 RPM +/- 10 percent (MEASURED 4615 at 100 percent); 4 poles, 2 pulses per revolution CONFIRMED 2026-10-10.
- Lead colours: RED = +12 V and BLACK = GND confirmed 2026-10-05, the fan ran on straight 12 V with BLUE and YELLOW open. BLUE = PWM and YELLOW = FG CONFIRMED 2026-10-10 under firmware: duty on BLUE set the speed 10 to 100 percent, YELLOW counted 2 pulses per revolution (doc 05 "S3 fan bench").

[LEGACY] 6 V brushed blower: blower - -> IRLZ44N drain, source -> GND; 1N5819 across the blower, anode on blower -, cathode on blower +; PWM GPIO -> 200 ohm -> gate, 10 k gate -> GND.

## Fan control pins by board (Delta)

| Role | Freenove WROVER | WROOM-32 | ESP32-S3 | Note |
|---|---|---|---|---|
| PWM out (to 2N7000 gate) | GPIO2 | GPIO4 | GPIO4 | same pin as the legacy PWM |
| TACH in (10 k pull-up to 3.3 V) | GPIO15 | GPIO34 | GPIO5 | WROVER: GPIO15 already has an internal pull-up and tolerates the external one. Do NOT use GPIO12 for tach: a pull-up there selects 1.8 V flash and the board will not boot. WROOM-32 GPIO34 is input-only, fine for an open-collector signal with an external pull-up |
| SUPPLY_EN out (to IRLZ44N gate) | GPIO12 | GPIO13 | GPIO6 | WROVER GPIO12 is safe as an output because the 10 k gate pull-down holds it low at boot |

## Pin map A: Freenove ESP32-WROVER camera board (FALLBACK), env freenove-wrover-max31855
The OV2640 camera has no power-down pin on this board and is wired to GPIO 4, 5, 18, 19, 21, 22, 23, 25, 26, 27, 34, 35, 36, 39 (it is a plug-in module: removing it frees those pins, see map A2) (VERIFIED: Freenove_ESP32_WROVER_Board camera_pins.h, CAMERA_MODEL_WROVER_KIT). PSRAM uses 16, 17. None of those are used below. Strapping pins 0, 2, 12, 15: GPIO2 carries PWM and is held low at boot by the gate pull-down, which is the state the boot ROM wants.

| ESP32 GPIO (extension board label) | MAX31855 (5-pin clone) | MAX31856 (9-pin) | Role |
|---|---|---|---|
| 5V rail | - | - | from LM2596 B; or USB on the bench, not both |
| EXT-3.3V rail | VCC | VIN | sensor supply. MAX31855 clone has NO regulator: 3.3 V only |
| GND | GND | GND | |
| 14 | SCK | SCK | SPI clock |
| 13 | not connected | SDI | MOSI (MAX31855 is read-only) |
| 33 | SO | SDO | MISO |
| 32 | CS | CS | chip select, active low |
| 2 | - | - | 200 ohm -> 2N7000 gate (Delta PWM) or IRLZ44N gate (legacy blower) |
| 15 | - | - | fan TACH in, 10 k pull-up to 3.3 V |
| 12 | - | - | 200 ohm -> IRLZ44N gate, fan SUPPLY_EN |

## Pin map A2: Freenove WROVER with the CAMERA REMOVED (optional)
The OV2640 is a plug-in module, so pulling it frees every camera GPIO: 4, 5, 18, 19, 21, 22, 23, 25, 26, 27, 34, 35, 36, 39 (VERIFIED camera map: XCLK 21, SIOD 26, SIOC 27, Y9 35, Y8 34, Y7 39, Y6 36, Y5 19, Y4 18, Y3 5, Y2 4, HREF 23, PCLK 22, VSYNC 25). Whether the connector is an FPC latch or a header is NOT VERIFIED in the Freenove docs reached; look before pulling.
This board has NO onboard microSD slot (VERIFIED), so 2, 12, 13, 14, 15 carry nothing else. GPIO2 does drive the onboard IO2 LED.
Why bother: it moves the fan pins off the strapping lines and off the LED pin.

| Role | Pin with camera fitted (map A) | Pin with camera removed |
|---|---|---|
| PWM out | 2 (strapping, onboard LED) | 25 |
| TACH in | 15 (strapping) | 39 (input only, external 10 k pull-up, which the open-collector tach needs anyway) |
| SUPPLY_EN out | 12 (strapping, safe only because of the gate pull-down) | 26 |
| SPI SCK / SDI / SDO / CS | 14 / 13 / 33 / 32 | unchanged |

Still true with the camera out: GPIO 34 to 39 are input only, and 0, 2, 12, 15 keep their boot-strap rules.

## Pin map B: plain ESP32-WROOM-32 dev board (no camera), env wroom32-max31855
Arduino-ESP32 VSPI defaults, VERIFIED variants/esp32/pins_arduino.h (SS 5, MOSI 23, MISO 19, SCK 18). GPIO5 as CS is acceptable (MCU drives it after boot).

| ESP32 pin | MAX31855 | MAX31856 | Role |
|---|---|---|---|
| VIN / 5V | - | - | from LM2596 B |
| 3V3 | VCC | VIN | sensor supply |
| GND | GND | GND | |
| GPIO18 | SCK | SCK | SPI clock |
| GPIO23 | not connected | SDI | MOSI |
| GPIO19 | SO | SDO | MISO |
| GPIO5 | CS | CS | chip select |
| GPIO4 | - | - | PWM to gate |
| GPIO34 | - | - | fan TACH in (input only), 10 k pull-up to 3.3 V |
| GPIO13 | - | - | fan SUPPLY_EN to IRLZ44N gate |

## Pin map C: ESP32-S3-DevKitC-1 (YEJMKJ N8R8), TARGET BOARD, env s3-n8r8-uart (console on the CH343 UART bridge; the s3-n8r8 and s3-n8r8-max31855 envs put Serial on the native USB port instead)
Arduino-ESP32 FSPI defaults. Avoid strapping pins 0, 3, 45, 46.

| S3 pin | MAX31855 | MAX31856 | Role |
|---|---|---|---|
| 5V | - | - | from LM2596 B |
| 3V3 | VCC | VIN | sensor supply |
| GND | GND | GND | |
| GPIO12 | SCK | SCK | SPI clock |
| GPIO11 | not connected | SDI | MOSI |
| GPIO13 | SO | SDO | MISO |
| GPIO10 | CS | CS | chip select |
| GPIO9 (opt) | - | DRDY | data ready |
| GPIO8 (opt) | - | FLT | fault |
| GPIO4 | - | - | PWM to gate |
| GPIO5 | - | - | fan TACH in, 10 k pull-up to 3.3 V |
| GPIO6 | - | - | fan SUPPLY_EN to IRLZ44N gate |

Firmware selects the map by IDF target plus -DBOARD_FREENOVE_WROVER (include/pins.h); sensor backend by -DPIT_SENSOR_MAX31855 (firmware README).

## Thermocouple
K-type probe -> T+ (yellow) / T- (red) on the screw terminal. THE BENCH PROBE HAS NO COLOURS: its connector was cut off and the two leads are bare metal. Polarity was set by the magnet test (doc 05, WROVER bench session): the CRIMPED lead is magnetic, so crimped = T- (negative), plain = T+ (positive). If the reading falls when the tip is warmed, the leads are swapped. Ungrounded junction required; leads not grounded anywhere; route away from the fan supply lead. MAX31855: no filter or config registers; fault bits OC / SCG / SCV come in the 32-bit frame and map to SENSOR_OPEN / SENSOR_SHORT. EGT probe (2026-10-04, S3): yellow lead on T+, red on T-, confirmed by the pinch test; the red lead is magnetic; sheath to leads open (ungrounded).

## Duct coupling (measured 2026-09-24)
Duct: 15/16 in (23.81 mm) OD, 0.85 mm wall, 22.1 mm bore, too thin to thread. The existing blower couples by sliding a 1 in ID sleeve about 1 in over the duct, and that is the pattern to copy: 1 in ID silicone or EPDM hose with two worm clamps. The Delta needs a rectangular-outlet-to-1-in-round transition. DESIGNED 2026-09-25, print IN PROGRESS: fan outlet 24.1 x 17.5 mm (VERIFIED from the Delta spec drawing, page 5) to a 23.8 mm OD stub with a retention bead; single printed piece, STL at output\3d-print\blower-adapter-delta-BFB0712HH-to-1in-duct-v1.stl, PLA as a fit-check part (reprint PETG or ABS if the duct inlet runs warm on the first cook). Confirm the outlet with calipers on arrival. Put the backdraft flap inside this coupling, between the stub and the duct; use a 4 in or 6 in hose so there is room for it.

SUPERSEDED 2026-10-02: the real outlet is 39.90 x 25.00 mm outside with a 35.90 x 17.50 mm bore, not 24.1 x 17.5. Current adapter is v6 direct, a socket over the fan outlet and a socket over the duct, no hose (output\3d-print\README-blower-adapter-v6.md). Flap deferred by Jason for the first tests.

## Before power-up
- Set LM2596 B to 5.0 V with a meter before connecting the board. LM2596 A is only used if the legacy 6 V blower is fitted.
- Confirm the fan PWM lead is driven open-drain only, and that SUPPLY_EN is low at reset.
- 2026-10-04, before the YELLOW (FG) or BLUE (PWM) lead goes to a GPIO: with 12 V on the fan and SUPPLY_EN low, measure both leads against controller GND (doc 09 M9). The supply cut is in the ground lead, so the fan's own ground floats when it is off. Firmware (built 2026-10-04) keeps the PWM GPIO low while SUPPLY_EN is low, raises SUPPLY_EN only after the PWM value is set, and drops it in IDLE and FAULT. RESULT 2026-10-05: both leads 10.85 V, the same into a 10 k load (doc 09 M9); YELLOW needs the fix in the blower section before it meets FAN_TACH. Never tie BLUE or YELLOW straight to ground while the cut is off: that feeds the fan through its signal leads.
- A broken PWM lead makes this fan run at full speed, so the tach check plus the SUPPLY_EN cut are the protection, with the thermostat as the backstop.
- Identify thermostat closed pair by continuity at room temperature; verify it opens with heat on the bulb.
- Do not feed the ESP32 from USB and the 5V/VIN pin simultaneously unless the board's diode-OR is confirmed.
- MAX31855 clone: confirm the chip marking before power; a MAX6675 on the same footprint reads a different frame and will show as "not detected" or wrong temperatures.
- Freenove WROVER: leave the camera ribbon connected or disconnected, it does not matter for the pins above; do not use any camera GPIO for anything else.
