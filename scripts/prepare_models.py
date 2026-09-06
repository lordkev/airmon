#!/usr/bin/env python3
"""Pin standard KiCad STEP models locally and make custom sensor envelopes."""
from pathlib import Path
import re,shutil,os
import cadquery as cq
ROOT=Path(__file__).resolve().parents[1];HW=ROOT/'hardware';MODELS=HW/'models';MODELS.mkdir(exist_ok=True)
SHARE=Path(os.environ.get('KICAD_SHARE',ROOT/'.tools/KiCad/KiCad.app/Contents/SharedSupport'))
for fp in (HW/'AirMon.pretty').glob('*.kicad_mod'):
 text=fp.read_text()
 for rel in re.findall(r'\$\{KICAD10_3DMODEL_DIR\}/([^"\n]+)',text):
  src=SHARE/'3dmodels'/rel
  if src.exists():
   shutil.copyfile(src,MODELS/src.name);text=text.replace('${KICAD10_3DMODEL_DIR}/'+rel,'${KIPRJMOD}/models/'+src.name)
 fp.write_text(text)
# SHT40's component body; small open top cavity and contacts are represented.
sht=cq.Workplane('XY').box(1.5,1.5,.55,centered=(True,True,False)).faces('>Z').workplane().rect(.7,.7).cutBlind(-.10)
name='Sensirion_DFN-4_1.5x1.5mm_P0.8mm_SHT4x_NoCentralPad.step'
cq.exporters.export(sht,str(MODELS/name))
for fp in (HW/'AirMon.pretty').glob('Sensirion*.kicad_mod'):
 text=fp.read_text();text=re.sub(r'\$\{KICAD10_3DMODEL_DIR\}/[^"\n]+','${KIPRJMOD}/models/'+name,text);fp.write_text(text)
# Socket paired with Adafruit 4505: mating height remains a physical fit check.
socket=cq.Workplane('XY').box(10.54,5,2,centered=(True,True,False))
socket=socket.faces('>Z').workplane().rect(6.2,2.9).cutBlind(-1.5)
cq.exporters.export(socket,str(MODELS/'PMSA003I_SuppliedSocket.step'))
fp=HW/'AirMon.pretty/PMSA003I_SuppliedSocket.kicad_mod';text=fp.read_text()
if '(model ' not in text:
 text=text.rstrip()[:-1]+'\n(model "${KIPRJMOD}/models/PMSA003I_SuppliedSocket.step" (offset (xyz 0 0 0)) (scale (xyz 1 1 1)) (rotate (xyz 0 0 0))))\n'
fp.write_text(text)
print('Prepared local STEP models')
