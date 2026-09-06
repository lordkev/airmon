#!/usr/bin/env python3
"""Intersect source-CAD envelopes; report actual overlapping volume, in mm³.

This checks the represented geometry only. Cable bend/strain and vendor-board
revision tolerances still need a physical fit test.
"""
from pathlib import Path
import itertools,json,sys
import trimesh
ROOT=Path(__file__).resolve().parents[1]
def mesh(path):
 m=trimesh.load_mesh(path);m.merge_vertices(digits_vertex=5);m.update_faces(m.nondegenerate_faces(height=1e-12));m.update_faces(m.unique_faces());m.remove_unreferenced_vertices()
 assert m.is_watertight and m.is_winding_consistent,path
 return m
shallow='--shallow' in sys.argv
folder=ROOT/('tmp/clearance-shallow' if shallow else 'tmp/clearance')
items={p.stem.removeprefix('check_'):mesh(p) for p in sorted(folder.glob('check_*.stl'))}
assert set(items)==({'display','carrier','rear','mount'} if shallow else {'display','carrier','pm','scd','sgp','rear','mount'}),'Missing or unexpected clearance envelopes'
items['bezel']=mesh(ROOT/'enclosure/stl/bezel.stl')
actual=ROOT/'tmp/clearance/carrier-kicad.stl'
if actual.exists():items['carrier']=mesh(actual)
rows=[]
for (an,a),(bn,b) in itertools.combinations(items.items(),2):
 intersection=trimesh.boolean.intersection([a,b],engine='manifold')
 volume=abs(float(intersection.volume)) if len(intersection.faces) else 0.0
 rows.append(dict(a=an,b=bn,overlap_mm3=round(volume,5),passed=volume<.01))
 print(an,bn,round(volume,4),flush=True)
(ROOT/('enclosure/clearance-validation-shallow.json' if shallow else 'enclosure/clearance-validation.json')).write_text(json.dumps(dict(note='Native KiCad STEP carrier when available; simplified vendor display and sensor envelopes. Cables, fasteners, and measured board revision require physical validation.',carrier_model='KiCad STEP' if actual.exists() else 'simplified envelope',checks=rows),indent=2)+'\n')
assert all(r['passed'] for r in rows),'CAD interference found; inspect clearance-validation.json'
