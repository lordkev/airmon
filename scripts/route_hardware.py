#!/usr/bin/env python3
"""Conservative two-layer grid router for this small, fixed carrier.

KiCad DRC remains the authority; this script never suppresses violations.
Routes avoid foreign pads/tracks and keep the temperature-sensor cavity clear.
"""
import pcbnew as k
from pathlib import Path
import heapq,math,json,time
ROOT=Path(__file__).resolve().parents[1];FILE=ROOT/'hardware/airmon.kicad_pcb'
b=k.LoadBoard(str(FILE));G=.1;W=421;H=551;N=W*H;CLR=.215
def mm(x):return k.ToMM(x)
def xy(p):return mm(p.x),mm(p.y)
def cell(x,y):return round(y/G)*W+round(x/G)
def point(i):return (i%W*G,(i//W)%H*G)
def vec(p):return k.VECTOR2I(k.FromMM(p[0]),k.FromMM(p[1]))
def inside(x,y):
 if not(.8<x<41.2 and .8<y<54.2):return False
 if y>47.2 and x>9.2:return False
 if 42.2<y<48.8 and x<3.8:return False
 return True
base=bytearray(N)
for iy in range(H):
 for ix in range(W):
  if not inside(ix*G,iy*G):base[iy*W+ix]=1
pads=[];groups={}
for f in b.GetFootprints():
 for p in f.Pads():
  x,y=xy(p.GetPosition());box=p.GetBoundingBox();lo=xy(box.GetOrigin());hi=xy(box.GetEnd())
  layers=[i for i,layer in enumerate([k.F_Cu,k.B_Cu]) if p.IsOnLayer(layer)]
  net=p.GetNetname();pads.append((net,lo,hi,layers))
  if net:groups.setdefault(net,[]).append((cell(x,y)+(layers[0]*N),x,y,layers[0]))
segments=[];vias=[]
def rectangle(mask,lo,hi,margin):
 x0=max(0,math.ceil((lo[0]-margin)/G));x1=min(W-1,math.floor((hi[0]+margin)/G))
 y0=max(0,math.ceil((lo[1]-margin)/G));y1=min(H-1,math.floor((hi[1]+margin)/G))
 if x1>=x0:
  row=bytes([1])*(x1-x0+1)
  for y in range(y0,y1+1):mask[y*W+x0:y*W+x1+1]=row
def disk(mask,x,y,r):
 for iy in range(max(0,math.ceil((y-r)/G)),min(H-1,math.floor((y+r)/G))+1):
  half=math.sqrt(max(0,r*r-(iy*G-y)**2));x0=max(0,math.ceil((x-half)/G));x1=min(W-1,math.floor((x+half)/G))
  if x1>=x0:mask[iy*W+x0:iy*W+x1+1]=bytes([1])*(x1-x0+1)
def obstacles(net,width):
 masks=[bytearray(base),bytearray(base)]
 for pn,lo,hi,ls in pads:
  if pn!=net:
   for l in ls:rectangle(masks[l],lo,hi,CLR+width/2)
 # SHT40 exposed die/cavity: no routing underneath the package.
 rectangle(masks[0],(4.52,51.25),(5.48,52.75),width/2)
 for sn,a,c,l,sw in segments:
  if sn!=net:
   r=CLR+(width+sw)/2
   # Router emits axis-aligned segments; a bounding rectangle is conservative.
   rectangle(masks[l],(min(a[0],c[0]),min(a[1],c[1])),(max(a[0],c[0]),max(a[1],c[1])),r)
 for vn,x,y in vias:
  if vn!=net:
   for m in masks:disk(m,x,y,CLR+.3+width/2)
 return masks
def route(start,goal,masks,vmasks,existing):
 def heuristic(i):
  a=i%N;c=goal%N
  return abs(a%W-c%W)+abs(a//W-c//W)+(0 if i//N==goal//N else 45)
 heap=[(heuristic(start),0,start)];dist={start:0};prev={}
 while heap:
  _,cost,u=heapq.heappop(heap)
  if cost!=dist[u]:continue
  if u==goal:
   path=[u]
   while u in prev:u=prev[u];path.append(u)
   return path[::-1]
  l=u//N;i=u%N;x=i%W;y=i//W
  candidates=[]
  if x>0:candidates.append((u-1,1))
  if x<W-1:candidates.append((u+1,1))
  if y>0:candidates.append((u-W,1))
  if y<H-1:candidates.append((u+W,1))
  if i in existing or (not vmasks[0][i] and not vmasks[1][i]):candidates.append((i+(1-l)*N,45))
  for v,w in candidates:
   if masks[v//N][v%N] and v!=goal:continue
   nd=cost+w
   if nd<dist.get(v,1e30):dist[v]=nd;prev[v]=u;heapq.heappush(heap,(nd+heuristic(v),nd,v))
 return None
def track(net,a,c,l,width):
 if a==c:return
 t=k.PCB_TRACK(b);t.SetStart(vec(a));t.SetEnd(vec(c));t.SetWidth(k.FromMM(width));t.SetLayer([k.F_Cu,k.B_Cu][l]);t.SetNetCode(b.FindNet(net).GetNetCode());b.Add(t)
 segments.append((net,a,c,l,width))
def emit(net,path,start,end,width):
 track(net,(start[1],start[2]),point(path[0]),start[3],width)
 run=path[0];last=path[0];direction=None
 for u in path[1:]:
  if u//N!=last//N:
   track(net,point(run),point(last),last//N,width)
   p=point(u)
   existing=next(((x,y) for vn,x,y in vias if vn==net and cell(x,y)==u%N),None)
   if existing:
    track(net,point(last),existing,last//N,width);track(net,existing,p,u//N,width)
   else:
    via=k.PCB_VIA(b);via.SetPosition(vec(p));via.SetWidth(k.FromMM(.6));via.SetDrill(k.FromMM(.3));via.SetViaType(k.VIATYPE_THROUGH);via.SetLayerPair(k.F_Cu,k.B_Cu);via.SetNetCode(b.FindNet(net).GetNetCode());b.Add(via);vias.append((net,*p))
   run=u;direction=None
  else:
   d=u-last
   if direction is not None and d!=direction:track(net,point(run),point(last),last//N,width);run=last
   direction=d
  last=u
 track(net,point(run),point(last),last//N,width)
 track(net,point(path[-1]),(end[1],end[2]),end[3],width)
# Explicit SHT40 fan-out keeps every trace outside its exposed central cavity.
for net,path in [('SDA',[(4.3,51.6),(2,51.6)]),('SCL',[(4.3,52.4),(3.4,52.4),(3.4,53.5),(2,53.5)]),('SENSOR3V3',[(5.7,52.4),(8,52.4)]),('GND',[(5.7,51.6),(6.6,51.6),(6.6,50.5),(8,50.5)])]:
 for a,c in zip(path,path[1:]):track(net,a,c,0,.25)
 p=path[-1];via=k.PCB_VIA(b);via.SetPosition(vec(p));via.SetWidth(k.FromMM(.6));via.SetDrill(k.FromMM(.3));via.SetViaType(k.VIATYPE_THROUGH);via.SetLayerPair(k.F_Cu,k.B_Cu);via.SetNetCode(b.FindNet(net).GetNetCode());b.Add(via);vias.append((net,*p))
 groups[net]=[(cell(*p)+N,*p,1) if abs(s[1]-path[0][0])<.01 and abs(s[2]-path[0][1])<.01 else s for s in groups[net]]
# Fan out close-pitch sockets and ICs before routing longer shared buses.
for f in b.GetFootprints():
 ref=f.GetReference()
 if ref not in ['J3','J4','J5','U1','Q1']:continue
 cx,cy=xy(f.GetPosition())
 for pad in f.Pads():
  net=pad.GetNetname()
  if not net:continue
  a=xy(pad.GetPosition());layer=0 if pad.IsOnLayer(k.F_Cu) else 1
  if ref in ['J3','J4']:p=(a[0],a[1]+2)
  elif ref=='J5':p=(a[0],a[1]+(2.75 if a[1]>cy else -2.75))
  else:p=(a[0]+(1.55 if a[0]>cx else -1.55),a[1])
  track(net,a,p,layer,.25)
  via=k.PCB_VIA(b);via.SetPosition(vec(p));via.SetWidth(k.FromMM(.6));via.SetDrill(k.FromMM(.3));via.SetViaType(k.VIATYPE_THROUGH);via.SetLayerPair(k.F_Cu,k.B_Cu);via.SetNetCode(b.FindNet(net).GetNetCode());b.Add(via);vias.append((net,*p))
  groups[net]=[(cell(*p)+(1-layer)*N,*p,1-layer) if abs(s[1]-a[0])<.01 and abs(s[2]-a[1])<.01 else s for s in groups[net]]
for net in ['SDA','SCL','PM_RESET','PM_SET','USB5V','FUSED5V','SENSOR5V','SENSOR3V3','GND']:
 width=.5 if net in ['USB5V','FUSED5V','SENSOR5V'] else .25
 todo=list(groups[net]);done=[todo.pop(0)];masks=obstacles(net,width);vmasks=obstacles(net,.6)
 while todo:
  _,si,ti=min((abs(s[1]-t[1])+abs(s[2]-t[2])+(s[3]!=t[3])*4.5,i,j) for i,s in enumerate(done) for j,t in enumerate(todo))
  # A via already on this net can be reused, but a second drilled hole must not
  # be placed within the fabrication hole-to-hole clearance.
  vmasks=obstacles(net,.6)
  # No unfilled via-in-pad: keep newly drilled vias clear of all SMT/THT lands.
  for pn,lo,hi,ls in pads:
   for m in vmasks:rectangle(m,lo,hi,.51)
  for vn,x,y in vias:
   for m in vmasks:disk(m,x,y,.56)
  existing={cell(x,y) for vn,x,y in vias if vn==net}
  start=done[si];end=todo[ti];path=route(start[0],end[0],masks,vmasks,existing)
  if path is None:
   k.SaveBoard(str(ROOT/'tmp/route-partial.kicad_pcb'),b)
   for s in [start,end]:
    print('Blocked at',s,[m[s[0]%N] for m in masks],[m[s[0]%N] for m in vmasks],flush=True)
    for pn,lo,hi,ls in pads:
     if pn!=net and lo[0]-.4<s[1]<hi[0]+.4 and lo[1]-.4<s[2]<hi[1]+.4:print('Nearby pad',pn,lo,hi,ls,flush=True)
   raise RuntimeError('Cannot route '+net+' '+str(start)+' -> '+str(end))
  emit(net,path,start,end,width);done.append(todo.pop(ti))
 print(net,'connected',len(done),'pads',flush=True)
removed=0
for item in list(b.GetTracks()):
 if not isinstance(item,k.PCB_VIA):continue
 p=xy(item.GetPosition());layers=set()
 for t in b.GetTracks():
  if isinstance(t,k.PCB_VIA) or t.GetNetCode()!=item.GetNetCode():continue
  if any(math.hypot(q[0]-p[0],q[1]-p[1])<.001 for q in [xy(t.GetStart()),xy(t.GetEnd())]):layers.add(t.GetLayer())
 for f in b.GetFootprints():
  for pad in f.Pads():
   if pad.GetNetCode()!=item.GetNetCode():continue
   box=pad.GetBoundingBox();lo=xy(box.GetOrigin());hi=xy(box.GetEnd())
   if lo[0]<=p[0]<=hi[0] and lo[1]<=p[1]<=hi[1]:
    for l in [k.F_Cu,k.B_Cu]:
     if pad.IsOnLayer(l):layers.add(l)
 if len(layers)==1:b.Remove(item);removed+=1
print('Removed',removed,'unused layer transitions')
k.SaveBoard(str(FILE),b)
(ROOT/'hardware/routing-report.json').write_text(json.dumps(dict(tracks=len(segments),vias=len(vias),grid_mm=G,clearance_mm=CLR),indent=2)+'\n')
print('Saved',len(segments),'tracks and',len(vias),'vias')
