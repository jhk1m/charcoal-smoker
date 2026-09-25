# Charcoal Smoker - Current System Baseline (Phase 0)

Captured: 2026-09-20
Updated: 2026-09-21 (air path corrected from Jason's annotated photo)
Status: as-built reference before Phase 1 (ESP32-S3 + PWM + BLE) redesign
Sources: vendor technical reply (기술제안서 답변, project files charcoal-smoker-v1-pt1..pt4.png), Jason's photos (2026-09-20_01_01_*.png, annotated drum photo), Gumsung Controls catalog (gscontrol.kr, fetched 2026-09-20)

## 0. PROTOTYPE SCOPE (decided 2026-09-20)
The prototype platform is the stainless DRUM smoker (Jason's garage unit), not the vendor's rotisserie cabinet.
The drum unit has NO 220 V PSU, NO transformer / 12 V rail, NO lights, NO rotation motor.
Electrical content of the prototype platform is therefore only:
- Gumsung capillary thermostat (label "03-3203 5", see section 3) in its stainless dial enclosure, clamped to the left vertical support post.
- Capillary bulb in a brass compression fitting on a stainless block, inserted at the small port on the drum wall beside the dial thermometer (center of cooking zone height).
- 6 V brushed blower (800 mA max) fed by the 6 V / 800 mA adapter (or 4 x AA in the camping variant). Nozzle inserts into the top of the external air duct (section 7a), NOT into the fire door.
Sections 1 and 4 below describe the vendor's cabinet product and are reference only; they do not apply to the prototype hardware.

DECISION 2026-09-20: SINGLE SUPPLY. One external DC adapter feeds the controller board; the board derives the blower rail (6 V) and the logic rail (3.3 V / 5 V) on-board. The existing 6 V / 800 mA adapter is retired from the prototype. Input voltage and adapter rating to be fixed in the Phase 1 power budget.

DECISION 2026-09-20: KEEP THE EXISTING THERMOSTAT as the Phase 1 hardware over-temp cutoff. It meets the role (mechanical, independent of software, contact rating far above the 6 V / 0.8 A blower load, 50 to 320 C range covers a cutoff above any cook target). Replace only if the capillary or bulb is damaged.

Consequences for Phase 1 design:
- No 12 V rail exists on the prototype; all rails come from the single adapter via on-board regulators.
- Mounting: the thermostat enclosure position on the post is a candidate location for the controller enclosure; the probe port is the candidate location for the digital probe.

## 1. Power system (vendor reply Q1, Q5) - cabinet product, reference only
- Mains input: AC 220 V / 60 Hz (rated supply).
- 220 V is used directly only for the power indicator lamp.
- All other electrics run from a transformer producing DC 12 V: internal rotation DC motor (rotisserie), internal halogen lamps, toggle switches.
- Blower: originally 4 x AA. Modified to a DC 6 V / 800 mA (rated output) wall adapter plugged into the 220 V outlet.
- Blower spec per vendor: voltage 6 V, max current 800 mA, motor type DC BRUSHED (DC브러쉬드).
- Blower does NOT run from the 12 V rail.

CORRECTION vs earlier summary: the vendor states the blower motor is brushed, not brushless. This removes the supply-side PWM concern; a plain low-side MOSFET PWM stage with flyback diode is appropriate.

## 2. Blower control (vendor reply Q2) - applies to prototype
- ON/OFF only. No PWM.
- Below setpoint -> blower ON. Setpoint reached -> thermostat contact opens -> blower OFF.
- The dial on the stainless box is the thermostat setpoint knob (0, 50 to 320 C), not a timer. No timer exists in the system.

## 3. Temperature controller (vendor reply Q3 + Gumsung catalog) - applies to prototype
- Mechanical capillary thermostat with dial knob, rated 250 V / 16 A.
- Manufacturer: Gumsung Controls Co (금성콘트롤), Korea. Catalog: gscontrol.kr.
- Label, as read by Jason from the unit: 모델명: 03-3203 5. Gumsung's public catalog does not list this string; the matching catalog part by range and rating is GS-320S (50 to 320 C, 250 VAC/16 A). Treat "03-3203 5" as the label text and GS-320S as the catalog equivalent.
- Label also shows: E239121 beside a CE mark (UL file-number format, not verified), 안전인증번호 XD020003-15001A (KC safety cert), mfg 2025.11, 정격 250V-16A, 제조국 한국.
- Catalog spec for GS-320S: range 50 to 320 C, differential 10 +/- 5 C, contact ratings 220 VAC/24 A, 250 VAC/16 A, 400 VAC/10 A, contact type SPST or SPDT, capillary 1000 mm standard (150 to 2000 mm option), bulb 5 mm dia x 65 mm or 3 mm dia x 169 mm. Circuit diagram and drawing PDFs exist on the product page (not yet downloaded).
- Three 6.3 mm spade terminals on the body, consistent with the SPDT variant (common + NC + NO). Which terminal is which: not verified; confirm by continuity at room temperature or from the Gumsung circuit PDF.
- Differential of 10 +/- 5 C means the failsafe will re-close roughly 5 to 15 C below the dial value once tripped.
- No MCU, no digital sensing, no PID, no software.

## 4. Sensor location (vendor reply Q4) - cabinet product
- Bulb positioned at the center of the cooking zone inside the cabinet.
- Mounting: a small stainless bracket/clip on the cabinet wall holds the bulb; the dial enclosure mounts on the vertical support post outside. Vendor states removable (탈부착 가능).
- The product is a rotating smokeless charcoal BBQ cabinet (회전 무연 숯불 바비큐, 청스텐아이디어뱅크 branding): glass door, halogen lamps top, toggle switches and pilot lamp on the top panel, side thermometer, blower + sensor on the post, valve, rotisserie racks, charcoal basket, brake casters.
- On the prototype drum the same parts mount as described in section 0.

## 5. Two blower configurations (vendor reply note)
- Same sensor, same ON/OFF method, same temperature setting on both.
- Camping/home: 4 x AA. Restaurant/commercial: adapter.

## 6. Disposition of existing controller (vendor reply Q6)
- Vendor: replacement or parallel use is possible (가능).
- Chosen direction: remove from normal regulation, keep in series in the blower power path as an independent hardware over-temperature cutoff.

## 7. Photo observations (Jason's photos, 2026-09-20)
- Prototype drum: stainless drum, Celsius dial thermometer approx 50 to 350 C, small open port beside it (probe port), hinged fire door at the bottom front with a round perforated charcoal basket in a solid base ring behind it, drip tray below, casters.
- Smaller second unit (not the prototype): stainless drum, glass lid, small bimetal gauge, hinged door; black red-fronted box on left post, identity not confirmed.
- Thermostat enclosure: stainless box, black setpoint dial, cable gland, gray lead to blower. Clamps to the drum's left post at the top of the air duct.
- Blower: black centrifugal, straight metal outlet nozzle, mesh intake, plastic handle, DC barrel jack on handle. Vendor photos show the AA compartment in the handle rewired to the jack.
- Probe: stainless bulb in a brass compression fitting on a stainless block, capillary to thermostat. Unscrews.

## 7a. Air path (CORRECTED 2026-09-21 from Jason's annotated photo; earlier text calling the tubes "unused" was wrong)
- INTAKE: the curved stainless tube is a forced-air duct. Inlet at the top left beside the thermostat box on the post; the blower nozzle inserts there. The duct runs down the outside left of the drum through a clamp ring and enters the fire chamber at the base, feeding the charcoal basket from below.
- EXHAUST: the straight centre tube rising from the lid is the exhaust stack.
- FIRE DOOR: for adding charcoal and removing ash only. Closed during a cook. It is not an air intake.
- The blower is therefore the single intended air path. This is the layout that every commercial fan controller assumes (one sealed intake, exhaust nearly closed).
- OPEN (measure): duct inlet ID and current nozzle OD; where the duct terminates under the basket; whether the exhaust stack has a cap or damper; whether the fire door seals.
- Consequence for the blower decision (doc 06 section 3): no drilling. The replacement blower mates to the duct inlet with a short coupling; the gravity backdraft flap goes at the duct inlet or inside the coupling.

## Current architecture (prototype drum)
```
6 V 800 mA adapter (or 4 x AA)
   -> thermostat contact (ON/OFF, dial = setpoint)
   -> 6 V brushed blower -> external air duct -> under charcoal basket -> chamber -> exhaust stack
   chamber temp -> capillary bulb at side port (feedback)
```

## Gaps (what the current unit lacks)
No PWM, no variable airflow, no MCU, no digital temperature, no PID, no state machine, no ramp-up/stabilize modes, no lid-open detection, no history, no digital setpoint, no fault/event log, no BLE, no software safety logic, no backdraft flap on the duct inlet.

## Phase 1 target (center of architecture)
Single DC adapter -> controller board (6 V blower rail + logic rail) -> ESP32-S3 (control + state) -> low-side MOSFET PWM -> 6 V brushed blower, with the existing thermostat retained in series in the blower rail as hardware over-temp cutoff (dial set above max cook temperature). Digital temp sensor at the probe port. See 02-phase1-architecture.md. Blower replacement (12 V, at the duct inlet) proposed in 06.

## Open items to verify before Phase 1 build
- Adapter input voltage and current rating for the single-supply design (power budget).
- Thermostat terminal pinout (which pair is closed below setpoint). Continuity check or Gumsung circuit PDF.
- Actual blower running current at 6 V (vendor gives 800 mA max only).
- Probe port: thread of the brass compression fitting and diameter of the side port, for a K-type thermocouple or RTD.
- Air duct inlet ID, nozzle OD, duct termination, exhaust stack damper, fire door seal (section 7a).
- Fan-off draft test: fire lit, blower unplugged, door closed; does the pit climb past 120 C on its own?
