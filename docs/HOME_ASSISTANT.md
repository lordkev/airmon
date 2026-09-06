# MQTT and Home Assistant

MQTT is optional and disabled by default. AirMon's LCD, web page, and HTTP API work without a broker.

1. Configure a broker and add Home Assistant's [MQTT integration](https://www.home-assistant.io/integrations/mqtt/).
2. In AirMon web Settings, enter the administrator token, enable MQTT, and enter the broker URI, username, and password. Use `mqtt://192.168.1.10:1883` for a trusted LAN or `mqtts://broker.example.com:8883` for TLS with a publicly trusted certificate. Use the separate credential fields; URI-embedded credentials are rejected.
3. Save, wait for “Settings saved,” and check `mqtt_connected:true` through the status endpoint. Self-signed/custom CA certificates and mutual TLS are not supported by this prototype's configuration UI.
4. Look for the AirMon device in Home Assistant's MQTT devices. Discovery creates eight entities; uninstalled or warming sensors remain unavailable.

## Topic contract

`DEVICE_ID` is the 12-character identifier returned by `/api/v1/status`.

| Topic | Payload / retention |
|---|---|
| `airmon/DEVICE_ID/state` | Flat JSON with all eight metric keys; values or null; not retained; about every 10 seconds |
| `airmon/DEVICE_ID/availability` | `online` / `offline`; retained; last will is `offline` |
| `airmon/DEVICE_ID/availability/METRIC` | `online` only for a fresh valid metric; retained |
| `homeassistant/sensor/airmon_DEVICE_ID/METRIC/config` | Retained discovery JSON |

Each entity requires both device and metric availability to be online. Discovery uses stable unique IDs `airmon_DEVICE_ID_METRIC`, measurement state class, and an expiry of 35 seconds. CO₂ uses the carbon-dioxide device class; PM uses PM1/PM2.5/PM10 classes; VOC/NOx are generic index sensors, not AQI or gas concentration classes.

AirMon subscribes to `homeassistant/status` and republishes discovery/state when it receives `online`. It reconnects after broker outages and republishes on connection. The broker keepalive is 30 seconds and the reconnect delay is 5 seconds. When disabling MQTT, existing retained discovery records remain in the broker; delete those specific topics if you want to remove the old entities permanently.

If discovery does not appear, verify the broker address, credentials, ACL permission for the topic prefixes, and Home Assistant's discovery setting. A broker ACL should permit AirMon to publish its own `airmon/DEVICE_ID/#` and discovery prefix and subscribe to `homeassistant/status`. TLS certificate validation also requires a correct clock; the prototype obtains time from `pool.ntp.org` using SNTP. Actual broker restart, discovery, and last-will timing tests are listed as pending in the validation ledger.
