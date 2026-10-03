# Blower adapter v6 "direct" - spec as supplied by Jason 2026-10-02

File: blower-adapter-v6-direct.stl. NOT IN THIS REPO as of 2026-10-02: neither the STL nor its generator
is on disk here. This file records the spec Jason pasted into the CLI session, from the session that
designed it. Versions v2 to v5 are not recorded anywhere in the repo. v1 (README-blower-adapter-v1.md,
gen_adapter.py, verify.py) is SUPERSEDED: it was a plug-in lip sized from the spec drawing's 24.1 x 17.5 mm,
which is not the real outlet. verify.py has not been run on v6 here.

One piece, blower outlet straight onto the smoker duct. No hose.

## Fan outlet as measured (replaces the 24.1 x 17.5 mm spec-drawing figure)
| Feature | Value |
|---|---|
| Outlet duct, outside | 39.90 x 25.00 mm |
| Outlet bore | 35.90 x 17.50 mm |
| Walls, long axis | 2.00 / 2.00 mm |
| Walls, short axis | 6.00 / 1.50 mm, so the bore is 2.25 mm off centre |
| Collar height | 3.5 mm proud of the housing at its shallowest |

## Part
Envelope 45.40 x 30.50 x 65.00 mm. 14.07 cm3. About 17 g in PLA, 87 cents at the library's rate.
Manifold mesh, 68,096 triangles (as stated by the designing session).

| Section | z (mm) | Detail |
|---|---|---|
| Blower socket | 0 to 3.0 | Bore 40.40 x 25.50 rounded rectangle, 3.0 mm deep, goes OVER the fan's outlet duct. Clearance 0.25 mm per side. 3.0 deep so it seats without bottoming on the housing. Internal shoulder at z 3.1 lands on the duct's end rim |
| Airway | 3.1 onward | 35.90 x 17.50, the fan's own bore, no area lost at the joint. Offset 2.25 mm on the short axis to follow the off-centre bore |
| Transition | 10 to 40 | Rectangle lofts to round, wall tapers to 2.0 mm. Throat 21.0 mm bore, 23.8 mm outside, 346 mm2, deliberately just under the duct's 383 mm2 |
| Duct socket | 43.1 to 65.0 | Bore 24.20 round, 21.9 mm deep, over the 23.81 mm OD duct. Clearance 0.195 per side. Outside 28.20 mm for a worm clamp. Shoulder at z 43.0 where the bore steps 21.0 to 24.2, so the duct butts |

ORIENTATION: the airway offset means the part has one correct way round. Rotated 180 degrees it still fits
but puts a 2.25 mm lip in the airflow. Check through the bore that the airway lines up with the fan's bore
before sealing.

## Printing
Blower socket face down, no supports, 0.2 mm layers, 3 to 4 perimeters, 20 percent infill. Brim optional.

## Fitting
Blower socket on, RTV or foil tape to seal, duct socket on until it butts the shoulder, worm clamp over the
28.20 outside.

## Open questions raised in the CLI session 2026-10-02
- BACKDRAFT FLAP: v1 put the flap inside the 1 in hose between the stub and the duct. v6 has no hose, so the
  flap has no location. RESOLVED FOR NOW by Jason 2026-10-02: no flap for the first tests, the duct is
  routed away from the barrel. Revisit after the fan-off draft test (doc 09 M3).
- The duct socket is rigid, unslotted plastic, so a worm clamp over it may not close the 0.195 mm clearance
  onto the duct. Not tested.
- PLA caveat from v1 still applies, and more so: this part touches the duct directly with no hose between.
- Print status of v6 not recorded.
