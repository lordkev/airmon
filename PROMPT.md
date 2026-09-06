# Keyestudio touchscreen air-quality monitor

## Summary

Build a USB-powered desktop monitor around the [Keyestudio 2.8-inch ESP32-32E touchscreen board](https://www.keyestudio.com/products/28-inch-esp32-32e-display-screen-lcd-tft-module-with-touch-wroom-for-arduino), with a compact custom sensor PCB, printable PLA enclosure, local web dashboard, HTTP API, and Home Assistant integration.

Use a landscape touchscreen layout. Temperature and humidity are included; PM, CO₂, and VOC/NOx modules can be purchased and plugged in independently. All configurations use the same firmware.

## Hardware and manufacturing

### Sensor configuration

| Measurement | Hardware | Installation |
|---|---|---|
| Temperature/humidity | Sensirion SHT40 | Assembled on sensor PCB |
| PM1, PM2.5, PM10 | Plantower PMSA003I bare module | Removable matching socket |
| Actual CO₂ | Adafruit SCD40 breakout, product 5187 | Internal STEMMA QT cable |
| VOC/NOx indices | Adafruit SGP41 breakout, product 6455 | Internal STEMMA QT cable |

Every sensor costing more than approximately $10 remains optional and independently installable after assembly.

### Sensor PCB and connections

- Design a two-layer KiCad PCB using standard 1.6 mm FR-4 and 1 oz copper. Minimize its outline around the PM socket, sensor placement, and connectors; it need not span the display board.
- Mount it behind the display using enclosure standoffs and short cable harnesses. Provide factory SMT assembly files; users install cables and optional modules.
- Connect I²C through the Keyestudio expansion connector: **GPIO19 SDA, GPIO18 SCL**, with 3.3 V logic. Keep microSD disabled and its slot empty because these pins share the SD interface. [Board pin allocation](https://www.lcdwiki.com/2.8inch_ESP32-32E_Display)
- Obtain USB-derived 5 V and ground from the UART connector using a power-only harness; leave UART signal pins disconnected. Reserve **GPIO22 low** to enable this supply, accounting for its shared red-LED function. Verify connector numbering and the delivered board revision against the [schematic](https://www.lcdwiki.com/res/E32R28T/2.8inch_ESP32-32_Display_Schematic.pdf).
- Feed the PM sensor from 5 V and provide a dedicated regulated 3.3 V sensor rail. Include decoupling, appropriate protection, test points, and a calculated startup/peak-current budget.
- Use keyed STEMMA QT connections for gas modules and expansion. Check combined I²C pull-ups and voltage levels with every supported module combination.
- Place the SHT40 at a ventilated lower edge on a thermally isolated PCB section, away from the display, processor, regulator, gas-sensor heaters, and PM exhaust.
- Keep the ESP32 antenna region clear of sensor modules, copper, and cables. Verify connector mating heights and cable bend space in assembled CAD.

### Manufacturing deliverables

Deliver editable KiCad sources and local libraries, schematic PDF, PCB STEP model, Gerber/drill ZIP, fabrication drawing, placement CSV, assembly drawings, and assembly BOM suitable for PCBway.

Provide a separate complete purchasing BOM covering the display board, components, optional modules, exact mating connectors and harnesses, fasteners, printed parts, and a 5 V USB supply. Include manufacturer part numbers, quantities, sourcing links, dated prices, and required/optional classifications.

## Enclosure

- Create a parametric OpenSCAD enclosure and export printable STLs alongside editable source.
- Use a landscape front bezel with a stable, slightly reclined desktop base, rear shell, and removable internal sensor mounts.
- Design around the display board’s approximately **86 × 50 mm** outline. Final enclosure depth follows the assembled sensor and cable clearances. [Mechanical documentation](https://www.lcdwiki.com/2.8inch_ESP32-32E_Display)
- Provide shallow and full-sensor rear-shell variants sharing the front bezel. The full version accommodates PM, CO₂, and VOC/NOx simultaneously.
- Target PLA, a 0.4 mm nozzle, 0.2 mm layers, support-free printing, and screws with captive nuts.
- Preserve touchscreen edge clearance so the bezel cannot press on the resistive panel. Include USB access, BOOT/RESET access, strain relief, and secure mounting through the board’s mounting holes.
- Separate PM intake and exhaust openings to limit recirculation. Give the temperature and gas sensors direct room-air exposure while limiting heat transfer from electronics.
- Supply print orientations, assembly illustrations, and a small fit-test print for fasteners and mating clearances.

## Firmware and interfaces

### Device firmware and touchscreen

- Use ESP-IDF targeting the classic **ESP32**, with pinned dependencies, ILI9341 display support, XPT2046 resistive-touch support, and LVGL using partial display buffers.
- Use separate SPI controllers for display and touch, following the board’s wiring. Reserve the repurposed expansion pins exclusively for sensor I²C.
- Show temperature, humidity, PM2.5, CO₂, and gas indices on the landscape overview. Tapping a metric opens its trend and sensor status; provide large touch targets and a settings page.
- Include first-use touch calibration, saved calibration, and a recalibration option. BOOT remains a fallback control: short press changes pages; a five-second press during normal operation opens Wi-Fi setup.
- Default backlight brightness to 30%, with adjustable brightness and a configurable dimming timer.
- Implement independent sensor drivers with CRC/checksum validation, startup timing, retries, and explicit absent, warming-up, valid, stale, and failed states.
- Use SHT40 readings for ambient measurements and SGP41 compensation. Apply Sensirion’s Gas Index Algorithm; expose VOC/NOx as indices, never estimated CO₂ or identified chemical concentrations.
- Provide documented CO₂ calibration controls and temperature correction. Disable automatic CO₂ baseline calibration by default until the user enables it under suitable placement conditions.

### Networking, web dashboard, and API

- On first boot, create a password-protected setup AP with credentials displayed onscreen, captive-portal support, and a direct setup address. Keep phone/browser provisioning as the primary Wi-Fi setup flow.
- Commit new Wi-Fi credentials after a successful connection. Preserve existing credentials through failed changes and network outages; allow physical re-entry to setup.
- Serve a responsive local dashboard with live measurements, sensor health, settings, and 24-hour charts. Store one-minute aggregates in RAM and indicate that history resets on reboot.
- Embed all web assets in firmware; require no cloud account, CDN, or internet connection for normal operation.
- Expose `GET /api/v1/readings`, `/status`, `/history`, and `/config`; `PUT /api/v1/config`; and `POST /api/v1/firmware`.
- Include measurement units, timestamps or uptime, reading age, and validity. Represent unavailable values as `null`.
- Protect configuration changes and firmware uploads with a per-device administrator credential. Keep measurement reads available on the local network and document HTTP’s security assumptions.
- Add optional MQTT configuration, Home Assistant discovery, stable device/entity identifiers, availability reporting, and automatic reconnection.
- Support browser firmware uploads with two OTA application slots and rollback. Keep compressed assets inside each application image and enforce flash/RAM budgets for the 4 MB board. Retain USB flashing and recovery.

## Validation and delivery

- **Electrical:** Pass ERC/DRC; review footprints, harness pinouts, shared GPIO functions, power startup, rail voltage under load, and every optional-sensor combination.
- **Mechanical:** Check assembled-model collisions, antenna clearance, airflow, touchscreen clearance, fasteners, printable meshes, and sliced orientations.
- **Software:** Test sensor parsing, corrupted data, missing sensors, history rollover, touch calibration, provisioning, configuration persistence, Wi-Fi/MQTT recovery, Home Assistant discovery, and interrupted OTA.
- **Physical prototype:** Verify enclosure fit, touch operation while assembled, viewing angle, fully populated power consumption, a 24-hour operating run, and temperature bias against a separate reference.
- **Documentation:** Include ordering, printing, assembly, tool installation, compilation, prebuilt firmware flashing, USB recovery, configuration, calibration, API examples, dashboard use, Home Assistant setup, and troubleshooting.
- **Release package:** Deliver editable sources, manufacturing files, complete BOMs, STLs, firmware binaries, documentation, and validation results. Clearly distinguish completed checks from physical tests still pending.

Defaults remain indoor desktop use, USB power, PLA, plug-in upgrades, and no firm overall budget cap. Battery operation, audio, persistent microSD logging, and additional gas species are outside the first version. Confirm the purchased Keyestudio revision matches the documented E32R28T hardware before fabrication; label the initial release as a prototype until physical validation is complete.

## Additional documentation requirement

Include renders of the assembled circuit board and the completed unit in its 3D-printed enclosure. Also provide an exploded assembly view showing the display board, sensor carrier, optional sensor modules, and enclosure parts.
