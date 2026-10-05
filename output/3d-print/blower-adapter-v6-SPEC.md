# Blower adapter v6 (direct to duct) - full specification

File: blower-adapter-v6-direct.stl
Generated 2026-10-01 by gen_v6.py. Units mm. Single body, no assembly, no fasteners.

## What it is
A one-piece transition that carries air from the Delta BFB0712HH-AWPH blower outlet into the
smoker's existing 23.81 mm OD air duct at the top of the left post. A socket at each end: the
lower one slips over the fan's outlet duct, the upper one slips over the smoker's duct. No hose,
no coupling, no backdraft flap.

## Envelope
Bounding box      45.40 x 30.50 x 65.00 mm
Solid volume      14.07 cm3
Mass in PLA       about 17 g at 1.24 g/cm3
Cost at $0.05/g   about $0.87
Mesh              68,096 triangles, manifold, outward normals, no degenerate faces

## Interfaces
FAN END, z = 0 to 3.0
  bore           40.40 x 25.50 mm, rounded-rectangular
  mates to       fan outlet duct, 39.90 x 25.00 mm outside (Delta STEP model, confirmed by caliper at 40)
  clearance      0.25 mm per side
  depth          3.0 mm, chosen to seat inside the 3.5 mm the collar stands proud at its shallowest
  stop           internal shoulder at z = 3.1 lands on the duct's end rim (2.00 mm wide on the long
                 sides, 6.00 and 1.50 on the short ones)

DUCT END, z = 43.1 to 65.0
  bore           24.20 mm round
  mates to       smoker duct, 23.81 mm OD (measured 2026-09-24)
  clearance      0.195 mm per side
  depth          21.9 mm
  outside        28.20 mm, which is the surface a worm clamp squeezes
  stop           internal shoulder at z = 43.0, bore steps 21.0 to 24.2, duct butts against it

## Airway
The opening through the shoulder is 35.90 x 17.50 mm, matching the fan's bore, and it is OFFSET
2.25 mm on the short axis because the fan's bore is not centred in its duct: the duct walls measure
2.00 / 2.00 on the long axis and 6.00 / 1.50 on the short one. A centred opening would hang 2.25 mm
into the airflow. THIS GIVES THE PART A CORRECT ORIENTATION. Rotated 180 degrees it fits but
obstructs. Mark it before fitting.

## Station table
```
  z mm     outer W x H mm      bore W x H mm  area mm2   note
  0.00    45.40 x  30.50    40.40 x  25.50     992.9   socket mouth, over the fan duct
  3.00    45.40 x  30.50    40.40 x  25.50     992.9   socket depth 3.0 (collar stands 3.5 proud)
  3.10    45.40 x  30.50    35.90 x  17.50     605.5   STOP SHOULDER lands on the duct end rim
  6.00    45.40 x  30.50    35.90 x  17.50     605.5   full outlet area restored
 10.00    39.90 x  21.50    35.90 x  17.50     605.5   body tapers to a 2 mm wall
 40.00    23.80 x  23.80    21.00 x  21.00     346.4   rect has become round, throat ID 21.0
 43.00    28.20 x  28.20    21.00 x  21.00     346.4   flare to the duct socket
 43.10    28.20 x  28.20    24.20 x  24.20     460.0   STOP SHOULDER: duct butts here
 65.00    28.20 x  28.20    24.20 x  24.20     460.0   socket end, 21.9 deep
```

## Flow path areas
fan outlet            628.2 mm2
throat (ID 21.0)      346.4 mm2   <- narrowest point in the part
smoker duct bore      383.6 mm2

The throat is deliberately just under the duct bore so there is no step for air to trip over on the
way in. It is 10 percent below the duct and the duct, not this part, remains the system restriction.

## Print settings
Orientation   fan socket face down on the plate. Everything above narrows, so NO SUPPORTS.
Layer         0.2 mm
Perimeters    3 to 4. This carries the hoop load where the clamps squeeze and keeps the wall airtight.
Infill        20 percent. Barely matters: the part is nearly all perimeter.
Brim          optional, the footprint is 45.4 x 30.5 so adhesion is not marginal.
Material      PLA as supplied by the library. See the thermal note below.

## Thermal note
PLA softens at 55 to 65 C. Calculated, not measured: the steel duct is an efficient cooling fin, so
conduction from the fire puts the connection at essentially ambient even with the basket end at
500 C. Fan running, it sits at ambient plus a few degrees. The exposure is fan OFF with hot gas
rising up the duct as a chimney, which with no flap fitted can reach pit temperature, 110 to 150 C
on a normal cook. Measure that joint on the first cook with the fan off for several minutes. Over
50 C means reprint in PETG or ABS.

## Assembly
1. Push the fan socket onto the blower outlet until the shoulder meets the rim. Check through the
   bore that the opening sits over the fan's bore, not offset the wrong way.
2. Seal with high temp RTV or foil tape. Three millimetres of engagement will not hold on its own.
3. Push the duct socket onto the smoker duct until it butts the shoulder, 21.9 mm of engagement.
4. Worm clamp over the 28.20 mm socket outside.

## Provenance of every dimension
39.90 x 25.00 fan duct outside   Delta STEP model, cross-sectioned; caliper confirmed at 40
35.90 x 17.50 fan bore            Delta STEP model; caliper confirmed exactly
2.00 / 6.00 / 1.50 wall thickness Delta STEP model, section at x = -37.0
3.5 mm collar protrusion          Jason's caliper
23.81 mm duct OD, 22.1 bore       Jason's caliper, 2026-09-24

Nothing in this part comes from the Delta datasheet PDF, which does not dimension the outlet at all.
