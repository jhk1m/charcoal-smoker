"""
Blower adapter v2. Delta BFB0712HH-AWPH outlet -> 1 in ID hose -> 23.81 mm OD duct.
Geometry taken from Delta's own STEP model (delta-fan.com/Download/3D/BFB0712HH-AWPH.stp),
cross-sectioned perpendicular to the outlet axis, and confirmed against Jason's calipers:
  outlet opening          35.90 x 17.50 mm   (STEP; calipers 35.9 x 17.5)
  outlet duct outside     39.90 x 25.00 mm   (STEP; calipers 40)
  straight duct available  6.7 mm before the scroll flares
This part is a SOCKET that slips over that duct and bottoms on its end rim.
Units mm. Z=0 at the socket mouth, part grows upward. Print socket-down, no supports.
"""
import math, struct

N = 128
CLR = 0.25          # clearance per side over the duct
SOCK_W, SOCK_H = 39.90 + 2*CLR, 25.00 + 2*CLR   # socket cavity 40.40 x 25.50
SOCK_WALL = 2.5
OPEN_W, OPEN_H = 35.90, 17.50                   # airway through the stop shoulder
STUB_OD, STUB_ID = 23.80, 21.00

OFF_Y = 2.25   # bore sits 2.25 mm off the duct centre on the short axis (STEP: walls 6.00 / 1.50)

def se(a, b, n, k):
    c, s = math.cos(k), math.sin(k); e = 2.0/n
    return a*math.copysign(abs(c)**e, c), b*math.copysign(abs(s)**e, s)

def lerp(p,q,t): return p+(q-p)*t

# z, outer a, outer b, outer n, inner a, inner b, inner n   (a,b = semi-axes)
so_a, so_b = (SOCK_W+2*SOCK_WALL)/2, (SOCK_H+2*SOCK_WALL)/2   # 22.70 x 15.25
si_a, si_b = SOCK_W/2, SOCK_H/2                               # 20.20 x 12.75
op_a, op_b = OPEN_W/2, OPEN_H/2                               # 17.95 x  8.75
KEY = [
    ( 0.00, so_a, so_b, 6.0, si_a, si_b, 6.0),   # socket mouth
    ( 3.00, so_a, so_b, 6.0, si_a, si_b, 6.0),   # socket depth 3.0: the collar stands only 3.5 proud
    ( 3.10, so_a, so_b, 6.0, op_a, op_b, 6.0),   # stop shoulder lands on the duct end rim
    ( 6.00, so_a, so_b, 6.0, op_a, op_b, 6.0),
    (10.00, op_a+2.0, op_b+2.0, 6.0, op_a, op_b, 6.0),  # taper the body down to a 2 mm wall
    (40.00, 11.90, 11.90, 2.0, 10.50, 10.50, 2.0),      # loft rect -> round, ID 21.0
    (43.00, 14.10, 14.10, 2.0, 10.50, 10.50, 2.0),      # flare out to the duct socket
    (43.10, 14.10, 14.10, 2.0, 12.10, 12.10, 2.0),      # STOP SHOULDER: bore opens to 24.2, duct butts here
    (65.00, 14.10, 14.10, 2.0, 12.10, 12.10, 2.0),      # 21.9 mm socket over the 23.81 mm OD duct
]

def densify(key, step=0.5):
    out=[]
    for i in range(len(key)-1):
        z0,z1=key[i][0],key[i+1][0]; n=max(1,int(round((z1-z0)/step)))
        for j in range(n):
            t=j/n; out.append(tuple(lerp(key[i][c],key[i+1][c],t) for c in range(7)))
    out.append(key[-1]); return out

L = densify(KEY)
def ring(lv, inner):
    z,ao,bo,no,ai,bi,ni = lv
    a,b,n = (ai,bi,ni) if inner else (ao,bo,no)
    # the airway is offset to match the duct; the socket bore and all outer surfaces stay centred.
    if inner and abs(a-op_a)<1e-6 and abs(b-op_b)<1e-6:
        dy = OFF_Y
    elif inner and z>9.5 and z<40.0:
        t = max(0.0,min(1.0,(z-10.0)/30.0)); dy = OFF_Y*(1.0-t)   # fade the offset out into the round stub
    else:
        dy = 0.0
    out=[]
    for i in range(N):
        x,y = se(a,b,n,2*math.pi*i/N)
        out.append((x, y+dy, z))
    return out

tris=[]
def quad(p1,p2,p3,p4): tris.append((p1,p2,p3)); tris.append((p1,p3,p4))
O=[ring(l,False) for l in L]; I=[ring(l,True) for l in L]
for i in range(len(L)-1):
    for j in range(N):
        k=(j+1)%N
        quad(O[i][j],O[i][k],O[i+1][k],O[i+1][j])
        quad(I[i][j],I[i+1][j],I[i+1][k],I[i][k])
for j in range(N):
    k=(j+1)%N
    quad(O[0][j],I[0][j],I[0][k],O[0][k])
    quad(O[-1][j],O[-1][k],I[-1][k],I[-1][j])

vol=0.0
for p,q,r in tris:
    vol+=(p[0]*(q[1]*r[2]-q[2]*r[1])-p[1]*(q[0]*r[2]-q[2]*r[0])+p[2]*(q[0]*r[1]-q[1]*r[0]))/6.0
def nrm(p,q,r):
    ux,uy,uz=q[0]-p[0],q[1]-p[1],q[2]-p[2]; vx,vy,vz=r[0]-p[0],r[1]-p[1],r[2]-p[2]
    nx,ny,nz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
    m=math.sqrt(nx*nx+ny*ny+nz*nz) or 1.0
    return nx/m,ny/m,nz/m
out="/home/claude/adapter2/blower-adapter-v6-direct.stl"
with open(out,"wb") as f:
    f.write(b"charcoal smoker blower adapter v2 socket mm".ljust(80,b" "))
    f.write(struct.pack("<I",len(tris)))
    for p,q,r in tris:
        f.write(struct.pack("<3f",*nrm(p,q,r)))
        for v in (p,q,r): f.write(struct.pack("<3f",*v))
        f.write(struct.pack("<H",0))
xs=[v[0] for t in tris for v in t]; ys=[v[1] for t in tris for v in t]; zs=[v[2] for t in tris for v in t]
print(f"tris {len(tris)}  bbox {max(xs)-min(xs):.2f} x {max(ys)-min(ys):.2f} x {max(zs)-min(zs):.2f}")
print(f"volume {abs(vol)/1000:.2f} cm3   PLA {abs(vol)/1000*1.24:.1f} g   ${abs(vol)/1000*1.24*0.05:.2f}")
print(f"signed volume {vol/1000:.2f} (positive = outward normals)")
