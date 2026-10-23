# Charcoal Smoker - Research: What Makes a Smoker Great, Blower Decision, Edge Cases, Cook Algorithm

Created: 2026-09-21
Status: RESEARCH + PROPOSAL. Nothing here is built yet. Items marked PROPOSED need Jason's OK; items marked VERIFIED cite a primary source; items marked NOT VERIFIED were not confirmable at a source and must not be relied on.
Builds on: 01 baseline, 02 architecture, 05 firmware framework.
Units: firmware works in C. F given in parentheses where the source used F.

## 0. Summary of proposals

| # | Proposal | Why | Cost |
|---|---|---|---|
| P1 | Replace the 6 V brushed blower with a 12 V blower fed straight from the input rail. Preferred: FireBoard Drive Blower G3 (20 CFM, 2-wire, turnkey) with its 3/4 in ball valve adapter. Alternative: Same Sky CBM-7530BF (10 CFM, 4-wire PWM + tach) on a DIY pipe stand-off. | Current blower has unknown CFM, no damper, no tach, needs a 6 V buck. Both options delete LM2596 A. | $59 to $99 (FireBoard) or ~$20 plus pipe fittings (Same Sky) |
| P2 | Add a gravity backdraft flap (or ball check) in the intake path regardless of blower choice. | Fan-off natural draft through the blower opening is the number one cause of runaway temps on fan-controlled pits. No blower on the market (except Auber, forum report) seals when off. | under $10 |
| P3 | Cap blower duty for the drum: STARTUP max 50 percent, HOLD max 60 percent, both tunable. Drop IGNITE_DUTY_PCT from 90 to 50. | Drum smokers need "a whisper of air". 20 to 25 CFM at full duty cools the pit, blows ash on the meat, and overshoots. | config change |
| P4 | Rework LID_OPEN to the HeaterMeter LidTrack pattern: rate trigger -0.3 C/s at cook temps, fire-out guard (output EMA under 90 percent), plateau exit, 30 s minimum, RECOVERY state with rate-of-rise limit. Add a drum-specific RISING-lid check. | Our current 1.0 C/s threshold is 3x too insensitive; opening a drum lid can RAISE the pit by 55 C in 3 to 4 min (PBC). | firmware change, unit tests |
| P5 | Add FUEL_LOW warning (output EMA above 90 percent and pit falling for 10 min) and pit deviation alarm (+/- 14 C, armed only after setpoint first reached, muted in LID_OPEN). | Every commercial unit has the alarm; none detects fire-out; the heuristic is documented by HeaterMeter and Flame Boss. | firmware change |
| P6 | Adopt HeaterMeter PID mechanics: derivative on a 60 s EMA, conditional-integration anti-windup, integral clamp equal to the startup cap, halve the integral at first setpoint crossing, long-PWM below min-start duty instead of stopping. | 10+ years of field use on kamados, WSMs and UDS. Source code read and verified. | firmware change |
| P7 | Phase 2 "Cook Manager": meat-probe-driven state machine (RISING, STALL, WRAPPED, FINISHING with BBQ Guru style ramp-down, TENDER_CHECK, HOLD, REST) plus food-safety watchdogs. This is the "keep the meat juicy" algorithm. Spec in section 6. | The controller cannot add moisture. It can stop the four things that dry meat: pit excursions, excess airflow, overshooting the internal target, and serving without a hold. | Phase 2: second MAX31856 or the spare MAX31855, new module, 4 BLE characteristics |
| P8 | Persist "cook active" + setpoint in NVS; on boot resume in RAMP with startup cap, never in HOLD at full authority. | DigiQ, UltraQ, HeaterMeter all auto-resume after power loss. | firmware change |
| P9 | Charcoal fuel management: fuel gauge (integrated airflow -> estimated charcoal consumed), FUEL_LOW lead time sized to the refuel procedure, REFUEL state (door open, add lit coals) that suppresses lid/fuel alarms, and a user-entered basket load at cook start. No auto-reignite exists on charcoal; the pellet-grill patterns (PiFire reignite, Traeger shutdown burn) do not apply. | Charcoal is the product. A pellet grill adds fuel by auger; we cannot. The controller's job is to stretch a basket and warn early enough for a human to add coal. | firmware change (section 1b) |

## 1. What makes a smoker great (applied to our stainless drum)

Sources: amazingribs.com vents and smoke science articles (Meathead, mod. 2026-07), Pit Barrel Cooker setup (amazingribs), Gateway Drum FAQ, UDS build guides (grilling24x7, howtobbqright), FireBoard / BBQ Guru / ThermoWorks / Flame Boss setup pages. URLs in section 9.

1. Intake controls the fire, exhaust makes the draft. On a fan-controlled pit the makers all say: seal every other intake and run the exhaust nearly closed (BBQ Guru "open as little as possible", FireBoard "slightly open", ThermoWorks "1/8 open", Flame Boss "almost closed") so the fan is the only air path. Never fully closed: trapped CO/CO2 smothers the fire and the smoke goes dirty. Meathead's "exhaust wide open" advice is for manual pits, not fan pits. VERIFIED.
2. Clean combustion, not just target temperature. Wood combustion sweet spot is 340 to 400 C (650 to 750 F). Above that the smoke turns acrid; starved of oxygen it smoulders white and bitter. Fan controllers "stoke and choke"; the choke phase is what makes dirty smoke with wood chunks. Design consequence: prefer continuous low airflow to bang-bang bursts, and never leave the fan at 0 for long while wood chunks are in the basket (HeaterMeter long-PWM plus fan minimum keeps a trickle). VERIFIED (amazingribs smoke science; pitmaster forum for the stoke/choke observation).
3. Thermal mass damps swings. Franklin, Mill Scale and Workhorse pits use 1/4 to 3/8 in steel. Our stainless drum is thin and low-mass, so expect faster swings and more wind/rain sensitivity than any published tuning assumes. A water pan on the lowest grate adds mass (water caps at 100 C), humidity and radiant evening of hot spots. Gateway: half-size pan on the lowest grate for low-and-slow, no pan for hot-and-fast drum cooking. VERIFIED. Consequence: PID tuning must be done with and without the pan; two gain sets.
4. Airflow sizing. BBQ Guru: 10 CFM (Pit Viper) for a UDS with a 3/4 in ball valve; 25 CFM Pit Bull is oversized for a 55 gal drum (ash on the meat, pit cooled at full speed). HeaterMeter wiki: a 250 mA blower "can easily support" a 55 gal UDS. UDS owners with 20 CFM fans cap at 35 percent and close the valve 30 percent. VERIFIED. Target for our drum: 5 to 15 CFM effective, with a software cap.
5. Air enters UNDER the basket, not at the side. One UDS owner fixed oscillation by moving the intake below the basket; blower air entering above the grate burned coals unevenly (forum, TVWBB). CORRECTED 2026-09-21: our drum already does this. The blower nozzle inserts into the top of the curved stainless duct at the left post; the duct runs down the outside of the drum and enters the fire chamber at the base under the charcoal basket. The fire door is for charcoal and ash only and stays closed during a cook. The straight centre tube on the lid is the exhaust stack. OPEN: duct inlet ID, nozzle OD, exact termination under the basket, and whether the stack has a damper.
6. Small lit fire, let the controller bring it up. PBC: light 1/4 of the basket. BBQ Guru PartyQ: "light a few coals at the top, do not over-fire". PBC fan users: 12 lit coals, stabilise near 107 C, then enable the fan and cap at 50 percent. "Start low and bring up" beats overshoot-then-choke (a drum is easier to heat than to cool). VERIFIED.
7. Drum-specific behaviours (PBC, amazingribs): temperature can spike to ~200 C (400 F) at startup before settling; opening the lid RAISES the temperature "as much as 100 degrees in 3 to 4 minutes" and stirs an ash vortex; drippings hit the coals. Intake opening varies with altitude (1/4 open at sea level). VERIFIED for PBC; our drum with a fire door is similar in geometry.
8. Probe placement. Pit probe at grate level, 2.5 cm (ThermoWorks) to 8 to 10 cm (BBQ Guru) from the meat, out of the fan jet, never touching metal. Cold meat creates a cool air layer that reads low; a probe outside the pit makes the fan run flat out. One UDS build measured a 28 C (50 F) difference between wall and centre grate. VERIFIED. Our side port beside the dial thermometer is at the wall; PROPOSED: sheath long enough to reach at least 8 cm inboard, at grate height.

## 1b. Charcoal is the product: charcoal-specific rules

The point of this unit is charcoal flavour with pellet-grill convenience. Every pellet controller pattern above must be filtered through one fact: our fuel is a fixed bed of charcoal in a basket, lit once by a human, with forced air delivered UNDER it through the duct. Nothing feeds it, nothing re-lights it.

1. Forced air under a charcoal bed spreads ignition upward through the whole bed (forge effect). Minion and snake methods work by lighting a small region and letting it creep; a blower under the basket defeats that if it runs hard. Evidence: TVWBB minion-with-controller thread (very little starter coal, controller regulates better than a full chimney; air entering above the grate burned coals unevenly); PBC fan users cap at 50 percent. Consequence: the duty caps in P3 are a fuel-life rule as much as an overshoot rule. As more of the bed becomes lit, the fan-off floor temperature creeps up over the cook ("lit-mass creep"); the controller sees it as steadily lower duty at the same setpoint, then inability to hold setpoint with duty at 0. That is the signature to log and alarm on (E25). Inference from the sources, NOT VERIFIED as a measured curve on this drum.
2. Fuel budget and consumption. Sourced points: PBC runs about 8 lb (3.6 kg) for 8 to 12 h at 132 to 154 C (pitbarrelcooker.com); UDS baskets hold 10 to 15 lb and owners report 12 to 15 h smokes, one reports 24 h on lump; a leaky or over-aired drum burned 15 lb of briquettes in 6.5 h and lump in 4 h (smokingmeatforums thread). Lump burns hotter and faster and makes less ash than briquettes; water pans and deflectors cost extra fuel. Planning figure for our drum: 0.3 to 0.6 kg/h at 107 to 121 C, to be measured (weigh the basket before and after two cooks).
3. First-principles airflow check (calculation, not a measurement). Charcoal is roughly 70 to 90 percent fixed carbon. Burning 1 kg of carbon needs 2.67 kg O2, about 11.5 kg of air, about 9.5 m3 (335 ft3). At 0.45 kg/h of charcoal that is about 2.5 CFM of air at stoichiometric, before excess air and leakage. A 10 CFM fan at 25 to 40 percent duty is therefore the right order of magnitude for a drum at cook temperature, and a 20 CFM fan at full duty is 8x stoichiometric, which is why oversized fans cool the pit and blow ash. This supports P3 and the 10 CFM class in section 3 independently of the forum reports.
4. Fuel gauge (P9). Integrate fan duty x fan CFM over the cook to estimate air delivered, multiply by a burn coefficient calibrated from item 2 (kg charcoal per m3 air), subtract from the basket load the user enters at cook start. Report "estimated charcoal remaining" and "hours at current burn rate". Expect 20 to 30 percent error until calibrated; useful for the warning, not for accounting. DESIGN.
5. FUEL_LOW must give a human enough lead time to open the door and add lit coals. Adding UNLIT coal drops the pit and gives dirty smoke until it catches (amazingribs vents article: only add lit coals). Warning threshold: output EMA >= 90 percent and pit falling for 10 min (section 4, E8), or fuel gauge under 20 percent. Alarm lead target: 30 min before the fire cannot hold setpoint.
6. REFUEL state (P9). User taps "refuelling" or the controller sees the fire-door open signature (pit rise with duty at 0, same as the rising-lid check E6). Blower 0 for the door-open period, lid and fuel alarms muted 10 min, then RECOVERY with the startup cap. Log REFUEL with the amount entered.
7. No auto-reignite. PiFire's reignite and Traeger's shutdown burn assume an igniter and an auger. On charcoal, a pit below setpoint - 40 C with duty pegged for 15 min is FIRE_OUT: blower OFF, alarm, done (E8). Running the fan on a dead bed only cools the drum and blows ash.
8. Ash is the charcoal-only failure mode. Briquettes leave more ash than lump; ash under the basket restricts the very air path our duct feeds. UDS practice: basket legs about 3 in (75 mm) of clearance, and clear the tray between cooks. The E9 detector (rising average duty at constant setpoint over hours) is the software side; it cannot distinguish ash choke from lit-mass creep by itself, so the alarm text says "check fuel and ash".
9. Clean smoke on charcoal. Charcoal itself burns nearly smokeless once ashed over; flavour smoke comes from wood chunks on the coals. The stoke-and-choke warning in item 2 of section 1 is about those chunks: at duty 0 they smoulder white. Options: a fan floor of 5 to 10 percent with the damper trimmed, or place chunks where the air path keeps them in flame. Both are user procedure plus the BLOWER_FLOOR_DUTY_PCT setting.
10. Carbon monoxide. CPSC: "Burning charcoal inside can kill you. It gives off carbon monoxide, which has no odor." "Do not barbecue in the garage." A forced-draft charcoal fire makes more CO when choked (fan at 0, exhaust nearly closed). Outdoor use only; the garage photo is for the bench phase with no fire. Put the CPSC wording in the app's first-run screen. VERIFIED (cpsc.gov charcoal grill safety tips PDF).
11. Startup on charcoal. There is no ignition to command. IGNITE in our state machine means "user has lit a small fire, bring it to setpoint - 17 C at capped duty". Procedure: light 1/4 of the basket or about 12 briquettes on top, insert the basket, close the door, start the controller. Chimney-lit full baskets are the main cause of E1 overshoot and of short fuel life.

## 2. How the established controllers do it, and what we borrow

| Feature | BBQ Guru (DX3 / UltraQ / CyberQ) | FireBoard Drive | Flame Boss 400/500 | ThermoWorks Billows + Signals | HeaterMeter (open source) | PiFire (open source) | We borrow |
|---|---|---|---|---|---|---|---|
| Control | Proprietary "Smart Cook": 100 percent fan until 20/30/40 F below setpoint, then pulsed in a proportional band (cycle 5 to 12 s, band 10 to 50 F) | PID, versions v0.5 to v3.0, gains unpublished | Variable-speed PID, learns for ~10 min, resets at power-off | ON/OFF pulsing only, no variable speed | PID 1 Hz, P 4 / I 0.02 / D 5 (per F), D on 60 s EMA, conditional anti-windup | PID, PB 60 F, Ti 180 s, Td 45 s, output centre 0.5; stepped DC fan profile 20/35/50/75/100 percent at 3/7/10/15 F error | HeaterMeter mechanics (P6). BBQ Guru "full speed until X below setpoint" as the IGNITE exit rule. |
| Startup overshoot | Full speed only to 20 to 40 F below setpoint | Preheat to 200 F, block 50 percent of airflow, kamado cap 30 percent | Insert blower once coals ash over | Pull probe out if flames overshoot it | Startup fan cap (users: 50 to 60 percent); integral clamp = cap; halve I at first crossing | Startup duty 100 percent for 240 s | Startup cap 50 percent, I clamp, halve-I (P3, P6) |
| Fan damper | Manual slide/rotary on Pit Viper | Manual slide on Drive Blower | None ("variable speed makes it unnecessary") | Manual 16-stop damper | Servo damper, "0 percent output = fully closed" | n/a (pellet) | Gravity flap now (P2), servo damper Phase 3 |
| Lid open | "Detects sudden drop", minimises fan, limits rate of rise after close, mutes alarm; disable in wind | "Rapid drop" -> fan stops for adjustable pause; can disable | Pit shows OPEN, fan paused 2 min default (adjustable on FB100 to 300, fixed on 400) | Added by firmware update, fan stops | Classic: 6 percent below SP after SP reached, output EMA under 90 percent, 240 s; LidTrack: -0.5 F/s at 225 F, exit at plateau | 15 percent below hold temp after target reached; fan+auger off 60 s | LidTrack rate trigger + classic backstop + fire-out guard + RECOVERY (P4) |
| Fire out / fuel low | None. Manual shutdown only | NOT VERIFIED | None; heuristic "falling temp with fan pegged" | None found | Guard only: output EMA >= 90 percent blocks lid mode ("probable out of fuel") | Reignite once if pit falls below 0.9 x startup temp, then Error | FUEL_LOW warning (P5) |
| Pit alarm | Deviation 50 F default (DX3, CyberQ), 25 F (UltraQ); not at cold power-up | App alarms | +/- band, example 20 F | +/- 25 F fixed in fan mode | Per-probe high/low with 1 degree hysteresis | maxtemp 550 F -> Error | +/- 14 C default, armed after first setpoint reach, muted in LID_OPEN, 1 C hysteresis (P5) |
| Probe fault | "---" / OPEN | Fan 0 percent with no control probe | Blower off with no pit reading | Will not run without probe on ch 4 | Reject rail values and < -100 C / > 500 C; output 0 | n/a | Already: SENSOR_* -> FAULT -> blower OFF. Keep. |
| Meat-driven modes | Ramp mode: within 30 F of done, pit setpoint ramps down to ~30 F above food target; Cook & Hold 200 F; timer -> HOLD/ALARM/SHDN | Drive Programs: steps end on food temp, time, or first of both; default final Hold step | "On + Keep Warm": pit drops to user keep-warm temp when meat done | High/low alarms per channel | Linear ETA fit over last hour, author calls it "sort of worthless" in the stall | Keep Warm -> Hold 165 F; recipe steps on probe/timer | Section 6 Cook Manager (P7) |
| Power loss | DX3/UltraQ auto-restart at same settings | Battery: fan may not reach full speed | Learning resets | n/a | Reboots into STARTUP with stored setpoint | n/a | P8 |
| Stability claim | +/- 2 F probe accuracy (not stability) | "within a couple of degrees" over 12 to 16 h | NOT VERIFIED | +/- 10 F typical | n/a | n/a | Target: +/- 5 C in HOLD, no water pan; measure |

Fan minimum handling worth copying exactly (VERIFIED in grillpid.cpp and grillpid_conf.h):
- Loop period 1000 ms. Probe EMA alpha 2/61. Output EMA alpha 2/241.
- Long PWM: below fan minimum speed, use a 10 s period; run at minimum speed for (requested / minimum) of the period, off for the rest.
- Boost: any 0 -> nonzero transition drives 100 percent for one 1/3 s sub-period. (Ours is 300 ms. Same idea.)
- Lid mode minimum 30 s before auto-resume; duration can never be set below 30 s.
- PiFire: min duty 20 percent "because some fans stall below a certain speed". FireBoard "Large" fan mode starts at 30 percent.

## 3. Blower decision

### 3.1 Why replace
The existing 6 V brushed blower: CFM unknown, no damper, no tach (no stall detection), needs the 6 V buck (LM2596 A), and its heat rating is unknown. It is adequate for bench steps 1 to 5 (it is on hand and the MOSFET stage is built for it). It is not the right long-term part.

### 3.1a Replacement blower requirements (added 2026-09-21)

MUST = build fails without it. SHOULD = strong preference. Rationale cites sources already in section 9.

Electrical
- R1 MUST run from the 12 V input rail: nominal 12 V, operate 11.0 to 13.0 V (adapter minus fuse and Schottky drop). No buck for the blower.
- R2 MUST draw <= 0.8 A continuous and <= 1.5 A inrush so blower plus 0.5 A logic stays inside the 12 V 2 A adapter with margin.
- R3 MUST be speed-controllable from the ESP32-S3 LEDC output by one of exactly two methods: (a) 2-wire brushed motor, supply chopped by the existing low-side IRLZ44N with flyback diode, 200 Hz to 25 kHz; or (b) 4-wire brushless with a logic PWM input at 21 to 28 kHz (Intel 4-wire spec) that accepts a 3.3 V high or an open-drain drive, and a steady 12 V supply. A brushless fan MUST NOT be supply-chopped.
- R4 MUST reach zero airflow when commanded off: rotor stops at 0 percent duty (Delta: stated; Same Sky: not stated) or the supply is cut by the MOSFET.
- R5 MUST fail safe: a broken control line must either stop the fan (2-wire) or be detectable (4-wire: a fan with the PWM lead open runs at full speed per the Delta sheet, so a tach output is REQUIRED on any 4-wire choice, with zero pulses at duty > 0 raising BLOWER_STALL and the thermostat as the hardware backstop).
- R6 SHOULD provide a tach (open collector, pulled up to 3.3 V) for stall and disconnect detection and as the airflow proxy for the fuel gauge (P9).

Airflow
- R7 MUST deliver 5 CFM at the duct's back-pressure and SHOULD have a free-air rating of 8 to 15 CFM. Basis: stoichiometric air for 0.45 kg/h charcoal is about 2.5 CFM (section 1b item 3); BBQ Guru specifies 10 CFM for a UDS; HeaterMeter states a 250 mA blower supports a 55 gal UDS; 20 to 25 CFM fans on drums cool the pit and blow ash. Anything above 15 CFM MUST be capped in software (BLOWER_MAX_DUTY) and by the damper.
- R8 MUST be a centrifugal blower chosen on STATIC PRESSURE, not CFM. UPDATED 2026-09-24 with measured duct geometry: OD 15/16 in (23.81 mm), wall 0.8 to 0.9 mm, so bore 22.1 mm; route 12 in down the outside of the drum, 90 degree bend, 6 in under the barrel, 90 degree bend, 2 in up into the basket. Total run 20 in (508 mm), two bends.
  Duct resistance (Darcy-Weisbach, Blasius friction, entry 0.5, two bends K 0.9, exit 1.0, plus K 2.0 for a backdraft flap, air at 1.2 kg/m3, before the charcoal bed):

| Flow | pressure needed |
|---|---|
| 2 CFM | 0.09 inH2O |
| 3 CFM | 0.20 inH2O |
| 4 CFM | 0.35 inH2O |
| 5 CFM | 0.55 inH2O |
| 6 CFM | 0.79 inH2O |
| 8 CFM | 1.39 inH2O |

  Intersecting that system curve with each candidate's fan curve (linear from free delivery to max static, fan laws for reduced duty) gives what each blower can actually put into this drum:

| Fan | Free air | Max static | Delivered at 100 percent | Duty needed for 2.8 CFM | Delivered at 50 percent |
|---|---|---|---|---|---|
| Same Sky CBM-7530BF, 75 mm | 10.1 CFM | 0.33 inH2O | 3.2 CFM | 88 percent | 1.6 CFM |
| Same Sky CBM-5015BF, 50 mm | 4.8 CFM | 1.24 inH2O | 3.6 CFM | 78 percent | 1.8 CFM |
| Delta BFB0712HH, 75 mm | 15.3 CFM | 0.98 inH2O | 5.4 CFM | 53 percent | 2.7 CFM |
| San Ace 9BMB12P2F01, 97 mm | 36.7 CFM | 1.64 inH2O | 7.7 CFM | 37 percent | 3.8 CFM |

  Reference demand: about 2.8 CFM holds 110 C in the sim plant (doc 08), which matches the stoichiometric estimate for 0.35 kg/h of charcoal with excess air.
  Consequences:
  - Free-air CFM ratings are nearly meaningless here. A 10 CFM fan delivers 3.2 CFM through this duct; a 20 CFM fan would not deliver much more, because the pipe, not the fan, sets the ceiling.
  - Requirement, revised: deliver at least 2.8 CFM at no more than 60 percent duty through a system curve of 0.20 inH2O at 3 CFM. In practice that means a maximum static pressure of at least 0.9 inH2O.
  - The 75 mm Same Sky fails this: it would run at 88 percent duty just to hold temperature, leaving no headroom for RAMP, a partly ashed intake, or wind.
  - The calculation assumes a smooth bore and no coal-bed resistance. The real curve is worse, which widens the gap rather than closing it.
- R9 SHOULD run stably down to 20 to 30 percent of rated speed; below that the firmware long-PWM (P6) takes over. FireBoard "Large" mode starts fans at 30 percent; PiFire floor is 20 percent.

Mechanical, thermal, environmental
- R10 MUST couple to the existing duct inlet at the top of the left post with an airtight joint (measure duct ID and socket depth first). Round outlet SHOULD match the duct; a rectangular outlet (FireBoard 30 x 40 mm) needs a transition piece.
- R11 MUST include, in the fan or in the coupling, a backdraft flap that closes at zero flow (E2). No verified commercial blower has one; an inline flap or ball check is part of the blower assembly by definition.
- R12 MUST tolerate the duct inlet temperature during a cook. The fan body sits in ambient air beside the thermostat box, but the duct conducts heat up from the fire chamber and hot gas can back-flow when the fan is off. Measure duct inlet temperature on the first cook; if it exceeds the fan rating (generic fans: 70 C max; FireBoard nozzle adapter 260 C, fan body rating not published) add a stand-off or the flap solves it.
- R13 MUST be outdoor-tolerant: intake facing down or shielded from rain, and wind-shielded (an unshielded blower lost control above 150 C in 8.5 mph wind in the Flame Boss test). Grease and ash are not a concern at the duct inlet, which is on the cold side.
- R14 SHOULD be <= 50 dBA at 1 m (Same Sky 40 dBA, Delta 51 dBA, current blower unknown).
- R15 MUST be removable without tools for cleaning and for swapping back to the original blower (connector or barrel jack, clamp or bayonet at the duct).
- R16 Cable >= 1 m to the controller enclosure on the same post; for a 4-wire fan the PWM and tach lines are logic level and SHOULD be routed away from the 12 V switched lead.

Compliance of the candidates (VERIFIED specs only; blanks are NOT VERIFIED)

| Req | FireBoard Drive G3 (2-wire) | Same Sky CBM-7530BF-130-404-22 | Delta BFB0712HH-AWPH | Current 6 V blower |
|---|---|---|---|---|
| R1 12 V | yes, 12 VDC | yes | yes, 7.0 to 13.8 V | no, 6 V (needs buck) |
| R2 current | 0.68 A | 0.30 A max | 0.42 A max | 0.8 A max |
| R3 control | (a) supply chop | (b) PWM 10 to 60 kHz, VIH 2.5 to 6 V | (b) 25 kHz, 0 to 5.5 V | (a) |
| R4 stops at 0 | via MOSFET | not stated | rotor stops | via MOSFET |
| R5/R6 tach | none, fails to OFF | yes, open collector | yes, open collector 5 mA | none, fails to OFF |
| R7 flow | 20 CFM, must cap | 10.1 CFM | 15.3 CFM | unknown |
| R8 pressure | not published | 0.33 inH2O max (marginal on a 25 mm duct at full flow, fine at 5 CFM) | 0.98 inH2O max (best) | unknown |
| R10 outlet | 30 x 40 mm rectangle | 75 mm blower housing, round-ish outlet | 75.7 mm housing | 25 mm class nozzle (fits today) |
| R11 flap | none (slide damper only) | none | none | none |
| R12 temp | fan body not published | 70 C | 70 C | unknown |
| R14 noise | not published | 40.4 dBA | 51 dBA | unknown |
| Price | $69 + $30 adapter | NV | about EUR 20 | owned |

Reading: Delta wins on pressure and fail-safe detection but is loudest; Same Sky is the quietest and right-sized but pressure-limited if the duct is narrow; FireBoard is turnkey and heat-rated at the nozzle but oversized, undetectable on failure, and needs a rectangle-to-round transition. The duct ID measurement decides between Same Sky and Delta.

### 3.2 Options (all VERIFIED at the URL given unless marked)

| Option | Type | V / A | CFM | Static P | Drive | Tach | Damper | Temp rating | Price | Source |
|---|---|---|---|---|---|---|---|---|---|---|
| A. FireBoard Drive Blower G3 | 2-wire (motor type NOT VERIFIED) | 12 VDC / 0.68 A, 8 W | 20 | NOT VERIFIED | Supply chop (our IRLZ44N) | No | Manual slide | Nozzle adapter -73 to 260 C (-100 to 500 F); fan body NOT VERIFIED | $59 to $69 + $30 ball valve adapter | atbbq.com, docs.fireboard.io/drive/blower.html |
| B. Same Sky CBM-7530BF-130-404-22 | Brushless 4-wire | 12 V / 0.20 A typ, 0.30 A max | 10.14 | 0.33 inH2O | Logic PWM 10 to 60 kHz, VIH 2.5 to 6 V, VIL 0 to 0.4 V | Open collector, N = FG x 30 | None | -10 to 70 C | ~$20 class, NOT VERIFIED | sameskydevices.com cbm-75bf.pdf |
| C. Delta BFB0712HH-AWPH | Brushless 4-wire | 12 V (7.0 to 13.8) / 0.34 A, 0.42 A max | 15.3 | 0.977 inH2O | PWM 25 kHz preferred, 0 to 5.5 V; rotor stops at 0 percent; runs FULL if PWM lead open | Open collector, 5 mA max | None | -10 to 70 C | ~EUR 20 (Mouser, OEM listing) | delta-fan.com BFB0712HH-AWPH.pdf |
| D. Flame Boss Universal blower | 2-wire | 12 V, plug 5.5 x 2.5 mm centre + | 21 | NOT VERIFIED | Supply chop | No | None | NOT VERIFIED | $52.99, $63.99 with 3/4 in NPT manifold | flameboss.com, support.flameboss.com adapter/specs article |
| E. BBQ Guru Pit Viper | NOT VERIFIED | 12 V (forum only) | 10 | NOT VERIFIED | Supply / analog | No | Manual slide | "high temperature and water resistant" | $59.50 + 3/4 in adapter #19386 | bbqguru.com |
| F. ThermoWorks Billows | Proprietary USB-C control, on/off only | 12 V | 10 (ThermoWorks support) vs 46 (reviews): CONFLICT | n/a | Not drivable by our board | n/a | Manual 16-stop | 260 C housing | $89 list | thermoworks.com |
| G. Keep current blower | Brushed 2-wire | 6 V / 0.8 A max | NOT VERIFIED | NOT VERIFIED | Supply chop via LM2596 6 V + IRLZ44N | No | None | NOT VERIFIED | $0 | baseline doc |

Rejected: Billows (proprietary), Inkbird / ChefsTemp Breezo (5 V USB-C), LavaLock (110 V), San Ace 97 mm (36.7 CFM, 0.9 A, oversized), Delta BFB0512VHD (3-wire, no PWM pin; the RotoDamper part), Auber BL series (pages blocked; voltage/connector NOT VERIFIED; a forum report says the BL-6.5 has an internal gravity flap, which is the only blower found with one).

### 3.3 Recommendation (revised 2026-09-24 after the duct measurements)
1. Bench bring-up (steps 1 to 5 in doc 02): keep the current blower. Unchanged.
2. PREFERRED: Delta BFB0712HH-AWPH. 12 V, 0.42 A max, 0.98 inH2O max static, 4-wire logic PWM at 25 kHz with a tach. Through the measured duct it delivers about 5.4 CFM at full duty and holds the 110 C demand at roughly 53 percent, which leaves real headroom for RAMP and for a fouled intake. The tach satisfies R5 and R6. Costs: 51 dBA and an open-drain PWM driver plus a tach input.
3. Second choice: Same Sky CBM-5015BF-175-459-22 (50 mm, 1.24 inH2O, 0.33 A, 45.9 dBA). Higher pressure than its 75 mm sibling despite a much lower CFM rating, which is exactly the point. It holds the demand at about 78 percent duty. Thin but workable, and physically small.
4. NOT recommended any more: Same Sky CBM-7530BF (0.33 inH2O, 88 percent duty to hold temperature) and FireBoard Drive G3 (20 CFM unusable through a 22 mm bore, static pressure unpublished, rectangular outlet, no tach).
5. Either blower still needs the backdraft flap. Flap sizing, from the same numbers: at 2 to 3 CFM the duct sits at 20 to 50 Pa, which on a 25 mm disc is 1 to 2 gram-force. Spring and swing check valves crack at tens of kPa and will never open. Use a near-zero-cracking gravity flap: a thin hinged flapper of about a gram, or the salt-weighted ping-pong ball check, tuned until it opens with the blower on and seals with it off.
6. If the delivered flow proves marginal on the first cook, the cheapest fix is not a bigger fan: shorten or straighten the duct, or increase the bore. Flow through a fixed pressure scales roughly with bore to the 2.5 power, so going from 22 mm to 28 mm would roughly double the delivered air.

### 3.4 Wiring and mating impact per option
Duct mating (all options): the duct inlet is 15/16 in (23.8 mm) OD, 22.1 mm bore, 0.8 to 0.9 mm wall (too thin to thread). A 1 in ID silicone or EPDM hose with two worm clamps is the simplest airtight coupling; if the pipe proves to be 3/4 in EMT, EMT set-screw couplings also fit. Confirm the bore and the current nozzle OD before buying either.

- A, D, E (2-wire 12 V): delete LM2596 A. +12 V node -> thermostat closed pair -> blower +; blower - -> IRLZ44N drain; 1N5819 flyback stays (40 V rating fine); source -> GND. PWM frequency: bench test 25 kHz vs 200 Hz as planned. Add a 12 V-rated inline fuse of 1 A on the blower rail. Power budget improves: 0.68 A at 12 V = 8.2 W vs 4.8 W plus buck loss today; the 12 V 2 A adapter still has 1.3 A headroom.
- B, C (4-wire brushless): delete LM2596 A. +12 V (through the thermostat contact, which now acts as a supply cut) -> fan red; black -> GND. NEVER chop the supply of a brushless fan for speed control. PWM: ESP32 GPIO4 -> gate of a small N-MOSFET (2N7002 / BSS138) as open-drain onto the fan's blue/PWM wire; the fan pulls the line up internally (Intel spec allows up to 5.25 V, Noctua white paper: 12 V on the PWM pin destroys the fan). Duty is INVERTED in firmware (drain low = 0 percent). Tach: yellow -> 10 k pull-up to 3.3 V -> GPIO with pulse counter; zero pulses while duty > 0 for 3 s = BLOWER_STALL fault. 25 kHz LEDC as already configured. Keep the IRLZ44N only as a hard power cut on FAULT (optional). Safety note from the Delta sheet: with the PWM lead disconnected the fan runs at FULL speed, so a broken PWM wire is a runaway; the tach plus pit rate check catches it, and the thermostat is the backstop. This is the one argument for Option A over B/C on a safety basis.
- G: no change.

## 4. Edge case catalog (controller side)

Each row: what happens, evidence, what we do. FW = firmware change; HW = hardware; PROC = procedure/documentation for the user.

| # | Edge case | Evidence | Mitigation | Type |
|---|---|---|---|---|
| E1 | Startup overshoot from too much lit fuel or aggressive fan | BGE: easier to heat than cool; PBC 400 F spikes; FireBoard FAQ; BBQ Guru manuals | IGNITE duty 50 percent (was 90); STARTUP_MAX_DUTY 50 percent in RAMP; integral clamp = cap; halve I at first crossing; exit IGNITE at setpoint - 17 C (30 F, BBQ Guru setting 1) | FW |
| E2 | Natural draft through the blower and the open duct inlet when the fan is off -> temperature creep, cannot run below ~135 C | TVWBB threads (runaway temps, kamado damper, servo damper) | Gravity flap / ball check (P2) at the duct inlet; fire door closed and sealed; exhaust stack damper nearly closed; long term servo damper closed at 0 percent | HW + PROC |
| E3 | Fan too large: pit cools at full speed, ash on food | BBQ Brethren Pit Bull on UDS thread | BLOWER_MAX_DUTY 60 percent default; damper half; 10 CFM class fan | FW + HW |
| E4 | Lid open misread as fuel out, and the reverse | HeaterMeter 90 percent output EMA guard | Lid mode only if output EMA < 90 percent; FUEL_LOW only if output EMA >= 90 percent and pit falling | FW |
| E5 | Lid false positive from wind gust or slow cooling | CyberQ "disable in excess air currents"; LidTrack thread | Rate trigger scaled to setpoint, plus require T < SP and setpoint previously reached; BLE flag to disable lid detect | FW |
| E6 | Drum lid open RAISES the pit temperature | PBC +100 F in 3 to 4 min (amazingribs) | New check: pit rising >= 0.3 C/s for 20 s while duty <= 10 percent and no setpoint change -> LID_OPEN (rising variant): duty 0, mute high alarm 5 min. No commercial product handles this; PROPOSED, not sourced. | FW |
| E7 | Recovery overshoot after lid closes | LidTrack author: cancelling early "goes 100 percent full speed ahead"; Flame Boss: coals ignite during pause | RECOVERY state: startup cap applies, I frozen, exit to HOLD when within band; 30 s minimum in LID_OPEN; plateau exit | FW |
| E8 | Fuel running out / fire dying | Flame Boss review heuristic; Traeger below 120 F 10 min; PiFire 0.9 x startup temp | FUEL_LOW warning: output EMA >= 90 percent and pit falling >= 5 C over 10 min. FIRE_OUT fault: pit < setpoint - 40 C for 15 min with duty pegged, blower OFF (no point feeding air to a dead fire and a live thermocouple in a cold drum would otherwise run the fan forever) | FW |
| E9 | Ash choke / basket bridging | smokingmeatforums ash choke thread | Detect: hour-over-hour rise in average duty at constant setpoint -> "airflow restricted, check ash" notice. PROPOSED, not sourced. Basket legs 3 in (UDS builds). | FW + PROC |
| E10 | Wind into the intake spikes the pit | Meadow Creek cold/wind tips; Flame Boss wind shield review (undamped blower lost control above 300 F in 8.5 mph wind) | Flap (P2), windbreak, fan intake facing away from wind | HW + PROC |
| E11 | Cold weather, rain, condensation | amazingribs cold weather | Expect higher steady duty and longer IGNITE; IGNITE_TIMEOUT stays 40 min but IGNITE_MIN_RISE check uses baseline captured at IGNITE entry (already fixed); allow a "cold weather" gain set | FW + PROC |
| E12 | Probe placement wrong (touching meat, in fan jet, at wall, outside pit) | BBQ Guru DynaQ FAQ, FireBoard usage, PartyQ manual | PROC: grate level, 2.5 to 10 cm from meat, out of the jet. FW: if duty pegged 100 percent for 20 min and pit never rises, FAULT NO_RISE (exists as IGNITE timeout; extend to HOLD as FUEL_LOW/FIRE_OUT) | PROC + FW |
| E13 | Probe damage: melted lead, water in cable, intermittent | ThermoWorks troubleshooting; TVWBB probe threads | MAX31856 fault bits (exists); add plausibility: reject step > 30 C in 1 s as a glitch (median-of-5 partly covers); rate-of-change sanity before LID logic | FW |
| E14 | Thermocouple drift / offset | BBQ Guru FAQ, Traeger offset -15 to +15 F | Per-probe offset in NVS; boiling/ice check procedure (bring-up step 3) | FW + PROC |
| E15 | Blower grease clog / heat damage | no primary source, NOT VERIFIED | Mount fan in cool air on a stand-off; generic fans are rated 70 C max; FireBoard nozzle adapter is the heat-rated part | HW |
| E16 | Power loss mid-cook | DX3, UltraQ, HeaterMeter | P8: NVS "cook active"; resume in RAMP with startup cap; log POWER_RESUME event | FW |
| E17 | Blower stall / disconnected / PWM wire broken | PiFire min duty; Delta sheet: open PWM lead = full speed | 4-wire: tach; 2-wire: optional low-side shunt (deferred in BOM). Pit rate check: duty > 0 but pit falling for 10 min -> FUEL_LOW covers the "fan not actually moving air" case | HW + FW |
| E18 | Over-oxygenated or choked fire -> acrid or white smoke | amazingribs smoke science; pitmaster forum stoke/choke | Continuous low duty via long-PWM below min-start; BLOWER_MAX_DUTY cap; avoid 0 percent for more than a few minutes in HOLD (floor duty option, default 0, user can set 5 to 10 percent with damper) | FW |
| E19 | Lump vs briquette; minion vs snake with a fan | TVWBB minion thread | PROC: minion with very little starter coal; aim air under basket centre. Lump: less ash, sparks (NOT VERIFIED). | PROC |
| E20 | Water pan present vs absent changes plant dynamics | amazingribs water pan; Gateway FAQ | Two PID gain sets in NVS (pan / no pan), selectable over BLE | FW |
| E21 | Setpoint changed mid-cook | HeaterMeter: setpoint change re-enters STARTUP | Setpoint change -> RAMP with startup cap (down-steps: duty 0 until within band, then PID) | FW |
| E22 | Software over-temp vs thermostat dial ordering | Doc 02 section 3 | PIT_SOFT_LIMIT_C 200 must be below the dial; SETPOINT_MAX 180 C. Verify dial reads >= 220 C at bring-up step 4. 2026-10-22: being replaced by a dynamic limit capped at 240 with the dial at 270, section 6a | PROC |
| E23 | BLE client disconnects / phone dies | n/a | Controller is autonomous; alarms buzz locally (Phase 2 buzzer, deferred); event log survives in RAM and later flash | FW (Phase 2) |
| E24 | Thin drum wall: fast swings, weather sensitivity | Inference from steel-thickness comparison; not sourced | Lower P, higher D relative to HeaterMeter defaults; log and tune; water pan | FW + PROC |
| E25 | Lit-mass creep: forced air under the basket lights the whole bed, fan-off floor temperature rises over the cook until setpoint cannot be held with duty 0 | Section 1b item 1 (inference from TVWBB minion thread and PBC fan practice) | Duty caps (P3), flap (P2), log duty trend; alarm "pit above setpoint with fan off, close exhaust or remove coal" if pit > setpoint + 14 C for 15 min at duty 0 | FW + PROC |
| E26 | Fuel budget exceeded: basket empties before the meat is done (a 14 h brisket on an 8 h basket) | PBC 8 lb for 8 to 12 h; UDS 10 to 15 lb baskets; leaky drum burned 15 lb in 6.5 h | Fuel gauge + basket load entry at cook start; FUEL_LOW 30 min lead; REFUEL state | FW |
| E27 | Unlit coal added mid-cook: pit dips, white smoke, then surge as it catches | amazingribs vents: add only lit coals | REFUEL state mutes alarms 10 min, then RECOVERY with startup cap; app text says add lit coals | FW + PROC |
| E28 | Lump vs briquette burn rate and ash differ by 30 to 50 percent | smokingmeatforums UDS thread (lump 4 h vs briquettes 6.5 h in the same drum) | Fuel type selector feeds the burn coefficient; two calibration constants | FW |
| E29 | CO from a choked charcoal fire | CPSC charcoal safety tips | Outdoor only; first-run warning; never fully close the exhaust (section 1 item 1) | PROC |

## 5. Meat moisture science (numbers the algorithm relies on)

All VERIFIED at the cited source unless marked.

Where the water goes:
- Muscle fibres shrink across their width at 50 to 65 C (myosin) and along their length at 70 to 75 C (actin). Cooking loss is driven by fibre shrinkage, not connective tissue (Purslow et al., Food Research International 2016, PMID 28460973).
- Beef held 30 min: 7 to 11 percent loss at 50 C; 17 to 21 percent at 60 C; 29 to 32 percent at 70 C; 34 to 40 percent at 80 C (Vaskoska et al., Foods 2020, PMID 32937816). This is the loss curve. It is steep between 60 and 70 C.
- Collagen denatures at 53 to 63 C, then dissolves to gelatin with further heat and time (Tornberg, Meat Science 2005, PMID 22063748). Gelatin binds up to ten times its weight in water (ThermoWorks). At 80 C full conversion takes 12 to 24 h (Baldwin); hotter is faster; the 82 to 96 C (180 to 205 F) range is where practitioners finish brisket and butt.
- Consequence: two different juiciness problems. Lean cuts (loin, steak, chicken breast): juiciness is set only by peak internal temperature; stop at 57 to 63 C (135 to 145 F) and never overshoot. Collagen cuts (brisket, butt, ribs): fibres are already squeezed dry by ~77 C; perceived juiciness comes from gelatin and rendered fat, which need TIME in the 71 to 96 C window and a long hold afterwards.

The stall:
- Mechanism is evaporative cooling (Blonder experiments, amazingribs). Starts at 65 to 77 C (150 to 170 F) internal; plateau 65 to 75 C (Baldwin). Can last 6 h at 107 C pit. At ~150 C (300 F) pit it may vanish.
- Airflow and humidity: more ventilation and lower humidity mean more evaporation from the surface (Modernist Cuisine measured chamber RH falling from 72 to under 50 percent as venting outpaced evaporation). Whether more airflow shortens or lengthens the stall is disputed between sources; that it dries the surface is not. Controller consequence: steady low fan duty, never bursts; log average duty during the stall as a drying indicator (DESIGN).
- Wrap (Texas crutch): brisket at 66 to 71 C (150 to 160 F), keep wrapped to 93 to 96 C; ribs at most 1 h. Foil is faster and seals; butcher paper is slower and keeps more bark. Unwrapping drops the meat temperature immediately (Blonder saw a fall to ~79 C) because evaporation resumes. That drop is NOT a fault.

Pit temperature:
- USDA (2011 smoking guidance): keep the smoker at 107 to 149 C (225 to 300 F) throughout.
- Practice: Meathead 107 C; ThermoWorks 121 C then 135 C after wrap; Franklin 124 C rising to 138 to 141 C before and after the wrap; hot-and-fast 177 C works with a foil wrap at 77 C. No controlled study links pit temperature to juiciness; NOT VERIFIED beyond practitioner consensus.

Rest and hold:
- Carryover 3 to 6 C (5 to 10 F) after removal. Resting lean cuts gives little (Blonder, Kenji). Holding brisket/butt for hours is different: moisture redistributes and the exterior softens (Meathead, Savell via NPR).
- Franklin: 2 to 3 h at 60 C (140 F); Mueller 2 to 4 h in a Cambro; Caldwell County 77 C (170 F) up to 17 h; commercial CVap/Alto-Shaam about 74 C (165 F); overnight home holds 79 C (175 F) (BarbecueFAQ, 2026-09-04); home oven test 62 to 64 C for 4 h stayed juicy (VWB). Serve at 66 to 71 C (150 to 160 F).
- Traeger Keep Warm is fixed at 74 C (165 F) and warns it will overcook a 54 C prime rib.
- Food safety: hot hold at or above 60 C (140 F) (USDA consumer); FDA Food Code commercial minimum 57 C (135 F). Danger zone 4 to 60 C; "4 hours" is industry rule (ServSafe), not USDA verbatim.

Doneness beyond temperature:
- Brisket/butt: 91 to 96 C (195 to 205 F) is a proxy; the real test is probe tenderness ("slides in with little resistance") and jiggle. "Reads 203 but not tender" means keep cooking and re-check; no authoritative interval (NOT VERIFIED).

Safety minimums (USDA / FoodSafety.gov, 2024-11-21): whole beef/pork/lamb 63 C (145 F) + 3 min rest; ground 71 C (160 F); poultry 74 C (165 F) instant, or time-at-temperature per FSIS 7-log table (e.g. 63 C for 11.3 min, 66 C for 3.5 min, 68 C for 52 s, 71 C for 16 s at the fattest column).

## 6. Cook Manager: the "never dry" algorithm (PROPOSED, Phase 2)

Honest framing: a controller cannot add moisture. It can remove the four causes of dry barbecue that are under its control: (1) pit excursions and bursts of airflow that dry the surface, (2) overshooting the internal target on lean cuts, (3) finishing collagen cuts on a thermometer number instead of tenderness plus time, (4) serving without a hold. Everything below targets one of those four.

### 6.1 Inputs
- pit_c (control probe, 1 Hz, existing).
- meat_c[0..1] (Phase 2 probes; NOT in the pit control loop).
- profile: BRISKET, PORK_BUTT, RIBS, POULTRY, LEAN_BEEF, PORK_LOIN, CUSTOM.
- user events over BLE: WRAPPED, UNWRAPPED, TENDER_YES, TENDER_NO, REMOVED, SERVE.
- wall clock (millis is enough; absolute time from phone at connect).

### 6.2 Profile table (defaults, all user-editable, stored in NVS)

| Profile | Pit setpoint C (F) | Wrap prompt at meat C (F) | Post-wrap pit C (F) | Target meat C (F) | Tender check required | Pull offset for carryover C | Hold pit C (F) | Safety min C (F) |
|---|---|---|---|---|---|---|---|---|
| BRISKET | 121 (250) | 66 to 71 (150 to 160) or STALL detected | 135 (275) | 96 (205) | yes, from 91 (195) | 0 (finish by feel) | 74 (165), range 66 to 79 | 63 (145) |
| PORK_BUTT | 121 (250) | 71 (160) or STALL | 135 (275) | 96 (205) | yes, from 93 (200) | 0 | 74 (165) | 63 (145) |
| RIBS | 121 (250) | 71 (160), max 1 h wrapped | 121 (250) | 93 (200) proxy; bend test | yes (bend) | 0 | 66 (150) | 63 (145) |
| POULTRY | 149 (300) | none | n/a | 74 (165) breast; 79 (175) thigh | no | 3 (5 F) | 63 (145) with time-at-temp | 74 (165) instant or FSIS table |
| LEAN_BEEF | 107 (225) | none | n/a | 54 (130) rare to 60 (140) | no | 5 (10 F) | 57 (135) | 63 (145) whole-muscle rule; user override logged |
| PORK_LOIN | 121 (250) | none | n/a | 63 (145) | no | 3 (5 F) | 60 (140) | 63 (145) + 3 min |

Sources for numbers: ThermoWorks brisket and hot-and-fast articles, amazingribs crutch and meat science, Franklin (MasterClass summary), USDA/FoodSafety.gov, Traeger keep warm, BarbecueFAQ holds. Ranges chosen inside sourced values; specific defaults are DESIGN choices.

### 6.3 Meat state machine (per probe)

```
NO_PROBE -> INSERTED (meat_c valid and < 40 C; if < 0 C: refuse, "do not thaw in the smoker", USDA)
INSERTED -> RISING       when meat_c >= start + 9 C (16 F, MEATER rule for ETA validity)
RISING   -> STALL        when profile is a collagen cut, meat_c in 63..79 C (145..175 F), slope_45min <= 1.7 C/h (3 F/h),
                          and pit within +/- 8 C of setpoint (so a pit problem is not called a stall)
STALL    -> RISING       when slope_20min >= 3.3 C/h (6 F/h)
RISING|STALL -> WRAPPED  on user WRAPPED, or auto-suggest when slope jumps >= 3x within 30 min after STALL (ask to confirm)
WRAPPED  -> RISING       on user UNWRAPPED (expect a drop of up to 5 C; suppress "meat falling" alarm for 20 min)
any      -> FINISHING    when meat_c >= target - 17 C (30 F): start RAMP-DOWN (6.4)
FINISHING -> TENDER_CHECK when meat_c >= tender_check_from (collagen cuts) : notify "probe for tenderness"
TENDER_CHECK -> TENDER_CHECK on TENDER_NO: hold pit, re-notify every 30 min (interval NOT VERIFIED, DESIGN)
TENDER_CHECK -> DONE     on TENDER_YES
FINISHING -> DONE        (non-collagen cuts) when meat_c >= target - pull_offset, and safety gate passed (6.6)
DONE     -> HOLD         immediately: pit setpoint := hold pit C (Flame Boss / FireBoard / Traeger pattern)
HOLD     -> REST         on REMOVED (meat_c falling >= 1 C/min while pit steady, or user event): start rest timer
REST     -> SERVE_WINDOW when meat_c in 66..71 C (150..160 F) for collagen cuts, or after 10 min for lean cuts; alarm
any      -> ALARM        per 6.6 watchdogs
```

slope_N = least-squares line over the last N minutes of 1-minute samples (HeaterMeter method); a fit is valid only with >= 30 min of data and > 0.6 C/h (1 F/h) slope.

### 6.4 Ramp-down (BBQ Guru ramp mode, patented behaviour, re-implemented from the manual's description)
When meat_c >= target - 17 C: pit_setpoint := max(hold_pit, min(pit_setpoint, meat_c + 17 C)) recomputed each minute. Effect: the pit walks down toward 17 C above the meat target so the meat approaches target asymptotically and cannot overshoot; the pit never goes below the hold temperature. Precondition (UltraQ): pit setpoint more than 17 C above meat target; for POULTRY at 149 C pit / 74 C target this is always true; for LEAN_BEEF at 107 C / 57 C also true. Ramp-down is OFF by default for BRISKET and PORK_BUTT (they need pit heat for collagen; Guru also ships it off by default) and ON by default for the lean profiles.

### 6.5 Estimated time to done
Only shown when: state is RISING or WRAPPED, fit valid, meat has risen >= 9 C from start. ETA = (target - meat_c) / slope_60min. In STALL show "stalled, ETA unavailable". Reset the fit window on WRAPPED/UNWRAPPED and on any pit setpoint change (MEATER documents that wrapping and heat changes break its estimate; HeaterMeter's author calls the linear fit "sort of worthless" through the stall, so the UI must say so instead of showing a number).

### 6.6 Safety watchdogs (alarms, never silent)
- DANGER_ZONE: accumulate time with meat_c between 4 and 60 C after INSERTED. Warn at 3 h, alarm at 4 h (industry 4-hour rule; FSIS calls > 6 h at 10 to 54 C "especially hazardous", primary text NOT VERIFIED).
- HOLD_FLOOR: in HOLD/REST, alarm if meat_c < 60 C (140 F) and falling; "serve or refrigerate within 2 h" if it stays below 60 C.
- POULTRY_LETHALITY: DONE requires meat_c >= 74 C at any instant, OR integrated time-at-temperature from the FSIS table (use the fattest column). Implement as a lookup of minimum seconds per 1 C bin from 58 to 74 C and integrate.
- WHOLE_MUSCLE: DONE for beef/pork whole cuts requires >= 63 C plus a 3 min timer, unless the user set a lower target (rare beef) which is logged as USER_OVERRIDE and the DANGER_ZONE timer still runs.
- FROZEN: refuse to start a cook state if meat_c < 0 C at INSERTED.
- PIT_LOW_FOR_MEAT: warn if pit < 107 C (225 F, USDA floor) while any meat probe is < 60 C for more than 60 min.
- HOLD_TOO_LONG: warn at 12 h in HOLD (Caldwell County 17 h, Franklin ~9 h, BarbecueFAQ 10 to 16 h).
- Pit alarms from section 4 (deviation, FUEL_LOW, FIRE_OUT) escalate to the phone; meat alarms never affect the blower except through the ramp-down setpoint.

### 6.7 What the pit loop does differently because meat is on
- HOLD max duty stays at BLOWER_MAX_DUTY (60 percent) and the floor duty option (5 to 10 percent with damper) is recommended once wood chunks are added, to keep smoke clean (section 1 item 2).
- In STALL the controller does nothing to the pit (raising the pit to "push through" is a user choice, offered as a one-tap "+14 C for 1 h" button, ThermoWorks / Franklin practice). It logs mean duty during STALL as dryness_index.
- Post-wrap step to the profile's post-wrap pit setpoint is offered as a prompt, not automatic.
- Fuel: at cook start the user enters basket load (kg) and fuel type. The Cook Manager compares fuel-gauge hours remaining against the meat ETA and warns "refuel before the stall" when hours remaining < ETA + 1 h. Refuelling during the stall is the least harmful time (meat temperature is flat anyway).

### 6.8 Firmware mapping
- New module src/control/cook_manager.{h,cpp}, pure C++, host-testable like state_machine. One instance per meat probe. Runs in the same 1 Hz task after the pit state machine; it may only call setSetpoint() on the pit state machine (ramp-down, hold) and raise notifications.
- New HAL: hal/meat_sensor (second MAX31856 on the same SPI bus, CS on GPIO14, or MAX31855 for K-type meat probes; NTC 100k needs an ADC path, not planned).
- New config.h constants: profile table, STALL thresholds, DANGER_ZONE hours, hold floor, FSIS lethality table.
- New BLE characteristics (encrypted writes): MEAT_TEMP[2] (notify), MEAT_PROFILE (rw), MEAT_STATE (notify), COOK_EVENT (write: WRAPPED/UNWRAPPED/TENDER_YES/TENDER_NO/REMOVED/SERVE), ETA (notify), ALARM (notify, bitmask). UUIDs ...000b to ...0011.
- Unit tests: stall entry/exit on synthetic curves, wrap detection, ramp-down never below hold, poultry lethality integration, danger-zone timer, unwrapped drop not flagged.

## 6a. Short cooks (DECIDED 2026-10-22, Jason confirmed the numbers; firmware side BUILT the same day, doc 05 "Short cooks build")

Scope (Jason, 2026-10-22): sear is dropped; short direct cooks of 20 to 60 min (pork belly, chicken, wings) stay. Manual mode (constant duty plus a timer) is set aside, see doc 09 N9.

What the sources say about pit temperature for this class of cook:
- Drum makers run 135 to 155 C (275 to 310 F) as the normal "fast and hot" range (Pit Barrel Cooker blog); Ace lists 275 to 325 F for the same drum.
- Chicken skin: 175 to 190 C (350 to 375 F) on a WSM gave bite-through skin in about 50 min; a forum cook does half birds at 205 to 220 C (400 to 425 F) and warns not to exceed 230 C (450 F) dome.
- Pork belly hot and fast: 140 to 175 C (grillfuerst guide); crisp-skin finishes quoted at 205 to 230 C but every smoker source says the skin crisps properly only over direct heat. That is the sear case, dropped.
- Drum behaviour: startup spikes to about 200 C and lid-open raises the pit (section 1 item 7), so the over-limit must sit well above the highest short-cook setpoint, not 20 C above it as today (SETPOINT_MAX 180, PIT_SOFT_LIMIT 200).
- Doneness is by internal temperature in every source, not time. A short cook is still a pit setpoint cook; the timer is a reminder, the meat probe (N1) is the real end condition.

Proposal:
| Item | Today | Proposed | Why |
|---|---|---|---|
| SETPOINT_MAX_C | 180 | 200 | Covers 275 to 390 F, the whole hot-and-fast range the sources use without direct heat. 400 F+ is sear territory |
| PIT over-limit | fixed PIT_SOFT_LIMIT_C 200 | dynamic: setpoint + PIT_OVER_MARGIN_C 40, capped at PIT_ABS_LIMIT_C 240 | 40 C margin clears the drum startup spike and lid-open rise at any setpoint; at setpoint 110 the limit is 150 (today 200), at 200 it is 240. The absolute cap keeps the software limit under the hard cutoff |
| Hard cutoff | dial thermostat in series, dial 50 to 320 C, trip never measured (M4) | keep it: set the dial to 270 C and prove the trip before the dial is removed (M4), then it is fixed. Second one-shot device DECIDED 2026-10-22 (Jason): Thermodisc MICROTEMP G4 one-shot thermal fuse, series tops out at 257 C, in the fan supply (doc 07 A14; wall temperature M10 first). No bimetal manual-reset switch above 150 C max surfaced in the search | Order must stay: software 240 < thermostat 270 < anything the drum or probe cannot take. 257 C fuse on the drum wall: wall temperature at the port is not the pit air temperature, measure at first fire before trusting it (new M row) |
| Cook timer | none | countdown, minutes, set over console and BLE, alarm at zero (buzzer N3 when it exists, BLE notify now). Does not change the setpoint | Short cooks are timed in every recipe; the meat probe is the Phase 2 end condition |
| Manual mode | `m` console duty only | deferred, N9 | Constant duty plus timer; nothing in the sources needs it for a short cook |

Gains: none of the sources suggest short cooks need different PID gains; the drum and blower are the same plant. Verify on the first 175 C cook.

Sources (web, 2026-10-22, not re-fetched): pitbarrelcooker.com blog "fast and hot vs low and slow" and "Signature Chicken"; tips.acehardware.com PBC overview; tvwbb.com threads 74686 (hot and fast chicken) and 91321 (leg quarters); grillfuerst.de "hot und fast bbq"; smokingmeatforums.com thread 308808 (crisping belly skin); element14 Thermodisc G4/G5 listings; uk.rs-online.com thermostatic switches 1117178, 2228248.

## 7. Concrete deltas to the current Phase 1 firmware (IMPLEMENTED 2026-09-21, see 05-firmware-framework.md section "Phase 1.5 changes"; the text below is the spec as written before implementation)

config.h changes:
```
IGNITE_DUTY_PCT            90  -> 50        (E1; PBC fan users cap at 50)
IGNITE_EXIT_MARGIN_C       15  -> 17        (BBQ Guru setting 1: 30 F below setpoint)
BLOWER_MAX_DUTY_PCT        100 -> 60        (E3; tunable over BLE, already in NVS)
+ STARTUP_MAX_DUTY_PCT     50               (RAMP and RECOVERY cap; also PID integral clamp)
+ BLOWER_FLOOR_DUTY_PCT    0                (optional trickle in HOLD, E18)
LID_OPEN_DROP_C_PER_S      1.0 -> 0.3       (LidTrack: -0.5 F/s at 225 F = -0.28 C/s; scale to 1.1 C/s at 260 C)
LID_OPEN_TOTAL_DROP_C      8   -> keep 8, but as a classic backstop only (HeaterMeter 6 percent of SP = 7 C at 121 C)
LID_OPEN_TIMEOUT_S         300 -> 240 rate mode, 600 classic backstop
+ LID_OPEN_MIN_S           30               (HeaterMeter LIDOPEN_MIN_AUTORESUME)
+ LID_EXIT_RISE_ABOVE_LOW_C 11              (LidTrack: 20 F above the low peak) and plateau 0 < dps < 0.06 C/s
+ LID_RISING_RATE_C_PER_S  0.3 for 20 s at duty <= 10  (E6, drum-specific)
+ OUTPUT_EMA_ALPHA         2/241            (HeaterMeter PIDOUTPUT_AVG_SMOOTH)
+ PROBE_EMA_ALPHA          2/61             (derivative source)
+ FUEL_LOW_OUTPUT_PCT      90, FUEL_LOW_FALL_C 5 over FUEL_LOW_WINDOW_S 600
+ FIRE_OUT_BELOW_SP_C      40 for FIRE_OUT_S 900 with duty pegged -> FAULT
+ PIT_ALARM_DEV_C          14 (25 F, UltraQ/Signals), PIT_ALARM_HYST_C 1
+ LONG_PWM_PERIOD_S        10               (below min-start duty)
+ FUEL_FAN_CFM_NOMINAL     10.0             (blower rating, for the fuel gauge)
+ FUEL_KG_PER_M3_AIR       0.047            (1 kg carbon per ~9.5 m3 air at stoichiometric, x0.45 excess-air factor; CALIBRATE)
+ FUEL_LOW_FRACTION        0.20
+ REFUEL_MUTE_S            600
+ CREEP_ABOVE_SP_C         14 for CREEP_S 900 at duty 0 -> alarm (E25)
PID_KP/KI/KD_DEFAULT       note: HeaterMeter 4 / 0.02 / 5 are per F. Per C equivalents 7.2 / 0.036 / 9. Our 4.0 / 0.02 / 0 per C are therefore softer than HM defaults, which is the right side to err on for a thin drum. Keep; derive from the step-response cook.
```

state_machine changes:
- RAMP: duty clamp = STARTUP_MAX_DUTY; PID integral max = STARTUP_MAX_DUTY; on first crossing of setpoint, integral *= 0.5, clamp -> BLOWER_MAX_DUTY.
- New state RECOVERY (after LID_OPEN): same caps as RAMP, exits to HOLD on the existing HOLD_BAND / HOLD_SETTLE rule.
- LID_OPEN entry: (rate trigger OR classic trigger) AND setpoint reached since last change AND output EMA < 90 AND pit < setpoint. Plus the rising variant (E6).
- LID_OPEN exit: max(30 s) then (plateau rule OR pit >= setpoint - band OR timeout) -> RECOVERY.
- HOLD: FUEL_LOW warning and FIRE_OUT fault checks; pit deviation alarm armed after first setpoint reach, muted in LID_OPEN/RECOVERY.
- Setpoint change in HOLD -> RAMP (HeaterMeter behaviour).
- Boot: if NVS cook_active, enter RAMP with stored setpoint, log POWER_RESUME.

pid changes:
- Derivative on (probe EMA - current), conditional integration anti-windup (integrate only when not saturated in the error's direction), integral clamp [0, imax] where imax is provided by the state machine.

blower changes:
- Long-PWM below min-start: 10 s period at min-start duty for (requested/min-start) fraction, instead of jumping to 0. Keep the 300 ms kick.
- Optional tach input and BLOWER_STALL fault (4-wire option only).

Unit tests to add: lid rate trigger and plateau exit on a synthetic drop/recover curve; fire-out guard blocks lid mode; rising-lid trigger; FUEL_LOW; startup cap and halve-I; long-PWM duty accounting; power-resume path.

## 8. Verification status

VERIFIED by direct read this session: HeaterMeter grillpid.cpp / grillpid_conf.h / hmcore.cpp constants (loop 1000 ms, EMA alphas, long PWM 10, boost 3, LIDOPEN_MIN_AUTORESUME 30, lambda 0.4, defaults 225 / 6 percent / 240 s / P 4 I 0.02 D 5 / startup max 100, output EMA < 90 guard, integral *= 0.5); PiFire common.py (LidOpenThreshold 15, LidOpenPauseTime 60, maxtemp 550, min/max startup temp 75/100); FireBoard Drive Blower (docs.fireboard.io: 20 CFM, 30 x 40 mm opening, adapter -100 to 500 F; atbbq.com: 12 VDC 0.68 A 8 W, 2.5 mm barrel, 4.5 ft, sliding damper, $59); Same Sky CBM-7530BF datasheet (12 V, 0.20/0.30 A, 10.14 CFM, 0.33 inH2O, 40.4 dBA, -10 to 70 C, PWM 10 to 60 kHz VIH 2.5 to 6 V, open-collector tach, wire colours); Delta BFB0712HH-AWPH datasheet (7.0 to 13.8 V, 0.34/0.42 A, 15.3 CFM, 0.977 inH2O, 51 dBA, 25 kHz PWM 0 to 5.5 V, rotor stops at 0 percent, full speed with PWM lead open, open-collector tach 5 mA); BBQ Guru UltraQ guide (ramp 30 F, Smart Cook 30/40/20 F, custom 5 to 12 s and 10 to 50 F, deviation default 25 F range 10 to 100 F, hold 200 F).

Reported by the research agents with URLs, not re-fetched by me: everything else in sections 1, 2, 5 and the edge-case evidence column. Treat as VERIFIED-BY-AGENT; spot-check before quoting externally.

NOT VERIFIED (do not act on without checking): Pit Viper voltage/motor type/connector; FireBoard and Flame Boss blower motor type; Billows CFM (10 vs 46 conflict); Auber BL series specs; Same Sky internal PWM pull-up voltage; Delta tach pulses per revolution; any product's lid-detect thresholds (BBQ Guru, FireBoard, Flame Boss, Billows); FireBoard PID gains; drift rates for thermocouples; the "oversized fan makes dirty smoke" claim; Franklin's exact warmer setpoint and book text; FSIS "6 h at 50 to 130 F" primary text; a re-check interval for "not tender"; any controlled data on pit temperature vs juiciness; collagen conversion rate constants; whether a fan-off draft leak exists on our specific blower nozzle (measure: with the fan off and the fire lit, does the pit still climb?).

## 9. Sources (primary first)

Controllers and open source
- HeaterMeter source: https://github.com/CapnBry/HeaterMeter (arduino/heatermeter/grillpid.cpp, grillpid_conf.h, hmcore.cpp; openwrt/package/linkmeter/luasrc/linkmeter/peaks.lua)
- HeaterMeter wiki: PID-Controller-Theory, PID-Tuning, Configuration-WebUI-Documentation, HeaterMeter-Operation, Max-Blower-Power, Servo-Mode, Blower-and-Servo-Wiring (https://github.com/CapnBry/HeaterMeter/wiki/...)
- LidTrack: https://tvwbb.com/threads/feature-spotlight-lidtrack-smarter-lid-mode-detection.84330/
- PiFire: https://github.com/nebhead/PiFire (common/common.py, control.py, controller/pid.py, notify/notifications.py)
- BBQ Guru UltraQ guide: https://www.bbqguru.com/wp-content/uploads/2023/02/UltraQ-User-Guide.pdf ; DigiQ DX3 manual: https://www.bbqguru.com/wp-content/uploads/2023/02/DigiQDX3-Manual-Rev1.0-Firm1.1.pdf ; CyberQ WiFi guide: https://www.bbqguru.com/wp-content/uploads/2023/02/CyberQ-WiFi-Firmware-V2.2-USER-GUIDE-rev-6.pdf ; PartyQ manual: https://www.bbqguru.com/wp-content/uploads/2023/02/UPDATED-PARTYQ-MANUAL-V1-3-09.28.17.pdf ; fan sizing: https://thebbqguru.freshdesk.com/support/solutions/articles/14000141257-what-fan-size-do-i-need- ; UDS adapter: https://www.bbqguru.com/cooker/ugly-drum-smoker-uds-with-3-4-ball-valve/ ; probe placement: https://thebbqguru.freshdesk.com/support/solutions/articles/14000141387-dynaq-the-pit-temperature-is-not-correct-
- FireBoard: https://docs.fireboard.io/drive/index.html , /drive/drive-settings/ , /drive/drive-usage/ , /drive/drive-faq/ , /drive/drive-programs/ , /drive/blower.html ; blower retail spec: https://www.atbbq.com/products/fireboard-drive-blower-20cfm ; Analyze: https://www.fireboard.com/blog/analyze-feature/
- Flame Boss: FB500 manual https://www.flameboss.com/manuals/flame-boss-500-english-manual/ ; damper FAQ https://support.flameboss.com/support/solutions/articles/14000045772-damper-on-blower ; blower/PSU specs https://support.flameboss.com/support/solutions/articles/14000123583-flame-boss-adapter-plate-sizes-blower-power-supply-specs ; open pit pause https://support.flameboss.com/support/solutions/articles/14000075890-how-to-change-the-default-open-pit-pause-setting-on-fb100-fb200-fb300 ; blower not running https://support.flameboss.com/support/solutions/articles/14000101381-blower-not-running
- ThermoWorks Billows: https://www.thermoworks.com/products/billows ; operating instructions PDF (cdn.shopify.com ... billows_operating_instructions.pdf) ; setup https://help.thermoworks.com/knowledge-base/billows-set-up-signals/ ; troubleshooting https://help.thermoworks.com/knowledge-base/troubleshooting-billows/ ; support https://thermoworks.zendesk.com/hc/en-us/articles/45419078025371
- Smobot: https://smobot.com/ , https://shop.smobot.com/products/smobot-grill-controller
- Traeger: Super Smoke https://support.traeger.com/hc/en-us/articles/4407213220507 ; controller functions https://support.traeger.com/hc/en-us/articles/4407213234843 ; low temp https://support.traeger.com/hc/en-us/articles/4407213171099 ; keep warm https://www.traeger.com/learn/traeger-keep-warm-temp-guide
- Green Mountain Ledge Prime 2.0 manual: https://www.greenmountaingrills.com/wp-content/uploads/2024/04/LEDGE-PRIME2.0-MANUAL-LR.pdf
- MEATER: https://support.meater.com/hc/en-us/articles/36914985836315 ; https://support.meater.com/hc/en-us/articles/37182246162331

Fans
- Same Sky CBM-7530BF: https://www.sameskydevices.com/product/resource/cbm-75bf.pdf ; CBM-5015BF: https://www.sameskydevices.com/product/resource/cbm-50bf.pdf
- Delta BFB0712HH-AWPH: https://www.delta-fan.com/Download/Spec/BFB0712HH-AWPH.pdf
- Intel 4-wire PWM fan spec rev 1.2: https://www.konilabs.net/docs/standards/fan/intel_4wire_pwm_fans_specs_rev1_2.pdf ; Noctua PWM white paper: https://cdn.noctua.at/media/Noctua_PWM_specifications_white_paper.pdf
- Draft leak / dampers: https://tvwbb.com/threads/runaway-temps-cheap-fix-long-story.43549/ ; https://tvwbb.com/threads/kamado-joe-classic-do-i-need-a-damper.54072/ ; https://tvwbb.com/threads/why-servo-damper-and-variable-speed-fan-blower.72032/ ; https://tvwbb.com/threads/prevent-overshooting.83193/ ; https://tvwbb.com/threads/introducing-the-roto-damper.46004/ ; Flame Boss wind test https://www.nakedwhiz.com/productreviews/flamebossblower/fbblowerupgrade.htm
- Charcoal: CPSC Charcoal Grill Safety Tips https://www.cpsc.gov/s3fs-public/465CharcoalGrillSafetyTipsWEB_0.pdf ; CPSC never indoors https://www.cpsc.gov/Newsroom/News-Releases/1999/CPSC-Warns-Never-Use-Charcoal-Grills-Indoors ; UDS burn rate thread https://www.smokingmeatforums.com/threads/new-uds-goes-through-charcoal-too-fast.308053/ ; PBC instructions (8 lb, 8 to 12 h) https://pitbarrelcooker.com/pages/instructions
- UDS fan sizing (forum): https://www.smokingmeatforums.com/threads/pid-fan-recommendations-for-uds-build.324560/ ; https://www.bbq-brethren.com/threads/bbq-guru-pit-bull-fan-25cfm-too-much-for-uds.256853/ ; https://tvwbb.com/threads/bigger-fan-options-when-moving-to-a-bigger-smoker.80645/

Pit design
- amazingribs vents: https://amazingribs.com/more-technique-and-science/grill-and-smoker-setup-and-firing/vents-control-temperature/ ; smoke science: https://amazingribs.com/more-technique-and-science/grill-and-smoker-setup-and-firing/science-of-wood-and-smoke/ ; water pans: https://amazingribs.com/more-technique-and-science/more-cooking-science/drip-pans-and-water-pans/ ; PBC setup: https://amazingribs.com/more-technique-and-science/grill-and-smoker-setup-and-firing/amazing-pit-barrel-cooker-setup-tips/ ; cold weather: https://amazingribs.com/more-technique-and-science/grill-and-smoker-setup-and-firing/how-grill-and-smoke-cold-weather/
- Gateway FAQ: https://www.gatewaydrumsmokers.com/pages/frequently-asked-questions ; PBC instructions: https://pitbarrelcooker.com/pages/instructions ; UDS builds: https://grilling24x7.com/ugly-drum-smoker-part-3/ , https://howtobbqright.com/udssmoker/ ; PBC fan thread: https://pitmaster.amazingribs.com/forum/charcoal-drums-barrels-and-bullet-smokers/pit-barrel-cooker/659951-anyone-use-a-fan-control-on-the-pbc ; minion with fan: https://tvwbb.com/threads/long-smokes-pit-controllers-and-the-minion-method-is-this-the-best-charcoal-setup.81301/ ; stoke/choke: https://pitmaster.amazingribs.com/forum/accessorize-your-pit/thermostatic-controllers-aa/972355-temp-controlling-fan-on-offset
- Big Green Egg: https://biggreenegg.com/blogs/guides/how-to-control-temperature ; Franklin pit: https://franklinbbqpits.com/collections/franklin-bbq-pit/products/franklin-bbq-pit ; Mill Scale 94: https://millscale.co/products/94-gallon

Meat science
- Tornberg 2005 Meat Science 70(3) PMID 22063748; Purslow et al. 2016 Food Research International 89 PMID 28460973; Vaskoska et al. 2020 Foods 9(9):1289 PMID 32937816; Alahakoon et al. 2017 PMID 28888449; Li et al. 2019 PMID 30562641; Ismail et al. 2019 PMID 30208691
- Baldwin sous vide guide: https://douglasbaldwin.com/sous-vide.html ; roasting: https://douglasbaldwin.com/roasting.html
- amazingribs: meat science https://amazingribs.com/technique-and-science/cooking-science/basic-meat-science/ ; stall https://amazingribs.com/more-technique-and-science/more-cooking-science/understanding-and-beating-barbecue-stall/ ; crutch https://amazingribs.com/more-technique-and-science/more-cooking-science/texas-crutch/ ; resting https://amazingribs.com/more-technique-and-science/more-cooking-science/science-of-resting-meat/ ; faux cambro https://amazingribs.com/more-technique-and-science/more-cooking-science/faux-cambro/ ; brisket https://amazingribs.com/tested-recipes/beef-and-bison-recipes/smoked-brisket-texas-style/
- Modernist Cuisine stall: https://modernistcuisine.com/mc/barbecue-stall/
- ThermoWorks: muscle fibres https://blog.thermoworks.com/coming-heat-effects-muscle-fibers-meat/ ; brisket https://blog.thermoworks.com/thermal-tips-smoked-brisket/ ; hot and fast https://blog.thermoworks.com/hot-and-fast-brisket-done-in-half-the-time/ ; low and slow https://blog.thermoworks.com/cooking-low-slow-meat-doing/
- Franklin (MasterClass summary): https://www.masterclass.com/articles/how-to-smoke-brisket-with-barbeque-pitmaster-aaron-franklin ; NPR holds: https://www.npr.org/sections/thesalt/2015/06/08/411778404/pitmasters-embrace-new-barbecue-truth-rested-meat-is-sublime ; VWB oven hold: https://www.virtualweberbullet.com/using-electric-oven-to-hold-brisket-at-temperature/ ; BarbecueFAQ holds (2026-09-04): https://barbecuefaq.com/brisket-done-too-early/ ; TAMU wrap test: https://extension.animalscience.tamu.edu/bbq/?p=651
- Safety: https://www.foodsafety.gov/food-safety-charts/safe-minimum-internal-temperatures (2024-11-21) ; USDA Smoking Meat and Poultry (2011): https://www.govinfo.gov/content/pkg/GOVPUB-A110-PURL-gpo26768/pdf/GOVPUB-A110-PURL-gpo26768.pdf ; FSIS poultry lethality table (mirror): https://riverstonehealth.org/wp-content/uploads/SousVide_LethalityChart_Poultry.pdf ; FDA Food Code 3-501.16 (Delaware adoption): https://www.law.cornell.edu/regulations/delaware/16-Del-Admin-Code-SS-3-501.16
- ETA methods: HeaterMeter https://tvwbb.com/threads/can-heatermeter-calculate-time-to-reach-the-target-meat-temperature.69415/ ; Home Assistant https://community.home-assistant.io/t/calculate-cook-time-based-on-meat-probe-temperature/267649
