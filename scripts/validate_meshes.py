#!/usr/bin/env python3
"""Validate STL topology; weld export-roundoff vertices at 0.00001 mm precision.

No holes are filled and no geometry is invented. Failed topology is fatal.
"""
from pathlib import Path
import json
import trimesh
ROOT=Path(__file__).resolve().parents[1]
results=[]
for path in sorted((ROOT/'enclosure/stl').glob('*.stl')):
 m=trimesh.load_mesh(path);before=float(m.volume);faces=len(m.faces)
 m.merge_vertices(digits_vertex=5)
 m.update_faces(m.nondegenerate_faces(height=1e-12));m.update_faces(m.unique_faces());m.remove_unreferenced_vertices()
 assert m.is_watertight and m.is_winding_consistent,path
 assert abs(float(m.volume)-before)<.01,(path,'volume changed')
 bodies=len(m.split());assert bodies==(2 if path.stem=='fit_test' else 1),(path,bodies)
 assert m.bounds[0].min()>-.001,(path,'below print bed')
 m.export(path,file_type='stl')
 results.append(dict(file=str(path.relative_to(ROOT)),watertight=True,consistent_winding=True,bodies=bodies,dimensions_mm=m.extents.round(4).tolist(),volume_mm3=round(float(m.volume),3),removed_roundoff_faces=faces-len(m.faces)))
(ROOT/'enclosure/mesh-validation.json').write_text(json.dumps(results,indent=2)+'\n')
print('PASS:',len(results),'STLs; watertight, consistent normals, expected bodies and print-bed placement')
