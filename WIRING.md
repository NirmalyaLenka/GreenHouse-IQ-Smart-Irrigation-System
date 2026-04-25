# Smart Greenhouse Irrigation System
## Hardware Wiring Guide

---

## ESP32 Pin Mapping

### Sensors

| Sensor         | Model      | ESP32 Pin | Notes                              |
|---------------|------------|-----------|------------------------------------|
| Air Temp/Hum   | DHT22      | GPIO 4    | 4.7kΩ pull-up to 3.3V              |
| Soil Moisture 1| Capacitive | GPIO 34   | ADC1_CH6 — 3.3V rail              |
| Soil Moisture 2| Capacitive | GPIO 35   | ADC1_CH7 — 3.3V rail              |
| Soil Moisture 3| Capacitive | GPIO 32   | ADC1_CH4 — 3.3V rail              |
| Soil Moisture 4| Capacitive | GPIO 33   | ADC1_CH5 — 3.3V rail              |
| Soil Temp      | DS18B20    | GPIO 5    | 4.7kΩ pull-up to 3.3V, OneWire     |

### Actuators

| Actuator           | GPIO | Relay Type | Notes                            |
|-------------------|------|------------|----------------------------------|
| Irrigation Valve 1 | 16   | 5V relay   | Zone 1 solenoid valve             |
| Irrigation Valve 2 | 17   | 5V relay   | Zone 2 solenoid valve             |
| Irrigation Valve 3 | 18   | 5V relay   | Zone 3 solenoid valve             |
| Irrigation Valve 4 | 19   | 5V relay   | Zone 4 solenoid valve             |
| Sprinkler          | 21   | 5V relay   | Mist/cooling sprinkler            |
| Water Pump         | 22   | 5V relay   | Main pump — powers all zones     |
| Status LED         | 2    | Onboard    | BLE conn indicator (built-in LED)|

---

## Wiring Diagrams

### DHT22 (Air Temperature & Humidity)
```
DHT22 Pin 1 (VCC)  → ESP32 3.3V
DHT22 Pin 2 (DATA) → ESP32 GPIO 4  → 4.7kΩ → 3.3V
DHT22 Pin 4 (GND)  → ESP32 GND
```

### Capacitive Soil Moisture Sensor (×4)
```
Sensor VCC  → ESP32 3.3V
Sensor GND  → ESP32 GND
Sensor AOUT → ESP32 GPIO 34 / 35 / 32 / 33
Note: Calibrate in code: ~3400 = dry, ~1200 = fully saturated
```

### DS18B20 (Soil Temperature)
```
DS18B20 VCC  → ESP32 3.3V
DS18B20 GND  → ESP32 GND
DS18B20 DATA → ESP32 GPIO 5 → 4.7kΩ → 3.3V
Multiple sensors can share the same pin (OneWire bus)
```

### Relay Module (for valves, pump, sprinkler)
```
Relay VCC → External 5V supply
Relay GND → Common GND (shared with ESP32)
Relay IN  → ESP32 GPIO (16/17/18/19/21/22)
Relay COM → Power supply (+ve)
Relay NO  → Actuator (valve/pump/sprinkler)
Relay NC  → Not connected

Note: Use optocoupler relay modules to protect ESP32 from back-EMF.
```

### Solenoid Valve Wiring
```
12V Supply (+) → Relay COM
Relay NO       → Solenoid Valve (+)
Solenoid Valve (−) → 12V Supply (−) / GND
```

---

## Power Supply

| Component        | Voltage | Current (each) |
|-----------------|---------|----------------|
| ESP32            | 3.3V    | ~240mA peak    |
| DHT22            | 3.3V    | 2.5mA          |
| Soil sensors ×4  | 3.3V    | 5mA each       |
| DS18B20          | 3.3V    | 1mA            |
| Relay modules ×6 | 5V      | 20mA each      |
| Solenoid valves ×5| 12V    | 300–500mA each |
| Water pump       | 12V     | 1–5A           |

**Recommended: Use a 12V/5A switching PSU + buck converters for 5V and 3.3V rails.**

---

## Schematic Notes

- Use flyback diodes (1N4007) across all solenoid valves to suppress voltage spikes
- Use opto-isolated relay modules to protect the ESP32 GPIO pins
- Add 100µF capacitor on DHT22 VCC pin to stabilize readings
- Keep sensor wiring away from relay/actuator power wiring to avoid interference
- Use twisted pair or shielded cable for soil sensors if wiring is long (>1m)

---

## BOM (Bill of Materials)

| Item                          | Qty | Approx Cost |
|------------------------------|-----|-------------|
| ESP32 Dev Board               | 1   | $5–8        |
| DHT22 Sensor                  | 1   | $3–5        |
| Capacitive Soil Moisture Sensor| 4  | $2–3 each   |
| DS18B20 Soil Temp Sensor       | 1   | $3–4        |
| 5V Opto-Isolated Relay (4-ch)  | 2   | $4–6 each   |
| 12V Solenoid Valve (N/C type)  | 4   | $5–10 each  |
| 12V Mist Sprinkler             | 1   | $8–15       |
| 12V Water Pump (submersible)   | 1   | $10–20      |
| 12V 5A Power Supply            | 1   | $10–15      |
| 5V Buck Converter              | 1   | $2–3        |
| Jumper wires, PCB, enclosure   | —   | $10–15      |
| **Total (approximate)**        |     | **$80–140** |
