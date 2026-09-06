# AirMon carrier — prototype r0.1

Open `airmon.kicad_sch` and `airmon.kicad_pcb` in KiCad 10. Local symbols, footprints, and STEP models are included. The carrier is 42 × 55 mm overall, with a ventilated SHT40 tongue and optional PMSA003I mating socket.

Read [hardware and ordering](../docs/HARDWARE.md) before sending files to a manufacturer. **Exact display-board connector identification and the supplied PM socket fit remain unresolved physical checks.** ERC/DRC results apply to the digital design and do not remove these release gates.

- `exports/airmon-gerbers.zip`: copper, mask, silk, paste, outline, PTH and NPTH drill files.
- `exports/schematic.pdf`, `assembly-top.pdf`, `assembly-bottom.pdf`: electrical and assembly drawings.
- `exports/footprint-fit-1to1.pdf`: print at actual size to compare the supplied PM socket.
- `exports/placement-smt.csv`: SMT placement; J1/J2 require through-hole assembly separately.
- `BOM.xlsx`, `assembly-bom.csv`, `purchasing-bom.csv`: components and full purchasing data.
- `exports/airmon.step`: assembled carrier geometry.

J5 pad numbering follows Plantower, not the generic numbering in the Adafruit reference design. It must be checked against the supplied mating socket. Physical module models in enclosure illustrations are simplified envelopes.
