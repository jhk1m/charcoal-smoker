# Blower adapter v1 - print and fit notes

File to submit: blower-adapter-delta-BFB0712HH-to-1in-duct-v1.stl
Created 2026-09-25. Units mm. Envelope 36.0 x 29.0 x 61.5 mm. Solid volume 9.54 cm3.

## What it does
Joins the Delta BFB0712HH-AWPH blower's rectangular outlet to the smoker's existing
1 in ID hose coupling, which clamps over the 23.81 mm OD air duct on the left post.

## Dimensions and where each came from
| Feature | Value | Source |
|---|---|---|
| Fan outlet opening | 24.1 x 17.5 mm | Delta BFB0712HH-AWPH spec, dimension drawing page 5, VERIFIED |
| Locating lip, outer | 23.3 x 16.7 mm, 3.4 mm tall | 0.8 mm clearance inside the fan opening, for print tolerance |
| Locating lip, bore | 21.7 x 15.1 mm | lip wall 0.8 mm |
| Flange plate | 36 x 29 x 3 mm | butts flat against the fan face, sealing surface |
| Internal channel at the flange | 24.1 x 17.5 mm | recovers full outlet area after the lip |
| Transition | 25 mm long, rect to round | area 422 mm2 -> 346 mm2, gradual |
| Round stub | OD 23.8 mm, ID 21.0 mm, 25 mm long | matches the duct's 23.81 mm OD so the same 1 in ID hose and worm clamp fit both ends |
| Retention bead | OD 25.0 mm at 4 to 6.5 mm from the tip | stops the hose walking off under clamp load |
| Wall | 1.4 mm stub and transition, 0.8 mm lip | 3 to 4 perimeters at 0.4 mm nozzle |

## Print settings
- Orientation: round stub DOWN on the plate, flange and lip up. Self supporting, NO supports needed.
- Brim recommended: the first layer is a 1.4 mm wide ring, small footprint.
- Layer 0.2 mm, 3 to 4 perimeters, 20 to 25 percent infill. The part is nearly solid at these walls.
- Material: PLA is what the library offers. See the caveat below.

## PLA caveat, read before relying on this part
PLA softens near its glass transition, commonly quoted at 55 to 65 C. This adapter sits at the
duct inlet on the outside of the drum, upstream of the fire, and the backdraft flap is between it
and the duct, so in normal operation it should stay near ambient. It has NOT been measured.
Treat the PLA print as a fit-check and first-cook prototype. Measure the duct inlet temperature
on the first cook (doc 06 R12). If it runs warm, reprint in PETG or ABS.
Grey resin is worse here: brittle and no better thermally. Do not order resin.

## After the print
1. Dry fit the lip into the fan outlet. If tight, sand the lip, do not force it.
2. Seal the flange to the fan face with high temp RTV or foil tape.
3. Slide the 1 in ID silicone or EPDM hose over the stub past the bead, worm clamp it.
4. Backdraft flap goes inside the hose between this stub and the duct.
5. Verify the outlet really is 24.1 x 17.5 with calipers when the fan arrives. If it differs,
   rerun gen_adapter.py with the measured numbers in the KEY table.

## Regenerating
gen_adapter.py builds the STL from the KEY table of cross sections. verify.py checks the mesh is
manifold, has outward normals, no degenerate triangles, and prints the cross section sizes.
