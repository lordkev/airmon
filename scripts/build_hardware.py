#!/usr/bin/env python3
"""Generate native KiCad sources from the reviewed component/net specification.

Run with KiCad's Python (pcbnew required). Normal KiCad editing is supported;
regeneration replaces generated files, so back up manual edits first.
"""
from pathlib import Path
import os, json, math, shutil, uuid
import pcbnew as k

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'hardware'
LIB=OUT/'AirMon.pretty'
LIB.mkdir(parents=True,exist_ok=True)
KICAD=Path(os.environ.get('KICAD_SHARE',ROOT/'.tools/KiCad/KiCad.app/Contents/SharedSupport'))
board=k.BOARD()
board.GetDesignSettings().SetBoardThickness(k.FromMM(1.6))
netnames=['GND','USB5V','FUSED5V','SENSOR5V','SENSOR3V3','SDA','SCL','PM_RESET','PM_SET']
nets={}
for name in netnames:
 n=k.NETINFO_ITEM(board,name);board.Add(n);nets[name]=n
def v(x,y): return k.VECTOR2I(k.FromMM(x),k.FromMM(y))
def uid(s): return str(uuid.uuid5(uuid.NAMESPACE_URL,'https://github.com/lordkev/airmon/'+s))
def copy_fp(lib,name):
 src=KICAD/'footprints'/f'{lib}.pretty'/f'{name}.kicad_mod'
 dst=LIB/(name+'.kicad_mod')
 if not dst.exists(): shutil.copyfile(src,dst)
 return name
def custom_socket():
 lines=['(footprint "PMSA003I_SuppliedSocket" (version 20241229) (generator "pcbnew") (layer "F.Cu") (attr smd)',
 '(fp_text reference "J5" (at 0 -4.5) (layer "F.SilkS") (effects (font (size 1 1) (thickness .15))))',
 '(fp_text value "PMSA003I socket" (at 0 4.5) (layer "F.Fab") (effects (font (size 1 1) (thickness .15))))']
 for i in range(5):
  for row in range(2):
   lines.append(f'(pad "{i*2+row+1}" smd rect (at {-2.54+1.27*i:.2f} {[-1.85,1.85][row]}) (size .65 2.6) (layers "F.Cu" "F.Paste" "F.Mask"))')
 for layer,w in [('F.SilkS',.15),('F.Fab',.1),('F.CrtYd',.05)]:
  dx,dy=(5.6,3.8) if layer=='F.CrtYd' else (5.27,3.5)
  lines.append(f'(fp_rect (start {-dx} {-dy}) (end {dx} {dy}) (stroke (width {w}) (type solid)) (fill none) (layer "{layer}"))')
 lines.append('(fp_circle (center -3.5 -3.8) (end -3.3 -3.8) (stroke (width .15) (type solid)) (fill none) (layer "F.SilkS"))')
 lines.append('(model "${KIPRJMOD}/models/PMSA003I_SuppliedSocket.step" (offset (xyz 0 0 0)) (scale (xyz 1 1 1)) (rotate (xyz 0 0 0)))')
 lines.append(')');(LIB/'PMSA003I_SuppliedSocket.kicad_mod').write_text('\n'.join(lines))
custom_socket()
F={
 'R':copy_fp('Resistor_SMD','R_0603_1608Metric'),
 'C':copy_fp('Capacitor_SMD','C_0603_1608Metric'),
 'bulk':copy_fp('Capacitor_SMD','C_1210_3225Metric'),
 'fuse':copy_fp('Fuse','Fuse_1206_3216Metric'),
 'LDO':copy_fp('Package_TO_SOT_SMD','SOT-23-5'),
 'MOS':copy_fp('Package_TO_SOT_SMD','SOT-23'),
 'TVS':copy_fp('Diode_SMD','D_SOD-123F'),
 'SHT':copy_fp('Sensor_Humidity','Sensirion_DFN-4_1.5x1.5mm_P0.8mm_SHT4x_NoCentralPad'),
 'PH':copy_fp('Connector_JST','JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical'),
 'QT':copy_fp('Connector_JST','JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal'),
 'hole':copy_fp('MountingHole','MountingHole_2.2mm_M2'),
 'TP':copy_fp('TestPoint','TestPoint_Pad_D1.0mm'),
 'PM':'PMSA003I_SuppliedSocket'}
# ref, value, footprint, x/y mm, rotation degrees, bottom, pad->net
parts=[
 ('J1','5V POWER', 'PH',26,42,0,True,{'1':'USB5V','2':'GND'}),
 ('J2','SDA SCL','PH',35,42,0,True,{'1':'SDA','2':'SCL'}),
 ('J3','STEMMA QT','QT',29,33,180,True,{'1':'GND','2':'SENSOR3V3','3':'SDA','4':'SCL'}),
 ('J4','STEMMA QT','QT',38,33,180,True,{'1':'GND','2':'SENSOR3V3','3':'SDA','4':'SCL'}),
 ('J5','PMSA003I supplied socket','PM',9.5,5.5,0,False,{'1':'SENSOR5V','2':'SENSOR5V','3':'GND','4':'GND','5':'PM_RESET','7':'SCL','9':'SDA','10':'PM_SET'}),
 ('U1','AP2112K-3.3TRG1','LDO',17,37,0,True,{'1':'SENSOR5V','2':'GND','3':'SENSOR5V','5':'SENSOR3V3'}),
 ('U2','SHT40-AD1B-R2','SHT',5,52,0,False,{'1':'SDA','2':'SCL','3':'SENSOR3V3','4':'GND'}),
 ('Q1','AO3401A','MOS',19,41,0,True,{'1':'GND','2':'SENSOR5V','3':'FUSED5V'}),
 ('F1','1206L100/16','fuse',25,28,0,True,{'1':'USB5V','2':'FUSED5V'}),
 ('D1','SMF5.0A','TVS',12,37,90,True,{'1':'SENSOR5V','2':'GND'}),
 ('C1','47uF 10V X5R','bulk',8,29,0,True,{'1':'SENSOR5V','2':'GND'}),
 ('C2','4.7uF 10V X5R','C',15,33,90,True,{'1':'SENSOR5V','2':'GND'}),
 ('C3','10uF 10V X5R','C',20,33,90,True,{'1':'SENSOR3V3','2':'GND'}),
 ('C4','100nF 10V X7R','C',8,49,0,False,{'1':'SENSOR3V3','2':'GND'}),
 ('R1','4.7k','R',15,28,90,True,{'1':'SENSOR3V3','2':'SDA'}),
 ('R2','4.7k','R',19,28,90,True,{'1':'SENSOR3V3','2':'SCL'}),
 ('R3','10k','R',7,15,0,True,{'1':'SENSOR3V3','2':'PM_RESET'}),
 ('R4','10k','R',13,15,0,True,{'1':'SENSOR3V3','2':'PM_SET'}),
 ('H1','M2 mount','hole',17,45,0,False,{}),
 ('H2','M2 mount','hole',39,46,0,False,{}),
]
for i,(name,x) in enumerate([('GND',12),('SENSOR5V',17),('SENSOR3V3',22),('SDA',27),('SCL',32)]):
 parts.append((f'TP{i+1}',name,'TP',x,21,0,True,{'1':name}))
fps={}
for ref,value,fp,x,y,angle,bottom,pinmap in parts:
 f=k.FootprintLoad(str(LIB),F[fp]);assert f, F[fp]
 f.SetReference(ref);f.SetValue(value);f.SetFPID(k.LIB_ID('AirMon',F[fp]))
 sch_root=str(uuid.uuid5(uuid.NAMESPACE_URL,'airmon/schematic/root'))
 sch_ref=str(uuid.uuid5(uuid.NAMESPACE_URL,'airmon/schematic/'+ref))
 f.SetPath(k.KIID_PATH('/'+sch_root+'/'+sch_ref))
 board.Add(f)
 f.SetPosition(v(x,y));f.SetOrientationDegrees(angle)
 if bottom:f.Flip(v(x,y),False)
 for pad in f.Pads():
  if pad.GetNumber() in pinmap:pad.SetNet(nets[pinmap[pad.GetNumber()]])
 f.Reference().SetTextSize(v(.8,.8));f.Reference().SetTextThickness(k.FromMM(.12))
 if ref=='Q1':f.Reference().SetPosition(v(19.6,45.5))
 f.Value().SetVisible(False)
 fps[ref]=f
def edge(a,b):
 s=k.PCB_SHAPE();s.SetShape(k.SHAPE_T_SEGMENT);s.SetStart(v(*a));s.SetEnd(v(*b));s.SetLayer(k.Edge_Cuts);s.SetWidth(k.FromMM(.05));board.Add(s)
# Narrow neck limits heat conduction from the main PCB to the SHT40 tab.
outline=[(0,0),(42,0),(42,48),(10,48),(10,55),(0,55),(0,48),(3,48),(3,43),(0,43)]
for a,b in zip(outline,outline[1:]+outline[:1]):edge(a,b)
for text,x,y,size in [('AirMon r0.1',26,25,1.1),('PM OPTIONAL',25,29,.8),('TEMP',5,54,.8)]:
 t=k.PCB_TEXT(board);t.SetText(text);t.SetPosition(v(x,y));t.SetTextSize(v(size,size));t.SetTextThickness(k.FromMM(.13));t.SetLayer(k.F_SilkS);board.Add(t)
board.SetAuxOrigin(v(0,0)) if hasattr(board,'SetAuxOrigin') else board.GetDesignSettings().SetAuxOrigin(v(0,0))
k.SaveBoard(str(OUT/'airmon.kicad_pcb'),board)
(OUT/'fp-lib-table').write_text('(fp_lib_table (version 7) (lib (name "AirMon") (type "KiCad") (uri "${KIPRJMOD}/AirMon.pretty") (options "") (descr "Pinned carrier footprints")))\n')
(OUT/'components.json').write_text(json.dumps([dict(reference=p[0],value=p[1],footprint=F[p[2]],x=p[3],y=p[4],rotation=p[5],side='bottom' if p[6] else 'top',nets=p[7]) for p in parts],indent=2)+'\n')
print('Wrote board and component specification:',len(parts),'parts')
