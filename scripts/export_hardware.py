#!/usr/bin/env python3
"""Export fabrication and assembly files with KiCad 10's supported CLI."""
import os,shutil,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];HW=ROOT/'hardware';OUT=HW/'exports';GER=OUT/'gerbers'
CLI=os.environ.get('KICAD_CLI') or shutil.which('kicad-cli') or str(ROOT/'.tools/KiCad/KiCad.app/Contents/MacOS/kicad-cli')
OUT.mkdir(exist_ok=True);GER.mkdir(exist_ok=True)
pcb=str(HW/'airmon.kicad_pcb');sch=str(HW/'airmon.kicad_sch')
def run(*args):subprocess.run([CLI,*args],check=True)
run('sch','erc','--exit-code-violations','-o',str(OUT/'erc.txt'),sch)
run('pcb','drc','--exit-code-violations','-o',str(OUT/'drc.txt'),pcb)
run('sch','export','netlist','--format','kicadxml','-o',str(OUT/'netlist.xml'),sch)
run('sch','export','pdf','-o',str(OUT/'schematic.pdf'),sch)
run('pcb','export','gerbers','--layers','F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts','-o',str(GER)+os.sep,pcb)
run('pcb','export','drill','--format','excellon','--excellon-units','mm','--excellon-separate-th','--generate-map','--map-format','svg','--generate-report','--report-path',str(OUT/'drill-report.txt'),'-o',str(GER)+os.sep,pcb)
run('pcb','export','pos','--format','csv','--units','mm','--side','both','--use-drill-file-origin','--smd-only','-o',str(OUT/'placement-smt.csv'),pcb)
run('pcb','export','pos','--format','csv','--units','mm','--side','both','--use-drill-file-origin','-o',str(OUT/'placement-all.csv'),pcb)
for layer,name in [('F.Fab','assembly-top.pdf'),('B.Fab','assembly-bottom.pdf')]:
 mirror=['--mirror'] if layer=='B.Fab' else []
 run('pcb','export','pdf',*mirror,'--mode-single','--layers',layer+',Edge.Cuts','--sketch-pads-on-fab-layers','--scale','3','-o',str(OUT/name),pcb)
run('pcb','export','pdf','--mode-single','--layers','Edge.Cuts,F.Fab','--sketch-pads-on-fab-layers','--scale','1','-o',str(OUT/'footprint-fit-1to1.pdf'),pcb)
run('pcb','export','step','--force','--drill-origin','-o',str(OUT/'airmon.step'),pcb)
with zipfile.ZipFile(OUT/'airmon-gerbers.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in sorted(GER.iterdir()):
  if p.suffix.lower() in ['.gbr','.gbrjob','.drl','.gtl','.gbl','.gtp','.gbp','.gto','.gbo','.gts','.gbs','.gm1']:z.write(p,p.name)
with zipfile.ZipFile(OUT/'airmon-gerbers.zip') as z:
 assert len(z.namelist()) >= 11 and 'airmon-F_Cu.gtl' in z.namelist() and 'airmon-B_Cu.gbl' in z.namelist(), 'Incomplete fabrication ZIP'
print('Exported fabrication and assembly package')
