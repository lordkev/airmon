# Requirements and validation ledger

**Prototype r0.1, 2026-09-06. No physical prototype has been tested.** The digital deliverables are provided for review and prototype preparation. Production/manufacturing approval remains withheld pending purchased-part and physical checks.

## Completed digital checks

| Area | Evidence | Result and limit |
|---|---|---|
| PCB electrical rules | [erc.txt](../hardware/exports/erc.txt) | 0 violations |
| PCB layout rules | [drc.txt](../hardware/exports/drc.txt) | 0 violations, 0 unconnected items |
| Schematic/PCB consistency | [netlist-validation.json](../hardware/exports/netlist-validation.json) | All 56 connected pads agree across 9 nets; 519 tracks, 60 vias |
| Manufacturing exports | [exports](../hardware/exports/) | Gerber/drill ZIP, placement CSVs, schematic/assembly/fabrication PDFs, 1:1 footprint print, and STEP generated; drawings visually inspected |
| BOM | [BOM.xlsx](../hardware/BOM.xlsx), two CSVs | Required/optional classifications, sources, estimates, and unknown quotes distinguished; workbook formula scan found no errors |
| Mesh topology | [mesh-validation.json](../enclosure/mesh-validation.json) | 7 watertight STLs; consistent winding, correct body counts, positive print-bed placement |
| Full CAD assembly | [clearance-validation.json](../enclosure/clearance-validation.json) | 28 solid-pair checks, no overlapping volume above 0.01 mm³ |
| Shallow CAD assembly | [clearance-validation-shallow.json](../enclosure/clearance-validation-shallow.json) | 10 solid-pair checks, same tolerance |
| Reference slicing | [slicing-validation.json](../enclosure/slicing-validation.json) | All 7 parts sliced successfully with no slicer warning strings; 0.4 mm nozzle, 0.2 mm layers, 4 walls, 15% gyroid, supports disabled |
| Documentation renders | [images](images/) and [assembly guide](ASSEMBLY.md) | Native PCB top/underside, PCB with PM, completed enclosure, exploded assembly; CAD/illustrative-screen limitations captioned |
| ESP32 compilation | [build report](firmware-build.txt), [manifest](../release/firmware/manifest.json) | Successful ESP-IDF v5.5.1 build; about 33% free in each OTA slot; runtime memory/load tests pending |
| Portable firmware logic | `scripts/test_core.sh` | ASan/UBSan pass: CRC/checksum, corrupt frames, PM field selection, freshness/nulls, history means/gaps/rollover, touch transform, token comparison, strict unsigned history-query parsing |
| Configuration logic | `scripts/test_config.sh` | ASan/UBSan pass using actual config.c with transactional NVS fake: limits, password redaction, credential/touch persistence, failed-commit preservation |
| Embedded web UI | [web-validation.json](web-validation.json) | 10 checks in isolated Chrome with simulated HTTP API; desktop/mobile layout, pagination, auth errors, password semantics, offline state, no uncaught JS errors |

Slicing is a reference-printer computation, not a physical print. Collision checks use the actual KiCad STEP carrier and simplified display/module envelopes; screws, cables, foam, and manufacturing tolerances are not validated solids. The firmware host/browser tests do not emulate physical I²C, ESP32 Wi-Fi, the HTTP server, NVS flash, TLS allocation, or OTA flash writes.

## Remaining release gates

| Requirement | What is still required |
|---|---|
| Exact display and harness | Match delivered E32R28T PCB; identify board-side connector housing/contact MPN or obtain confirmed mating pigtails; verify numbered conductors |
| PM socket | Compare supplied socket to 1:1 land pattern, confirm Plantower mating pin map, measure mating height and actual port alignment |
| BOM completeness for ordering | Resolve the two board-side connector housings/contacts and socket sourcing/consignment; obtain PCB/PCBA/harness quotes |
| Electrical behavior | Power startup, USB/Q5 drop, 5 V and 3.3 V rail minima, current, LDO temperature, I²C LOW voltage/rise time for all eight module combinations |
| Mechanical behavior | Fit coupon, print bridges, nut retention, touchscreen edge clearance, cable bend/strain relief, antenna placement, duct foam seal, service access, stand stability |
| Device UI and drivers | Actual LCD colors/orientation, touch calibration and BOOT, real sensor readings, failure/recovery and warmup timings |
| Provisioning | Successful/failed Wi-Fi changes, router outage/reboot, NVS persistence after power loss, AP reentry and closure |
| API | Real ESP32 HTTP positive/negative requests, simultaneous readers, history pagination over reboot, auth and body limits |
| MQTT/Home Assistant | Broker integration, discovery, per-metric availability, last will, broker/router restarts, TLS memory stress |
| OTA | Incompatible image, interrupted upload/write, new-image crash/rollback, preserved settings, successful USB recovery |
| Long run and ambient accuracy | Fully populated 24-hour run, minimum free heap, resets/error counts, calibrated temperature reference at multiple brightness/load conditions |

The costly sensors remain optional. A later PM upgrade is plug-in only if its socket was installed initially; otherwise that upgrade includes one soldering step. This procurement limitation is documented rather than hidden in the optionality claim.

## Physical acceptance procedure

Record board revision, module revisions, firmware manifest hash, printer/filament, ambient conditions, date, and instrument models before testing. Do not mark a row passed without measured evidence.

1. **Unpowered inspection:** Verify harness continuity, labels and no shorts. Match both supplied connectors to drawings. Confirm pin 1 and module port orientations.
2. **Fit coupon and enclosure:** Print the coupon before full parts. Verify captive nuts and alignment strip. Assemble gently; test touch across all corners with the shell open and closed. Check access to USB/BOOT/RESET and remove/reinstall the sensor frame.
3. **Electrical matrix:** For each PM/CO₂/SGP41 on/off combination, unplug USB before changing modules. Record steady and startup current, minimum SENSOR5V/SENSOR3V3, LDO temperature, and I²C rise/LOW measurements. PM input must remain 4.5–5.5 V; target carrier 3.3 V ±5%. Confirm the red LED/GPIO22 supply behavior. Keep SD empty.
4. **Driver behavior:** Confirm supported addresses and fresh readings. With power removed, disconnect one sensor and reboot: other metrics must continue, absent values must be null. Reconnect and verify detection. Use a controllable I²C test fixture for corrupt-response and held-bus tests; never short a powered supply.
5. **Wi-Fi/API:** Provision correct credentials, then attempt incorrect ones and confirm old configuration returns. Cycle the router. Reopen AP with BOOT. Verify GET reads, rejected unauthenticated writes, valid configuration, oversized JSON, and concurrent history reads.
6. **MQTT:** Observe retained discovery and unavailable missing sensors. Restart Home Assistant and the broker. Remove Wi-Fi temporarily and confirm availability expires; reconnect and confirm recovery. Repeat with MQTT TLS enabled while monitoring minimum heap.
7. **OTA/recovery:** Save a known-good USB bundle. Try a wrong-chip image and an interrupted HTTP transfer; verify the current image still boots. On a sacrificial prototype, interrupt flash programming and test a deliberately crashing new image to verify rollback. Confirm all settings survive successful OTA. Demonstrate BOOT/RESET USB recovery.
8. **Thermal/24-hour run:** Place an independent temperature reference near the inlet, clear of exhaust. Compare after equilibrium at 5%, 30%, and 100% brightness with optional modules absent and present. Record changing bias; redesign airflow/isolation if an offset is insufficient. Run fully populated for 24 hours with charts and MQTT active. Record uptime, minimum heap, error counters, reset history, rail minima, and sample continuity.
9. **Release decision:** Resolve connector/BOM unknowns, attach measurements and photos, update models/firmware as required, rerun affected digital checks, and only then remove the prototype hold.

## Test record template

| Date / configuration | Instrument / method | Measurement / observation | Pass criterion | Result |
|---|---|---|---|---|
| Pending | Pending | Pending | As specified above | Not tested |

The private GitHub repository is `lordkev/airmon`. Commits containing these files are prototype checkpoints, not evidence of physical acceptance.

## Follow-up validation preparation

- History query parsing now rejects malformed decimal values, signed inputs, fractions, overflow, and truncated query strings. Core sanitizer tests cover these boundaries.
- Touch recalibration requests are transferred atomically to the display task, preventing a concurrent request from being cleared by the consumer. Physical touchscreen validation remains pending.
- `scripts/test_device_api.py` provides read-only acceptance checks against an actual device. It has not been run against hardware because no ESP32 serial device or AirMon network address is available. Run it after first provisioning and retain its JSON report with the physical test record.
