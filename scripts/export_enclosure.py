#!/usr/bin/env python3
"""Regenerate printable meshes, clearance envelopes, and documentation renders."""
from pathlib import Path
import os, shutil, subprocess
ROOT=Path(__file__).resolve().parents[1]
CLI=os.environ.get('OPENSCAD') or shutil.which('openscad') or 'openscad'
source=ROOT/'enclosure/airmon.scad'
def run(part, output, *extra):
 output.parent.mkdir(parents=True,exist_ok=True)
 subprocess.run([CLI,'-D',f'part="{part}"','-o',str(output),*extra,str(source)],check=True)
for part in ['bezel','rear_full','rear_shallow','sensor_mount_full','sensor_mount_shallow','stand','fit_test']:
 run(part,ROOT/f'enclosure/stl/{part}.stl')
for part in ['display','carrier','pm','scd','sgp','rear','mount']:
 run('check_'+part,ROOT/f'tmp/clearance/check_{part}.stl')
for part in ['display','carrier','rear','mount']:
 run('check_'+part,ROOT/f'tmp/clearance-shallow/check_{part}.stl','-D','full=false')
for part,filename,camera in [
 ('assembly','assembled-unit.png','0,0,0,70,0,30,260'),
 ('exploded','exploded-assembly.png','0,0,0,65,0,30,400'),
 ('pcb','assembled-pcb-with-pm.png','0,0,0,40,0,30,180')]:
 run(part,ROOT/'docs/images'/filename,'--imgsize=1600,1200','--colorscheme=Tomorrow','--camera='+camera,'--viewall','--autocenter')
print('Enclosure exports complete; run validate_meshes.py and check_clearances.py next.')
