#!/usr/bin/env python3
"""Slice all STLs with an isolated Bambu Studio A1/PLA reference profile.

Validation only: outputs under tmp/ are not a universal printer release.
"""
from pathlib import Path
import json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
APP=Path(os.environ.get('BAMBU_APP','/Applications/BambuStudio.app'))
profiles=APP/'Contents/Resources/profiles/BBL'
index={p.stem:p for p in profiles.rglob('*.json')}
def resolve(name):
 d=json.loads(index[name].read_text());result={}
 if d.get('inherits'):result.update(resolve(d['inherits']))
 for inc in d.get('include',[]):result.update(resolve(inc))
 result.update(d);result.pop('inherits',None);result.pop('include',None)
 return result
out=ROOT/'tmp/slicing';out.mkdir(parents=True,exist_ok=True)
machine=resolve('Bambu Lab A1 0.4 nozzle')
process=resolve('0.20mm Standard @BBL A1')
process.update(wall_loops='4',top_shell_layers='5',bottom_shell_layers='5',sparse_infill_density='15%',sparse_infill_pattern='gyroid',enable_support='0',layer_height='0.2',initial_layer_print_height='0.2')
filament=resolve('Generic PLA @BBL A1')
for name,d in [('machine',machine),('process',process),('filament',filament)]:
 (out/f'{name}.json').write_text(json.dumps(d,indent=2))
reports=[]
for path in sorted((ROOT/'enclosure/stl').glob('*.stl')):
 dest=out/path.stem;dest.mkdir(exist_ok=True)
 args=[str(APP/'Contents/MacOS/BambuStudio'),'--datadir',str(ROOT/'.tools/bambu-config'),'--load-settings',str(out/'machine.json')+';'+str(out/'process.json'),'--load-filaments',str(out/'filament.json'),'--arrange','1','--orient','0','--slice','0','--export-3mf','sliced.3mf','--outputdir',str(dest),str(path)]
 with (dest/'slicer.log').open('w') as log:subprocess.run(args,check=True,stdout=log,stderr=subprocess.STDOUT,cwd=dest)
 r=json.loads((dest/'result.json').read_text());assert r['return_code']==0,r
 assert r['wall_loops']==4 and abs(r['layer_height']-.2)<1e-5 and r['sparse_infill_density']==15,r
 g=(dest/'plate_1.gcode').read_text()
 assert '; enable_support = 0' in g and '; nozzle_diameter = 0.4' in g
 reports.append(dict(file=str(path.relative_to(ROOT)),return_code=0,layer_height_mm=.2,wall_loops=4,infill_percent=15,supports=False,plates=r['sliced_plates']))
 print('Sliced:',path.name,flush=True)
(ROOT/'enclosure/slicing-validation.json').write_text(json.dumps(dict(slicer='Bambu Studio 02.08.02.61',profile='A1 0.4 mm / Generic PLA; reference validation only',checks=reports),indent=2)+'\n')
