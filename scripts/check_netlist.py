#!/usr/bin/env python3
"""Verify every schematic connected pad against the independently exported PCB."""
from pathlib import Path
import json, xml.etree.ElementTree as ET
import pcbnew
ROOT=Path(__file__).resolve().parents[1]
board=pcbnew.LoadBoard(str(ROOT/'hardware/airmon.kicad_pcb'))
actual={(fp.GetReference(),pad.GetNumber()):pad.GetNetname() for fp in board.GetFootprints() for pad in fp.Pads() if pad.GetNumber() and pad.GetNetname()}
tree=ET.parse(ROOT/'hardware/exports/netlist.xml')
expected={}
for net in tree.findall('./nets/net'):
 for node in net.findall('node'):
  if not net.attrib['name'].startswith('unconnected-'):
   # Single root sheet: KiCad prefixes local-label names with '/'.
   expected[(node.attrib['ref'],node.attrib['pin'])]=net.attrib['name'].removeprefix('/')
assert expected==actual,dict(missing={str(k):v for k,v in expected.items() if actual.get(k)!=v},extra={str(k):v for k,v in actual.items() if expected.get(k)!=v})
report=dict(passed=True,connected_pads=len(actual),nets=len(set(actual.values())),tracks=sum(isinstance(t,pcbnew.PCB_TRACK) and not isinstance(t,pcbnew.PCB_VIA) for t in board.GetTracks()),vias=sum(isinstance(t,pcbnew.PCB_VIA) for t in board.GetTracks()))
(ROOT/'hardware/exports/netlist-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(report)
