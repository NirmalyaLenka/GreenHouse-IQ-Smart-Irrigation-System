# 🌿 GreenHouse IQ — Smart Irrigation System

A complete ESP32-based smart greenhouse irrigation system with BLE connectivity, multi-crop profiles, automated moisture/temperature control, and a Progressive Web App (PWA) dashboard for real-time monitoring — works fully **offline** via Bluetooth Low Energy.

---

## ✨ Features

- **Multi-zone irrigation** — 4 independent zones, each switchable to different crops
- **8 crop profiles** — Tomato, Lettuce, Pepper, Cucumber, Basil, Spinach, Strawberry, Custom
- **Automatic decisions based on:**
  - Soil moisture (capacitive sensors)
  - Air temperature & humidity (DHT22)
  - Soil temperature (DS18B20)
- **Sprinkler system** for temperature & humidity control
- **BLE (Bluetooth Low Energy)** communication — no Wi-Fi needed
- **PWA web dashboard** — installable, works offline, real-time monitoring
- **EEPROM config persistence** — settings survive power cycles
- **Auto/Manual mode** per zone
- **Event log** with timestamped entries

---

## 🔧 Hardware

See [`hardware/WIRING.md`](hardware/WIRING.md) for full wiring diagrams and BOM.

**Key components:**
- ESP32 DevKit v1
- DHT22 (Air Temp + Humidity)
- 4× Capacitive Soil Moisture Sensors
- DS18B20 (Soil Temperature, OneWire)
- 4× 12V Solenoid Irrigation Valves
- 1× Mist Sprinkler
- 1× Water Pump
- 2× 4-Channel Opto-Isolated Relay Modules

---

## 📁 Project Structure

```
smart-greenhouse/
├── firmware/
│   ├── smart_greenhouse.ino    # Main ESP32 sketch
│   └── platformio.ini          # PlatformIO configuration
├── web/
│   ├── index.html              # PWA dashboard
│   ├── manifest.json           # PWA manifest
│   ├── sw.js                   # Service worker (offline)
│   └── src/
│       ├── bleManager.js       # Web Bluetooth API layer
│       └── bleConstants.js     # Shared UUIDs & crop profiles
├── hardware/
│   └── WIRING.md               # Pin mapping & wiring guide
└── README.md
```

---

## 🚀 Getting Started

### 1. Flash the ESP32

**Option A — PlatformIO (recommended):**
```bash
cd firmware/
pio run --target upload
pio device monitor   # 115200 baud
```

**Option B — Arduino IDE:**
1. Install ESP32 board support
2. Install libraries: DHT, OneWire, DallasTemperature, ArduinoJson, ESP32 BLE Arduino
3. Open `smart_greenhouse.ino` and upload

### 2. Deploy the Web Dashboard

**Option A — Local file server (simplest):**
```bash
cd web/
npx serve .         # or python3 -m http.server 8080
```
Then open `http://localhost:8080` in Chrome/Edge.

**Option B — GitHub Pages:**
- Push the `web/` folder contents to a GitHub Pages branch
- Access at `https://yourusername.github.io/smart-greenhouse/`

**Option C — Any static host** (Netlify, Vercel, Cloudflare Pages)

> ⚠️ Web Bluetooth requires **HTTPS** (or localhost). Chrome/Edge only.

### 3. Connect via BLE

1. Power on the ESP32
2. Open the dashboard in Chrome/Edge
3. Click **Connect BLE** — select `GreenHouse-ESP32`
4. Live data streams immediately

---

## 🌱 Crop Profiles

| Crop       | Moisture Range | Temp Range  | Irrigation |
|-----------|----------------|-------------|------------|
| Tomato     | 40–70%         | 18–30°C     | 60s / 30min|
| Lettuce    | 50–80%         | 10–24°C     | 45s / 20min|
| Pepper     | 35–65%         | 20–32°C     | 90s / 40min|
| Cucumber   | 45–75%         | 18–28°C     | 75s / 25min|
| Basil      | 40–70%         | 16–26°C     | 50s / 30min|
| Spinach    | 55–85%         | 8–22°C      | 40s / 20min|
| Strawberry | 45–70%         | 15–25°C     | 55s / 35min|
| Custom     | Configurable   | Configurable| Configurable|

---

## 📡 BLE Protocol

The ESP32 exposes a single GATT service with 4 characteristics:

| Characteristic | UUID suffix | Properties        | Description              |
|---------------|-------------|-------------------|--------------------------|
| Sensor Data   | `...def1`   | Read + Notify     | JSON sensor readings      |
| Commands      | `...def2`   | Write             | Control valves/pump/sprinkler |
| Config        | `...def3`   | Read+Write+Notify | Zone/crop configuration   |
| Notifications | `...def4`   | Notify            | System events             |

### Command examples
```json
// Irrigate zone 1
{ "cmd": "irrigate", "zone": 0 }

// Manual valve control
{ "cmd": "valve", "zone": 1, "state": true }

// Sprinkler on
{ "cmd": "sprinkler", "state": true }

// Emergency stop
{ "cmd": "stop_all" }
```

### Config example
```json
// Set Zone 2 to Lettuce, enabled, auto mode
{ "zone": 1, "cropIndex": 1, "enabled": true, "autoMode": true }
```

---

## 🌐 PWA Offline Use

The dashboard is a **Progressive Web App**:
- Install it on your phone/tablet from Chrome → "Add to Home Screen"
- The UI loads fully offline (cached by service worker)
- BLE connection via phone Bluetooth — no internet needed
- Last sensor readings are stored in memory during the session

---

## 📈 Sensor Calibration

### Capacitive Soil Moisture
Adjust these values in `smart_greenhouse.ino` based on your sensors:
```cpp
// In readMoisture() function:
int pct = map(raw, 3400, 1200, 0, 100);
//              ^dry  ^wet
```
Calibrate by reading `analogRead()` when sensor is dry vs. submerged.

### DHT22
No calibration needed. If readings are `nan`, check:
- 4.7kΩ pull-up resistor on DATA pin
- 2-second minimum between reads (handled in firmware)

---

## 🔒 License

MIT License — free to use, modify, and distribute.

---

## 🤝 Contributing

Pull requests welcome! Please open an issue first for major changes.
