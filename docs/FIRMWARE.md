# Firmware: flashing, building, and recovery

The firmware targets **classic ESP32 with 4 MB flash**, without PSRAM. It has been compiled successfully; LCD orientation, touch, Wi-Fi setup, sensors, and OTA still require testing on the exact board. Keep the microSD slot empty. Do not flash an ESP32-C6/S3 image onto this board.

## Flash the supplied prototype

Install Python 3.12 and create an isolated tooling environment. Commands below assume a terminal at the repository root; substitute your serial port for `PORT`.

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install esptool==4.12.0
cd release/firmware
shasum -a 256 -c SHA256SUMS
python -m esptool --chip esp32 --port PORT --baud 460800 write_flash @flash_args
```

On macOS, list ports with `ls /dev/cu.*`; the USB serial device should appear when the data cable is connected. Linux commonly uses `/dev/ttyUSB0`; Windows uses a COM port. Use a data-capable USB-A-to-C cable. If needed, install the USB-to-serial driver corresponding to the chip on the delivered display board; do not assume a chip based only on the product title.

Disconnect the sensor carrier for the first display-board flash. If automatic bootloader entry fails, hold BOOT, press/release RESET, then release BOOT when the upload begins. Try 115200 baud if uploads fail at 460800. Press RESET after flashing. Open a 115200-baud serial monitor for diagnostic logs; credentials are displayed on the LCD, not printed to the log.

`flash_args` writes these images without erasing the entire flash:

| Offset | Image | Purpose |
|---|---|---|
| `0x1000` | `bootloader.bin` | ESP-IDF bootloader with rollback |
| `0x8000` | `partition-table.bin` | 4 MB partition map |
| `0xf000` | `ota_data_initial.bin` | Initial OTA selection metadata |
| `0x20000` | `airmon.bin` | Application, including dashboard assets |

Check [manifest.json](../release/firmware/manifest.json) and hashes before flashing. The application alone is used for browser OTA; do not upload a bootloader, merged flash image, or partition table through the web interface.

## Build from source

Install Git, CMake, Ninja, and Python 3.12. The pinned SDK is **ESP-IDF v5.5.1**, with recursively checked-out submodules. Follow Espressif's [platform-specific prerequisites](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/get-started/index.html) if your host lacks build tools.

```sh
mkdir -p .tools
git clone --recursive --branch v5.5.1 --depth 1 https://github.com/espressif/esp-idf.git .tools/esp-idf
export IDF_TOOLS_PATH="$PWD/.tools/idf"
.tools/esp-idf/install.sh esp32
. .tools/esp-idf/export.sh
scripts/build_firmware.sh
```

The script embeds the web page using deterministic gzip compression, then runs `idf.py build`. `firmware/dependencies.lock` pins managed components, including LVGL 9.2.2, ILI9341 2.0.1, and mDNS 1.8.2. Sensirion's Gas Index Algorithm source and license are vendored with an upstream commit reference.

For configuration and development flashing:

```sh
cd firmware
idf.py menuconfig
idf.py -p PORT flash monitor
```

Exit the IDF monitor with Ctrl+]. To regenerate the supplied binary bundle after a successful build, run `python3 scripts/package_firmware.py` from the repository root. `sdkconfig.defaults` is authoritative for a fresh build. Existing `sdkconfig` values override defaults; review changes with `idf.py menuconfig` when updating an existing checkout.

## Flash and RAM limits

Two OTA slots each contain `0x1e0000` bytes (1,966,080 bytes). The prototype image uses about 1.33 MB, leaving about 33% in each slot; exact values are in the manifest. NVS occupies `0x9000–0xefff`. History is a 1,440-point RAM ring, not a filesystem or SD log.

The measured build uses 146,036 bytes of static DRAM and 99,391 bytes of IRAM. Static DRAM headroom is 34,700 bytes; additional runtime heap regions are separate from that linker figure. LVGL's pool is limited to 48 KiB, its two partial DMA buffers total 25,600 bytes, Wi-Fi dynamic RX/TX buffers are capped at eight each, and the MQTT outbox is bounded at 8 KiB. This is a build budget, not proof of runtime headroom. Track `free_heap` and `minimum_free_heap` from the status endpoint through TLS connection, chart loading, OTA, and the 24-hour test.

## Browser updates and rollback

Open Settings, enter the administrator token, choose **application `airmon.bin`**, and select Upload firmware. The device checks image size, classic ESP32 chip ID, project name, and ESP-IDF image validity before selecting the inactive slot. Do not disconnect power while updating. A failed transfer leaves the current image selected. A new image that resets before startup validation can roll back to the previous image. Startup is marked valid after its services initialize and a ten-second interval; it does not wait for optional sensors or a reachable router.

This is not signed-firmware verification: anyone with the administrator token can upload a compatible image. Actual interrupted-transfer, interrupted-flash, and rollback tests remain pending on hardware.

## Recovery and factory reset

If the application fails to start, enter the ROM bootloader with BOOT/RESET and reflash the complete supplied bundle over USB. NVS is preserved by normal flashing. Firmware deliberately does not silently erase NVS when initialization fails.

A factory reset is a separate destructive action: it deletes Wi-Fi/MQTT settings, administrator/AP credentials, touch calibration, and installed firmware. Only when you intend that reset, run:

```sh
python -m esptool --chip esp32 --port PORT erase_flash
python -m esptool --chip esp32 --port PORT --baud 460800 write_flash @flash_args
```

New credentials are generated after the reset. Ordinary network recovery needs only a five-second BOOT press during normal operation to reopen setup; it does not require a factory reset.
