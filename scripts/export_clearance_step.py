#!/usr/bin/env python3
"""Tessellate the actual KiCad STEP into the enclosure coordinate system."""
from pathlib import Path
import cadquery as cq
import numpy as np
import trimesh
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'tmp/clearance';out.mkdir(parents=True,exist_ok=True)
shape=cq.importers.importStep(str(ROOT/'hardware/exports/airmon.step')).val()
path=out/'carrier-kicad.stl'
cq.exporters.export(shape,str(path),tolerance=.02,angularTolerance=.1)
mesh=trimesh.load_mesh(path)
mesh.apply_transform(np.array([[1,0,0,15],[0,-1,0,15],[0,0,1,22],[0,0,0,1]],dtype=float))
mesh.export(path)
print('Native carrier enclosure bounds:',mesh.bounds)
