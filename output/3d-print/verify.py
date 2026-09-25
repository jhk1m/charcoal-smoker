import struct, math, collections
f=open("blower-adapter-delta-BFB0712HH-to-1in-duct-v1.stl","rb"); f.read(80)
n=struct.unpack("<I",f.read(4))[0]; tris=[]
for _ in range(n):
    d=struct.unpack("<12fH", f.read(50)); tris.append((d[3:6],d[6:9],d[9:12]))
print("tris",n)
# manifold check on quantised vertices
Q=lambda p: (round(p[0],4),round(p[1],4),round(p[2],4))
edges=collections.Counter()
for t in tris:
    a,b,c=map(Q,t)
    for e in ((a,b),(b,c),(c,a)):
        edges[tuple(sorted(e))]+=1
bad=[k for k,v in edges.items() if v!=2]
print("non-manifold edges:", len(bad))
# signed volume, sign tells winding
vol=0.0
for p,q,r in tris:
    vol+=(p[0]*(q[1]*r[2]-q[2]*r[1])-p[1]*(q[0]*r[2]-q[2]*r[0])+p[2]*(q[0]*r[1]-q[1]*r[0]))/6.0
print("signed volume cm3: %.3f (positive = outward normals)"%(vol/1000))
# degenerate triangles
deg=0
for p,q,r in tris:
    ux,uy,uz=q[0]-p[0],q[1]-p[1],q[2]-p[2]; vx,vy,vz=r[0]-p[0],r[1]-p[1],r[2]-p[2]
    nx,ny,nz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
    if math.sqrt(nx*nx+ny*ny+nz*nz)<1e-9: deg+=1
print("degenerate tris:", deg)
# cross-section extents at key heights: gather vertices near z
for z in (0.0,5.5,15.0,25.0,37.5,50.0,56.0,59.0,61.5):
    vs=[v for t in tris for v in t if abs(v[2]-z)<0.26]
    if not vs: print(f"z={z}: none"); continue
    xs=[v[0] for v in vs]; ys=[v[1] for v in vs]
    # split inner/outer by radius rank
    rs=sorted(math.hypot(v[0],v[1]) for v in vs)
    print(f"z={z:5.1f}  X span {max(xs)-min(xs):6.2f}  Y span {max(ys)-min(ys):6.2f}  r min {rs[0]:.2f} r max {rs[-1]:.2f}")
