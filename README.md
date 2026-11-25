# GreenHouse IQ — Smart Irrigation Prototype

GreenHouse IQ combines an ESP32 firmware sketch for four soil-moisture zones with a root-level browser simulation and BLE-related web helper files. The firmware reads soil and air sensors, exposes a BLE GATT service, and can drive four irrigation valves, a pump, and a sprinkler relay.

**Status clarification:** the current `index.html` is titled and structured as an interactive system demo; it does not import the checked-in BLE manager. The repository also contains PWA/deployment scaffolding whose paths do not match the current root-level layout. Do not assume the browser demo is a live BLE dashboard or that offline installation/GitHub Pages deployment works without further integration.

## Firmware behavior

- Reads a DHT22 for air temperature/humidity, four capacitive soil-moisture inputs, and a DS18B20 soil-temperature sensor.
- Provides four configurable zones with crop profiles, enabled/disabled state, and automatic/manual mode.
- In automatic mode, starts a zone when its moisture value is below that profile's minimum and stops it after the configured irrigation duration.
- Can operate the sprinkler when air temperature exceeds an enabled zone's crop maximum or humidity is below 40%.
- Exposes sensor, command, configuration, and event characteristics over BLE.
- Stores zone/system configuration in ESP32 EEPROM.

The crop thresholds are starter values in source code, not agronomic recommendations. Calibrate each moisture sensor in the actual growing medium and validate watering durations for the pump, tubing, soil, and crop before enabling unattended irrigation.

## Hardware and pin map

See [`WIRING.md`](WIRING.md) for the full wiring notes.

| Device | ESP32 GPIO | Notes |
|---|---:|---|
| DHT22 air temperature/humidity | 4 | 4.7 kΩ pull-up to 3.3 V per wiring guide |
| Soil moisture zone 1 | 34 | ADC1 |
| Soil moisture zone 2 | 35 | ADC1 |
| Soil moisture zone 3 | 32 | ADC1 |
| Soil moisture zone 4 | 33 | ADC1 |
| DS18B20 soil temperature | 5 | OneWire; 4.7 kΩ pull-up |
| Irrigation valve 1–4 | 16, 17, 18, 19 | Relay inputs |
| Sprinkler relay | 21 | Relay input |
| Pump relay | 22 | Relay input |
| Status LED | 2 | BLE connection indicator |

The wiring guide specifies a 5 V relay supply and a 12 V actuator supply with a suitable power arrangement. Use relay modules and driver components rated for the actual load, add flyback suppression for inductive valves/pumps as appropriate, and keep the ESP32 logic supply separate from actuator current where required.

### First-power safety checklist

1. Keep valves, pump, and sprinkler disconnected from their power supply for the initial firmware check.
2. Verify sensor supply voltages, shared reference/ground requirements, relay input polarity, and the actual relay contact ratings.
3. Power the ESP32 and confirm sensor readings over Serial before connecting actuators.
4. Test each relay with the actuator disconnected. Then test one low-voltage actuator at a time with water contained and a reachable power cutoff.
5. Check that `stop_all` stops the valves, pump, and sprinkler in the expected way before considering unattended use.

The firmware accepts BLE write commands that can switch actuators, and no authentication/access-control mechanism is evident in the current sketch. A nearby compatible BLE client may therefore be able to issue commands. Do not expose this prototype to unattended watering without physical safeguards, leak/overflow protection, and a deliberate security design. Do not switch mains voltage with this project.

## Software prerequisites

- Arduino IDE 2.x and the **ESP32 by Espressif Systems** board package.
- Libraries listed by the project: DHT sensor library, OneWire, DallasTemperature, ArduinoJson, and ESP32 BLE Arduino (availability/version depends on the selected ESP32 core).
- USB data cable and the correct serial port.

## Build and upload

### Arduino IDE

1. Install the ESP32 board package and libraries listed above.
2. Open the root-level `smart_greenhouse.ino`.
3. Select the correct ESP32 board and serial port.
4. Compile and upload via USB.
5. Open Serial Monitor at **115200 baud**. Confirm the boot/configuration messages and check sensor values with actuators still disconnected.

The repository includes a root-level `platformio.ini`, but the sketch is also root-level and the configuration does not set a custom `src_dir`. The default PlatformIO source layout therefore may not discover the sketch. A PlatformIO build was not verified; this documentation pass did not move files or change the configuration.

## BLE interface

The firmware advertises as `GreenHouse-ESP32` and defines one GATT service with these characteristics:

| Characteristic | UUID suffix | Purpose |
|---|---|---|
| Sensor data | `def1` | Read/notify sensor and actuator state |
| Commands | `def2` | Write JSON commands such as `valve`, `pump`, `sprinkler`, `irrigate`, `stop_all`, and `get_sensors` |
| Configuration | `def3` | Read/write zone configuration and system enable state |
| Events | `def4` | Notifications for irrigation/actuator events |

The UUIDs are declared in `smart_greenhouse.ino` and mirrored in `bleConstants.js`. `bleManager.js` is a Web Bluetooth helper module, but the current `index.html` does not import it, so the helper is not proof of an integrated browser-to-firmware control path.

## Calibration and configuration

- The firmware maps raw soil ADC values using `map(raw, 3400, 1200, 0, 100)`. Measure each sensor dry and at the target wet condition, then adjust/calibrate before relying on the displayed percentage.
- DHT22 readings can be invalid if the wiring/pull-up or sampling timing is wrong.
- Default crop profiles and zone behavior are defined in `smart_greenhouse.ino`; initial defaults enable four zones in automatic mode.
- Configuration writes are saved to EEPROM. Treat stored settings as persistent across power cycles and confirm them after any change.

## Repository map

| Path | Purpose |
|---|---|
| `smart_greenhouse.ino` | Firmware, sensors, relay control, BLE GATT service, EEPROM configuration |
| `WIRING.md` | Pin map, wiring and power notes |
| `platformio.ini` | PlatformIO board/dependency settings; source layout needs review |
| `index.html` | Interactive browser simulation, not connected to physical hardware |
| `bleManager.js` | Web Bluetooth helper module |
| `bleConstants.js` | BLE UUIDs and mirrored crop profiles |
| `manifest.json` | PWA manifest referencing icon paths not present in the current tree |
| `sw.js` | Service worker scaffold; asset paths refer to `/src/`, while the JS files are at repository root |
| `deploy.yml` | Pages workflow currently watches and publishes `web/**`, but no `web/` directory exists in the current tree |

Do not infer successful PWA installation or Pages deployment from the presence of these files; their paths need to be reconciled and the result tested.

## Troubleshooting

- **No sensor values:** check the pin map, sensor power, pull-ups, common reference, and Serial Monitor at 115200 baud.
- **Moisture percentage is inverted or implausible:** calibrate dry/wet ADC values for the actual sensor and medium.
- **Relay does not switch:** verify relay input polarity, supply, grounding/isolation arrangement, and load wiring with the actuator disconnected.
- **Unexpected watering:** disable the system or disconnect actuator power, then review the moisture thresholds, zone mode, saved EEPROM settings, and sensor calibration.
- **Browser cannot connect:** the current demo page is not integrated with `bleManager.js`; check the module/path integration before troubleshooting Bluetooth permissions.
- **PWA/Pages assets fail:** the service-worker and deployment paths currently reference directories that are absent from the root tree.

## Verification status

The README was checked against the current firmware, pin/wiring guide, PlatformIO configuration, BLE helper files, manifest, service worker, and deployment workflow. No firmware build, BLE connection test, PWA install test, watering test, or hardware test is claimed.

## License

The repository currently contains no root-level `LICENSE` file in the inspected tree. Confirm and add the intended license before advertising reuse terms.

## Maintenance & audit

- **Audit date:** 13 November 2026
- **Scope:** documentation-only review; source, pins, dependencies, and hardware behavior were not changed.
- **Verification:** compilation, BLE integration, PWA/deployment, and physical watering remain unverified.

## Contact

Maintained up to 25 November 2026.

If you encounter any problems, please contact [carmodbhai@gmail.com](mailto:carmodbhai@gmail.com).
