#!/usr/bin/env python3
"""Create a labelled, native schematic using the carrier's component contract."""
from pathlib import Path
import json,uuid
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'hardware'
parts=json.loads((OUT/'components.json').read_text())
def uid(x):return str(uuid.uuid5(uuid.NAMESPACE_URL,'airmon/schematic/'+x))
def q(x):return json.dumps(str(x))
def effects(size=1.27):return f'(effects (font (size {size} {size})))'
def prop(n,val,x,y,hide=False):return f'(property {q(n)} {q(val)} (at {x} {y} 0) (effects (font (size 1.27 1.27)) {"(hide yes)" if hide else ""}))'
root=uid('root');symbols=[];instances=[];wires=[]
pin_names={'J1':{'1':'USB 5V','2':'GND'},'J2':{'1':'GPIO19 SDA','2':'GPIO18 SCL'},'U1':{'1':'VIN','2':'GND','3':'EN','4':'NC','5':'VOUT'},'U2':{'1':'SDA','2':'SCL','3':'VDD','4':'VSS'},'Q1':{'1':'G','2':'S','3':'D'},'D1':{'1':'K','2':'A'},'J5':{'1':'5V','2':'5V','3':'GND','4':'GND','5':'RESET','6':'NC','7':'SCL','8':'NC','9':'SDA','10':'SET'}}
order=['J1','F1','Q1','D1','C1','U1','C2','C3','J2','R1','R2','J3','J5','R3','R4','J4','U2','C4','TP1','TP2','TP3','TP4','TP5','H1','H2']
for i,ref in enumerate(order):
 p=next(p for p in parts if p['reference']==ref)
 pins=dict(pin_names.get(ref,{n:n for n in p['nets']}))
 if ref in ['J3','J4']:pins={'1':'GND','2':'3V3','3':'SDA','4':'SCL'}
 n=max(1,len(pins));height=(n+1)*2.54;x=45.72+(i%4)*96.52;y=25.4+(i//4)*35.56
 libid='AirMon:'+ref;name=ref
 s=[f'(symbol {q(name)} (pin_names (offset .508)) (in_bom yes) (on_board yes)',prop('Reference',ref,0,2.54),prop('Value',p['value'],0,5.08),prop('Footprint','AirMon:'+p['footprint'],0,0,True),
 f'(symbol {q(name+"_0_1")} (rectangle (start 0 0) (end 25.4 {-height}) (stroke (width .254) (type default)) (fill (type background))))',f'(symbol {q(name+"_1_1")}']
 for j,(num,pname) in enumerate(pins.items()):
  py=-(j+1)*2.54
  typ='passive'
  if ref=='J1':typ='power_out'
  if ref=='U1' and num in ['1','2']:typ='power_in'
  if ref=='U1' and num=='5':typ='power_out'
  # Q1 source is the protected 5 V power output; it supplies U1 VIN.
  if ref=='Q1' and num=='2':typ='power_out'
  if ref=='U2':typ={'1':'bidirectional','2':'input','3':'power_in','4':'power_in'}[num]
  if pname=='NC':typ='no_connect'
  s.append(f'(pin {typ} line (at -5.08 {py} 0) (length 5.08) (name {q(pname)} {effects(1.016)}) (number {q(num)} {effects(1.016)}))')
  px=x-5.08;sy=y-py
  if num in p['nets']:
   lx=px-10.16
   wires.append(f'(wire (pts (xy {lx} {sy}) (xy {px} {sy})) (stroke (width 0) (type default)) (uuid {q(uid(ref+num+"wire"))}))')
   wires.append(f'(label {q(p["nets"][num])} (at {lx} {sy} 0) (effects (font (size 1.016 1.016)) (justify left bottom)) (uuid {q(uid(ref+num+"label"))}))')
  else:wires.append(f'(no_connect (at {px} {sy}) (uuid {q(uid(ref+num+"nc"))}))')
 s+=['))'];sym='\n'.join(s);symbols.append(sym)
 fpuid=uid(ref)
 instances.append(f'''(symbol (lib_id {q(libid)}) (at {x} {y} 0) (unit 1) (in_bom yes) (on_board yes) (dnp no) (uuid {q(fpuid)})
 {prop('Reference',ref,x+12.7,y-5.08)} {prop('Value',p['value'],x+12.7,y-2.54)} {prop('Footprint','AirMon:'+p['footprint'],x,y,True)}
 (instances (project "airmon" (path "/{root}" (reference {q(ref)}) (unit 1)))))''')
lib='(kicad_symbol_lib (version 20241209) (generator "kicad_symbol_editor")\n'+'\n'.join(symbols)+')\n'
(OUT/'AirMon.kicad_sym').write_text(lib)
embedded=[]
for ref,s in zip(order,symbols):embedded.append(s.replace('(symbol '+q(ref),'(symbol '+q('AirMon:'+ref),1))
sch=f'''(kicad_sch (version 20250114) (generator "eeschema") (uuid "{root}") (paper "A3")
 (title_block (title "AirMon sensor carrier — prototype r0.1") (date "2026-09-06") (rev "0.1") (company "AirMon") (comment 1 "USB only. GPIO22 LOW enables 5V. GPIO19 SDA / GPIO18 SCL. microSD EMPTY."))
 (lib_symbols {''.join(embedded)})
 {''.join(wires)} {''.join(instances)}
 (sheet_instances (path "/" (page "1"))) (embedded_fonts no))\n'''
(OUT/'airmon.kicad_sch').write_text(sch)
(OUT/'sym-lib-table').write_text('(sym_lib_table (version 7) (lib (name "AirMon") (type "KiCad") (uri "${KIPRJMOD}/AirMon.kicad_sym") (options "") (descr "Carrier schematic symbols")))\n')
print('Wrote native schematic and local symbols')
