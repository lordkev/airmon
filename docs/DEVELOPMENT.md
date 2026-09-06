# Reproducing the design

The checked-in native files are editable without regeneration. Scripts that generate the board or schematic overwrite their output; preserve manual edits before running them. The repository does not include downloaded SDKs, native applications, virtual environments, or build caches.

## Tools

| Area | Version used |
|---|---|
| Electronics | KiCad 10.0.6, including its `pcbnew` Python module |
| Enclosure | OpenSCAD 2026.09.05, manifold backend |
| Mechanical checks | Python 3.12; CadQuery 2.8.0, trimesh 5.1.0, manifold3d 3.5.2 |
| Firmware | ESP-IDF v5.5.1, esptool 4.12.0, locked managed components |
| Slicing check | Bambu Studio 02.08.02.61; isolated A1 0.4 mm / Generic PLA reference profile |
| Host software tests | Clang with AddressSanitizer/UndefinedBehaviorSanitizer; Node 24 and Playwright/Chrome |
| BOM authoring | Node with `@oai/artifact-tool`; CSVs and XLSX are already delivered |

Install KiCad from [kicad.org](https://www.kicad.org/download/) and OpenSCAD from [openscad.org](https://openscad.org/downloads.html). Set `KICAD_CLI`, `KICAD_SHARE`, and `OPENSCAD` if executables/libraries are outside normal locations. `KICAD_CONFIG_HOME` can point to a private configuration directory. Run PCB Python scripts with KiCad's bundled Python, which has `pcbnew` available; other scripts use ordinary Python.

## Electronics

For export from the existing native files:

```sh
python3 scripts/export_hardware.py
KICAD_PYTHON scripts/check_netlist.py
python3 scripts/fabrication_drawing.py
```

Replace `KICAD_PYTHON` with the executable path for KiCad's Python. `fabrication_drawing.py` needs `reportlab`. The exporter requires successful ERC/DRC, produces PDFs/netlist/Gerbers/drills/placement/STEP, and verifies that the manufacturing ZIP contains the copper layers as well as drills. The independent connectivity check compares every connected schematic pin with PCB pad nets.

Full regeneration, only when you intend to replace the editable layout:

```sh
KICAD_PYTHON scripts/build_hardware.py
python3 scripts/prepare_models.py
KICAD_PYTHON scripts/build_hardware.py
KICAD_PYTHON scripts/route_hardware.py
python3 scripts/build_schematic.py
python3 scripts/export_hardware.py
KICAD_PYTHON scripts/check_netlist.py
```

The first board pass prepares footprint copies; model preparation attaches local STEP paths; the second pass loads those models. Routing uses a two-layer grid search and explicit fine-pitch escape rules. Inspect regenerated geometry and drawings even after a clean DRC. `components.json` records the net/pad specification. Vendor sources and commits are in [the reference manifest](../hardware/references/manifest.json).

To render the actual populated PCB:

```sh
kicad-cli pcb render --side bottom --rotate '25,0,30' --zoom 0.65 \
  --quality high --background opaque --floor --width 1600 --height 1200 \
  -o docs/images/assembled-pcb-underside.png hardware/airmon.kicad_pcb
```

Use `--side top` and a different output name for the top view. Documentation enclosure/module renders come directly from OpenSCAD; no generated product photography is used.

## Mechanical export and checks

```sh
python3 -m venv .venv-cad
. .venv-cad/bin/activate
python -m pip install cadquery==2.8.0 trimesh==5.1.0 manifold3d==3.5.2 networkx
python scripts/export_enclosure.py
python scripts/validate_meshes.py
python scripts/export_clearance_step.py
python scripts/check_clearances.py
python scripts/check_clearances.py --shallow
```

The exporter creates all seven STLs, full/shallow collision envelopes, and three enclosure documentation views. Mesh validation welds only export-roundoff vertices at 0.00001 mm precision and removes degenerate/duplicate faces; it does not fill holes. It checks watertightness, normals, body count, volume preservation, and placement above the print bed.

The clearance check intersects the actual KiCad STEP carrier with the printed parts and simplified vendor component envelopes. Full configuration has 28 pair checks; shallow has 10. A zero intersection is only a check of represented solids: screws, cables, connector mating tolerances, airflow, and thermal behavior still require a physical prototype.

`python scripts/check_slicing.py` runs Bambu Studio locally using an isolated data directory. Set `BAMBU_APP` to its app bundle if needed. It flattens bundled profile inheritance, applies the documented print settings, slices each STL, and records results. Files under `tmp/slicing/` are validation artifacts for a reference printer, not universal print-ready G-code. Re-slice STLs with your actual printer and filament profile.

## Firmware and tests

Follow [firmware installation](FIRMWARE.md) for the SDK. From the repository root:

```sh
scripts/test_core.sh
scripts/test_config.sh
scripts/build_firmware.sh size
python3 scripts/package_firmware.py
```

Core tests cover sensor CRC/checksum parsing, atmospheric PM field selection, invalid readings, history means/gaps/rollover, touch transforms, and token comparison. Configuration tests compile the production parser/persistence code with a transactional NVS fake. They exercise input limits, password redaction, persistence across simulated reboot, and failed-commit preservation. The fake does not establish real flash durability or FreeRTOS synchronization.

For browser testing, install the `playwright` npm package in an isolated development environment with Google Chrome available, then run:

```sh
node tests/test_web.mjs
```

It serves the actual embedded HTML against a simulated API, checks desktop/390 px mobile layouts, metric states, paginated history, authorization errors, password-preservation semantics, and loss of connection. It does not emulate the ESP32 HTTP server or Wi-Fi stack. No external server is contacted by the fixture.

The XLSX builder reads `hardware/bom-data.json` and requires `@oai/artifact-tool`:

```sh
node scripts/build_bom.mjs
```

The resulting workbook and CSVs are usable without that tool. Prices labeled as estimates remain estimates. Editing source JSON alone does not update the exported BOMs.

## Prototype package

`release/firmware/manifest.json` contains exact flash offsets, byte counts, hashes, and remaining OTA space. Run `python3 scripts/package_firmware.py` only after a successful build. `scripts/audit_release.py` checks required artifacts, local documentation links, ZIP layers, binary hashes, mesh reports, and reference to the renders.

Physical acceptance results belong in [VALIDATION.md](VALIDATION.md), with measured board revision, sensor serial/revision information, test date, and observations. Do not change the prototype designation merely because software/CAD checks pass.
