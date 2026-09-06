#!/usr/bin/env python3
"""Audit the digital prototype bundle; physical release gates remain explicit."""
from pathlib import Path
import csv,hashlib,json,re,zipfile
ROOT=Path(__file__).resolve().parents[1]
required=['PROMPT.md','README.md','THIRD_PARTY.md','hardware/airmon.kicad_sch','hardware/airmon.kicad_pcb','hardware/BOM.xlsx','hardware/assembly-bom.csv','hardware/purchasing-bom.csv','hardware/references/manifest.json','hardware/exports/airmon.step','hardware/exports/schematic.pdf','hardware/exports/fabrication-drawing.pdf','hardware/exports/assembly-top.pdf','hardware/exports/assembly-bottom.pdf','hardware/exports/placement-smt.csv','hardware/exports/placement-all.csv','hardware/exports/footprint-fit-1to1.pdf','enclosure/airmon.scad','docs/ASSEMBLY.md','docs/HARDWARE.md','docs/FIRMWARE.md','docs/API.md','docs/USE.md','docs/HOME_ASSISTANT.md','docs/DEVELOPMENT.md','docs/VALIDATION.md','docs/web-validation.json']
images=['assembled-unit.png','assembled-pcb-underside.png','assembled-pcb-top.png','assembled-pcb-with-pm.png','exploded-assembly.png']
required += ['docs/images/'+i for i in images]
for name in required:assert (ROOT/name).is_file() and (ROOT/name).stat().st_size>0,name
for doc in [ROOT/'README.md',ROOT/'PROMPT.md',ROOT/'THIRD_PARTY.md',*sorted((ROOT/'docs').glob('*.md')),ROOT/'hardware/README.md',ROOT/'release/README.md']:
 for link in re.findall(r'\]\(([^)]+)\)',doc.read_text()):
  if re.match(r'^[a-z]+:',link) or link.startswith('#'):continue
  target=link.split('#')[0]
  assert (doc.parent/target).exists(),(doc.relative_to(ROOT),target)
for image in images:assert any(image in p.read_text() for p in [ROOT/'README.md',*sorted((ROOT/'docs').glob('*.md'))]),image
with zipfile.ZipFile(ROOT/'hardware/exports/airmon-gerbers.zip') as z:
 names=set(z.namelist());expected={'airmon-'+layer+ext for layer,ext in [('F_Cu','.gtl'),('B_Cu','.gbl'),('F_Paste','.gtp'),('B_Paste','.gbp'),('F_Silkscreen','.gto'),('B_Silkscreen','.gbo'),('F_Mask','.gts'),('B_Mask','.gbs'),('Edge_Cuts','.gm1'),('PTH','.drl'),('NPTH','.drl')]}
 assert expected<=names,(expected-names)
 for name in expected:assert z.read(name)==(ROOT/'hardware/exports/gerbers'/name).read_bytes(),name
for name in ['erc','drc']:
 s=(ROOT/f'hardware/exports/{name}.txt').read_text()
 assert (re.search(r'ERC messages: 0\s+Errors 0\s+Warnings 0',s) if name=='erc' else 'Found 0 DRC violations' in s and 'Found 0 unconnected pads' in s),s[:200]
assert json.loads((ROOT/'hardware/exports/netlist-validation.json').read_text())['passed']
for name,count in [('mesh-validation',7),('clearance-validation',28),('clearance-validation-shallow',10),('slicing-validation',7)]:
 r=json.loads((ROOT/f'enclosure/{name}.json').read_text());rows=r if isinstance(r,list) else r['checks'];assert len(rows)==count,(name,len(rows))
 for row in rows:
  if 'passed' in row:assert row['passed']
  if 'watertight' in row:assert row['watertight'] and (ROOT/row['file']).is_file()
  if 'return_code' in row:assert row['return_code']==0 and not row['supports']
manifest=json.loads((ROOT/'release/firmware/manifest.json').read_text())
for row in manifest['files']:
 p=ROOT/'release/firmware'/row['file'];assert p.stat().st_size==row['size_bytes'];assert hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256']
for path,digest in manifest['source_sha256'].items():assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest,(path,'source changed after firmware packaging')
assert manifest['ota_free_bytes']>0
with zipfile.ZipFile(ROOT/'hardware/BOM.xlsx') as z:assert 'xl/workbook.xml' in z.namelist()
for name in ['assembly-bom','purchasing-bom']:
 with (ROOT/f'hardware/{name}.csv').open() as f:assert len(list(csv.reader(f)))>10
assert json.loads((ROOT/'docs/web-validation.json').read_text())['passed']
assert 'No physical prototype has been tested' in (ROOT/'docs/VALIDATION.md').read_text()
report=dict(status='digital prototype audit passed; physical gates pending',required_files=len(required),render_files=images,gerber_drill_files=len(expected),firmware_images=len(manifest['files']),source_files_hashed=len(manifest['source_sha256']),physical_validation=False)
(ROOT/'release/audit.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
