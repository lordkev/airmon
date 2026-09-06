#!/usr/bin/env python3
"""Dimensioned prototype carrier drawing, independent of KiCad plot scaling."""
from pathlib import Path
from reportlab.pdfgen import canvas
from reportlab.lib.pagesizes import A4
ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'hardware/exports/fabrication-drawing.pdf';c=canvas.Canvas(str(p),pagesize=A4)
c.setTitle('AirMon r0.1 carrier fabrication drawing — prototype')
c.setFont('Helvetica-Bold',19);c.drawString(40,795,'AirMon / Carrier fabrication drawing')
c.setFont('Helvetica',10);c.drawString(40,773,'Prototype r0.1  |  2026-09-06  |  Dimensions in mm; do not scale drawing')
c.setFillColorRGB(.65,.18,.08);c.drawString(40,751,'HOLD: verify supplied PM socket and exact display-board harness before fabrication.')
c.setFillColorRGB(0,0,0)
x0,y0,s=95,683,5
outline=[(0,0),(42,0),(42,48),(10,48),(10,55),(0,55),(0,48),(3,48),(3,43),(0,43)]
def point(x,y):return x0+x*s,y0-y*s
path=c.beginPath();path.moveTo(*point(*outline[0]));[path.lineTo(*point(*p)) for p in outline[1:]];path.close()
c.setLineWidth(1);c.drawPath(path)
for x,y in [(17,45),(39,46)]:
 X,Y=point(x,y);c.circle(X,Y,1.1*s);c.setLineWidth(.4);c.line(X-9,Y,X+9,Y);c.line(X,Y-9,X,Y+9)
c.setFont('Helvetica',9)
c.drawString(335,670,'TOP VIEW')
c.drawString(335,651,'Origin: upper-left corner')
c.drawString(335,637,'+X right; +Y down')
c.drawString(335,607,'2 x 2.20 mm NPTH')
c.drawString(335,593,'Hole centres:')
c.drawString(335,579,'(17.00, 45.00)')
c.drawString(335,565,'(39.00, 46.00)')
c.drawString(335,531,'Main body: 42.00 x 48.00')
c.drawString(335,517,'Tongue: 10.00 x 7.00')
c.drawString(335,503,'Left neck notch: 3.00 x 5.00')
def dim(x1,y1,x2,y2,text,tx,ty):
 c.setLineWidth(.4);c.line(x1,y1,x2,y2)
 for x,y in [(x1,y1),(x2,y2)]:c.line(x-3,y-3,x+3,y+3)
 c.drawCentredString(tx,ty,text)
dim(x0,y0+22,x0+42*s,y0+22,'42.00',x0+21*s,y0+28)
dim(x0-22,y0,x0-22,y0-55*s,'55.00',x0-45,y0-27.5*s)
dim(x0,y0-55*s-18,x0+10*s,y0-55*s-18,'10.00',x0+5*s,y0-55*s-31)
c.setFont('Helvetica-Bold',12);c.drawString(40,345,'Fabrication and assembly notes')
notes=['2 copper layers; FR-4; finished board thickness 1.60 mm; 1 oz copper.',
'Solder mask both sides; lead-free finish; no controlled impedance requirement.',
'Minimum designed track width 0.25 mm; clearance 0.20 mm.',
'Through vias: 0.30 mm drill / 0.60 mm copper diameter. No via-in-pad.',
'Follow Gerber outline and Excellon files for complete machining geometry.',
'Non-dimensioned tolerances: confirm manufacturer standard capability before order.',
'Assemble both sides using drawings, BOM, and placement CSV; check bottom rotation.',
'J1/J2 through-hole assembly is separate from SMT; J5 supplied socket is optional.',
'Keep SHT40 aperture free of flux contamination, wash, adhesive, and coating.',
'No production approval implied: electrical load, thermal behavior, and fit tests pending.']
c.setFont('Helvetica',9)
for i,t in enumerate(notes):c.drawString(40,321-i*18,t)
c.setFont('Helvetica-Oblique',8);c.drawString(40,68,'Source of truth: hardware/airmon.kicad_pcb and hardware/exports/airmon-gerbers.zip')
c.drawString(40,53,'See docs/HARDWARE.md for harness pinout and prototype release gates.');c.showPage();c.save();print(p)
