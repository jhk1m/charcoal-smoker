"""
Charcoal smoker: Delta BFB0712HH-AWPH blower outlet -> 1 in ID hose -> 23.81 mm OD air duct.
Single-piece printed transition. Units: mm. Z = 0 at the round stub tip, part grows upward.

Source dimensions (VERIFIED, Delta spec sheet BFB0712HH-AWPH, dimension drawing page 5):
  fan outlet opening 24.1 x 17.5 mm rectangular
Measured on the smoker 2026-09-24: duct OD 23.81 mm, bore 22.1 mm, coupled with a 1 in ID sleeve.
"""
import math, struct

N = 128          # points around each ring
T = 1.4          # wall thickness, stub and transition

def superellipse(a, b, n, k):
    """Point on a superellipse at parameter angle k (radians). n=2 -> ellipse, n=6 -> rounded rect."""
    c, s = math.cos(k), math.sin(k)
    e = 2.0 / n
    x = a * math.copysign(abs(c) ** e, c)
    y = b * math.copysign(abs(s) ** e, s)
    return x, y

def lerp(p, q, t):
    return p + (q - p) * t

# (z, outer a, outer b, outer n, inner a, inner b, inner n)
# outer/inner are SEMI-axes, so OD = 2a.
KEY = [
    ( 0.00, 11.90, 11.90, 2.0, 10.50, 10.50, 2.0),   # stub tip, OD 23.8 / ID 21.0
    ( 3.50, 11.90, 11.90, 2.0, 10.50, 10.50, 2.0),
    ( 4.50, 12.50, 12.50, 2.0, 10.50, 10.50, 2.0),   # retention bead, OD 25.0
    ( 6.50, 12.50, 12.50, 2.0, 10.50, 10.50, 2.0),
    ( 7.50, 11.90, 11.90, 2.0, 10.50, 10.50, 2.0),
    (25.00, 11.90, 11.90, 2.0, 10.50, 10.50, 2.0),   # end of parallel stub, 25 mm of clamp length
    (50.00, 13.45, 10.15, 6.0, 12.05,  8.75, 6.0),   # loft round -> rect, inner 24.1 x 17.5
    (54.50, 18.00, 14.50, 6.0, 10.85,  7.55, 6.0),   # flange flare at 45 deg, inner narrows to lip bore
    (57.50, 18.00, 14.50, 6.0, 10.85,  7.55, 6.0),   # flange plate 3 mm, face that butts the fan
    (57.60, 11.65,  8.35, 6.0, 10.85,  7.55, 6.0),   # step down to the locating lip
    (61.00, 11.65,  8.35, 6.0, 10.85,  7.55, 6.0),   # lip, outer 23.3 x 16.7, wall 0.8
    (61.50, 11.35,  8.05, 6.0, 10.85,  7.55, 6.0),   # lead-in chamfer
]

def densify(key, step=0.5):
    """Insert intermediate rings so lofts are smooth."""
    out = []
    for i in range(len(key) - 1):
        z0, z1 = key[i][0], key[i + 1][0]
        span = z1 - z0
        n = max(1, int(round(span / step)))
        for j in range(n):
            t = j / n
            out.append(tuple(lerp(key[i][c], key[i + 1][c], t) for c in range(7)))
    out.append(key[-1])
    return out

LEVELS = densify(KEY)

def ring(level, inner):
    z, ao, bo, no, ai, bi, ni = level
    a, b, n = (ai, bi, ni) if inner else (ao, bo, no)
    pts = []
    for i in range(N):
        k = 2.0 * math.pi * i / N
        x, y = superellipse(a, b, n, k)
        pts.append((x, y, z))
    return pts

tris = []
def quad(p1, p2, p3, p4):
    tris.append((p1, p2, p3))
    tris.append((p1, p3, p4))

outer = [ring(L, False) for L in LEVELS]
inner = [ring(L, True) for L in LEVELS]

# outer skin, normals out
for i in range(len(LEVELS) - 1):
    for j in range(N):
        k = (j + 1) % N
        quad(outer[i][j], outer[i][k], outer[i + 1][k], outer[i + 1][j])
# inner skin, normals in (reverse winding)
for i in range(len(LEVELS) - 1):
    for j in range(N):
        k = (j + 1) % N
        quad(inner[i][j], inner[i + 1][j], inner[i + 1][k], inner[i][k])
# bottom annulus (z=0), faces down
for j in range(N):
    k = (j + 1) % N
    quad(outer[0][j], inner[0][j], inner[0][k], outer[0][k])
# top annulus, faces up
for j in range(N):
    k = (j + 1) % N
    quad(outer[-1][j], outer[-1][k], inner[-1][k], inner[-1][j])

# ---- volume by divergence theorem (signed tetra volumes)
vol = 0.0
for (p, q, r) in tris:
    vol += (p[0]*(q[1]*r[2]-q[2]*r[1]) - p[1]*(q[0]*r[2]-q[2]*r[0]) + p[2]*(q[0]*r[1]-q[1]*r[0])) / 6.0
vol = abs(vol)

def norm(p, q, r):
    ux, uy, uz = q[0]-p[0], q[1]-p[1], q[2]-p[2]
    vx, vy, vz = r[0]-p[0], r[1]-p[1], r[2]-p[2]
    nx, ny, nz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx
    m = math.sqrt(nx*nx+ny*ny+nz*nz) or 1.0
    return nx/m, ny/m, nz/m

out = "/home/claude/adapter/blower-adapter-delta-BFB0712HH-to-1in-duct-v1.stl"
with open(out, "wb") as f:
    f.write(b"charcoal smoker blower adapter v1 mm".ljust(80, b" "))
    f.write(struct.pack("<I", len(tris)))
    for (p, q, r) in tris:
        f.write(struct.pack("<3f", *norm(p, q, r)))
        for v in (p, q, r):
            f.write(struct.pack("<3f", *v))
        f.write(struct.pack("<H", 0))

xs = [v[0] for t in tris for v in t]; ys = [v[1] for t in tris for v in t]; zs = [v[2] for t in tris for v in t]
print(f"triangles      : {len(tris)}")
print(f"bbox mm        : {max(xs)-min(xs):.2f} x {max(ys)-min(ys):.2f} x {max(zs)-min(zs):.2f}")
print(f"solid volume   : {vol/1000.0:.2f} cm3")
print(f"PLA mass @1.24 : {vol/1000.0*1.24:.1f} g")
print(f"cost @ $0.05/g : ${vol/1000.0*1.24*0.05:.2f}")
print(f"file           : {out}")
