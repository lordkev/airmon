# HTTP API v1

Base URL: `http://DEVICE_IP/api/v1`. The same API is available at `http://192.168.4.1/api/v1` while connected to the setup AP. Requests and responses use UTF-8 JSON except the firmware binary upload and the noted text responses. The device does not grant cross-origin browser access.

## Authentication and errors

GET endpoints are readable without authentication. All mutations require `Authorization: Bearer TOKEN`, where TOKEN is the 32-character administrator credential displayed on the LCD. HTTP is not encrypted; use a trusted LAN. Missing/incorrect tokens return `401` and `{"error":"Administrator token required"}`.

Other error statuses include `400` invalid input/upload, `409` busy or unavailable calibration, `413` JSON outside the 1–2,048 byte limit, and `503` insufficient history-response memory. Error bodies normally contain an `error` string. Unknown `/api/` GET routes return `404`.

## Endpoints

| Method | Path after `/api/v1` | Result |
|---|---|---|
| GET | `/readings` | Current measurements, units, validity and age |
| GET | `/status` | Firmware/network/heap/config/sensor status |
| GET | `/history?limit=180&before=...` | Paginated minute aggregates |
| GET | `/config` | Redacted configuration |
| PUT | `/config` | Partial settings update; `202` means pending |
| POST | `/calibration/co2` | Forced CO₂ calibration request; `202` |
| POST | `/calibration/touch` | Begin LCD touch calibration; `200` text |
| POST | `/firmware` | Raw application binary; `200` text then reboot |

## Readings

```sh
curl http://DEVICE_IP/api/v1/readings
```

The response contains `uptime_ms`, `timestamp` (Unix seconds or `null`), and `metrics`. Each metric is an object with `value`, `unit`, `status`, and `age_ms`. Illustrative metric entry:

```json
{"value":22.4,"unit":"°C","status":"valid","age_ms":540}
```

| Key | Unit | Source |
|---|---|---|
| `temperature` | `°C` | SHT40 plus configured correction |
| `humidity` | `%` | SHT40 |
| `pm1`, `pm2_5`, `pm10` | `µg/m³` | PMSA003I atmospheric/environmental fields |
| `co2` | `ppm` | SCD40 actual CO₂ |
| `voc_index`, `nox_index` | `index` | Sensirion Gas Index Algorithm |

All eight keys are present. Unavailable values are `null`, never zero as a missing-data sentinel. Status is `absent`, `warming_up`, `valid`, `stale`, or `failed`. A reading expires after 5 seconds, or 15 seconds for CO₂. `age_ms` is elapsed time since the last received sample; it is `null` if no sample exists. A warming sample may have an age but still has a null value. These gas indices are not estimated CO₂, ppm of a particular VOC, or an AQI.

## Status

`GET /status` returns `version`, `device_id` (12 hexadecimal MAC-derived characters), `board`, `uptime_ms`, `free_heap`, `minimum_free_heap`, `wifi_connected`, `ip`, `ap_active`, `mqtt_connected`, `config_pending`, `config_result`, `touch_calibrated`, `ota_busy`, `history_points`, and `sensor_errors`.

Heap values are bytes. `sensor_errors` is a four-element counter array in SHT40, PMSA003I, SCD40, SGP41 order. Counters and history reset on reboot. Poll `config_pending` and `config_result` after an asynchronous configuration or CO₂ calibration request. No passwords or administrator token are included.

## History

```sh
curl 'http://DEVICE_IP/api/v1/history?limit=180'
```

The response shape is:

```json
{
  "resolution_seconds":60,
  "persistent":false,
  "points":[
    {"uptime_s":120,"timestamp":null,"temperature":22.4,"humidity":45.1,"pm1":null,"pm2_5":null,"pm10":null,"co2":null,"voc_index":null,"nox_index":null}
  ],
  "next_before":120
}
```

`limit` is 1–180, default 180. Both numeric query fields accept decimal digits only; signs, suffixes, fractions, uint32 overflow, and oversized/truncated query strings return `400`. `before` is an exclusive unsigned uptime-second boundary, not an epoch time. The latest eligible points are selected and returned oldest-to-newest. To fetch older pages, pass `next_before` as `before` and prepend their points. Stop on an empty/short page or `next_before:null`. Eight full pages cover 1,440 points. A full final page can legitimately require one extra empty request to establish the end.

Values are minute means of fresh valid samples. The incomplete current minute is omitted. Missing minutes/metrics remain null. UTC may be absent until SNTP succeeds; use uptime for stable ordering. Discard prior pagination state after a device reboot.

## Configuration

`GET /config` returns the fields below except passwords, plus `wifi_password_set` and `mqtt_password_set`. The PUT operation is a partial update despite its method name: omitted fields retain their values. Unknown fields and incorrect types are rejected.

| Field | Type / limits | Default |
|---|---|---|
| `name` | Nonempty string, at most 32 UTF-8 bytes | `AirMon xxxxxx` |
| `ssid` | Nonempty string, at most 32 UTF-8 bytes | Empty until provisioned |
| `password` | Empty for open Wi-Fi, otherwise 8–63 bytes | Empty |
| `brightness` | Integer, 5–100 percent | 30 |
| `dim_seconds` | Integer, 0–3,600; zero disables dimming | 120 |
| `temperature_offset` | Number, −10 to +10 °C | 0 |
| `altitude_m` | Integer, 0–3,000 m | 0 |
| `co2_asc` | Boolean | false |
| `mqtt_enabled` | Boolean | false |
| `mqtt_uri` | String, at most 192 bytes; `mqtt://` or `mqtts://` when enabled | Empty |
| `mqtt_user` | String, at most 64 bytes | Empty |
| `mqtt_password` | String, at most 128 bytes | Empty |

Control characters are rejected in strings. Use the separate MQTT credential fields; credentials embedded in broker URIs are rejected because the URI is returned by GET config. Password omission preserves a stored password, while `"password":""` explicitly clears it. Do not PUT the GET response wholesale: the two read-only password-set flags are not accepted input fields. A Wi-Fi SSID is required in the resulting configuration, including for a first settings update.

```sh
curl -X PUT http://DEVICE_IP/api/v1/config \
  -H 'Authorization: Bearer TOKEN' -H 'Content-Type: application/json' \
  --data '{"ssid":"My Wi-Fi","password":"replace-this-password","name":"Living room"}'
```

`202 {"status":"pending"}` means the change was queued. New Wi-Fi credentials are tested for up to 40 seconds before persistence. Failed changes retain/restore the previous settings. A concurrent configuration or firmware update returns `409`. Poll `/status`; connection changes may require reconnecting to the new device IP. The setup AP closes 60 seconds after a successful connected save.

## Calibration

```sh
curl -X POST http://DEVICE_IP/api/v1/calibration/co2 \
  -H 'Authorization: Bearer TOKEN' -H 'Content-Type: application/json' \
  --data '{"reference_ppm":420}'
```

Use your actual known reference, not the example blindly. `reference_ppm` must be an integer from 400–2,000. CO₂ must be fresh/valid and periodic measurement must have run for at least three minutes. Success returns `202 {"status":"calibration_pending"}`; completion/failure appears in `config_result`. This is not a substitute for stable reference air.

```sh
curl -X POST http://DEVICE_IP/api/v1/calibration/touch \
  -H 'Authorization: Bearer TOKEN'
```

Touch calibration takes place on the LCD. The response is plain text, `Touch calibration started on the display`.

## Firmware upload

```sh
curl -X POST http://DEVICE_IP/api/v1/firmware \
  -H 'Authorization: Bearer TOKEN' -H 'Content-Type: application/octet-stream' \
  --data-binary @release/firmware/airmon.bin
```

Send a normal Content-Length upload of the application `.bin`, not multipart/form-data or a chunked request. The maximum image size is the inactive slot size, 1,966,080 bytes. Success returns plain text `Firmware validated. Rebooting.` The device reboots shortly afterwards. Invalid/incompatible/incomplete uploads return `400` and keep the current image selected. See [firmware and recovery](FIRMWARE.md) for rollback behavior and pending hardware tests.

## Live acceptance checks

Once a prototype is flashed and reachable, run the read-only checker from the repository root:

```sh
python3 scripts/test_device_api.py http://DEVICE_IP
```

It checks real HTTP responses, metric/null/freshness semantics, history pagination, malformed query rejection, redacted configuration, dashboard delivery, and three concurrent readers. The JSON report defaults to `tmp/device-api-validation.json`. It sends no configuration, calibration, or firmware updates. An empty history can pass structural checks; it is not evidence of a 24-hour run. Hardware-dependent tests in the validation ledger remain separate.
