/**
 * Smart Greenhouse Irrigation System
 * ESP32 Firmware with BLE + Sensor Integration
 *
 * Sensors:
 *   - DHT22     : Temperature & Humidity (GPIO 4)
 *   - Capacitive Soil Moisture x4 (GPIO 34, 35, 32, 33)
 *   - DS18B20   : Soil Temperature (GPIO 5)
 *   - FC-28     : Analog moisture backup (ADC)
 *
 * Actuators:
 *   - Irrigation solenoid valves x4 (GPIO 16,17,18,19)
 *   - Sprinkler relay (GPIO 21)
 *   - Water pump relay (GPIO 22)
 *   - Status LED (GPIO 2)
 *
 * Communication:
 *   - BLE (Bluetooth Low Energy) for offline monitoring
 *   - Serial debug @ 115200
 *
 * Author: Smart Greenhouse Project
 * License: MIT
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// ─── Pin Definitions ────────────────────────────────────────────────────────

#define DHT_PIN         4
#define DHT_TYPE        DHT22

#define SOIL_SENSOR_1   34   // Crop zone 1 moisture
#define SOIL_SENSOR_2   35   // Crop zone 2 moisture
#define SOIL_SENSOR_3   32   // Crop zone 3 moisture
#define SOIL_SENSOR_4   33   // Crop zone 4 moisture

#define DS18B20_PIN     5    // Soil temp (OneWire)

#define VALVE_1_PIN     16   // Zone 1 irrigation valve
#define VALVE_2_PIN     17   // Zone 2 irrigation valve
#define VALVE_3_PIN     18   // Zone 3 irrigation valve
#define VALVE_4_PIN     19   // Zone 4 irrigation valve

#define SPRINKLER_PIN   21   // Sprinkler relay (temp control)
#define PUMP_PIN        22   // Main water pump
#define STATUS_LED      2    // Onboard LED

// ─── BLE UUIDs ──────────────────────────────────────────────────────────────

#define SERVICE_UUID          "12345678-1234-5678-1234-56789abcdef0"
#define SENSOR_CHAR_UUID      "12345678-1234-5678-1234-56789abcdef1"
#define COMMAND_CHAR_UUID     "12345678-1234-5678-1234-56789abcdef2"
#define CONFIG_CHAR_UUID      "12345678-1234-5678-1234-56789abcdef3"
#define NOTIFY_CHAR_UUID      "12345678-1234-5678-1234-56789abcdef4"

// ─── EEPROM Addresses ────────────────────────────────────────────────────────

#define EEPROM_SIZE         256
#define EEPROM_MAGIC        0xAB
#define EEPROM_MAGIC_ADDR   0
#define EEPROM_CONFIG_ADDR  1

// ─── Crop Profiles ───────────────────────────────────────────────────────────

struct CropProfile {
  char name[20];
  uint8_t moistureMin;    // % - start irrigation below this
  uint8_t moistureMax;    // % - stop irrigation above this
  float   tempMin;        // °C - activate sprinkler below this
  float   tempMax;        // °C - activate sprinkler above this
  uint16_t irrigationDuration;  // seconds per cycle
  uint16_t irrigationInterval;  // minutes between checks
};

const CropProfile cropProfiles[] = {
  // name,         moistMin, moistMax, tempMin, tempMax, duration, interval
  {"Tomato",          40,      70,      18.0,   30.0,    60,      30  },
  {"Lettuce",         50,      80,      10.0,   24.0,    45,      20  },
  {"Pepper",          35,      65,      20.0,   32.0,    90,      40  },
  {"Cucumber",        45,      75,      18.0,   28.0,    75,      25  },
  {"Basil",           40,      70,      16.0,   26.0,    50,      30  },
  {"Spinach",         55,      85,      8.0,    22.0,    40,      20  },
  {"Strawberry",      45,      70,      15.0,   25.0,    55,      35  },
  {"Custom",          40,      70,      15.0,   30.0,    60,      30  }
};
const uint8_t NUM_PROFILES = sizeof(cropProfiles) / sizeof(CropProfile);

// ─── Zone Configuration ──────────────────────────────────────────────────────

struct ZoneConfig {
  uint8_t cropIndex;    // Index into cropProfiles[]
  bool    enabled;
  bool    autoMode;     // true = auto, false = manual
};

struct SystemConfig {
  ZoneConfig zones[4];
  bool       systemEnabled;
  uint8_t    customMoistureMin;
  uint8_t    customMoistureMax;
  float      customTempMin;
  float      customTempMax;
};

SystemConfig sysConfig;

// ─── Sensor Data ─────────────────────────────────────────────────────────────

struct SensorData {
  float   airTemp;
  float   humidity;
  uint8_t soilMoisture[4];  // 0-100 %
  float   soilTemp;
  bool    valveState[4];
  bool    sprinklerState;
  bool    pumpState;
  uint32_t timestamp;
};

SensorData sensorData;

// ─── Objects ─────────────────────────────────────────────────────────────────

DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);

BLEServer*         bleServer    = nullptr;
BLECharacteristic* sensorChar   = nullptr;
BLECharacteristic* commandChar  = nullptr;
BLECharacteristic* configChar   = nullptr;
BLECharacteristic* notifyChar   = nullptr;
bool               bleConnected = false;

// ─── Timers ──────────────────────────────────────────────────────────────────

unsigned long lastSensorRead     = 0;
unsigned long lastIrrigationCheck = 0;
unsigned long irrigationStartTime[4] = {0, 0, 0, 0};
bool          irrigating[4]      = {false, false, false, false};

#define SENSOR_READ_INTERVAL     2000   // ms
#define IRRIGATION_CHECK_INTERVAL 60000 // ms

// ─── BLE Callbacks ───────────────────────────────────────────────────────────

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    bleConnected = true;
    digitalWrite(STATUS_LED, HIGH);
    Serial.println("[BLE] Client connected");
    bleServer->getAdvertising()->stop();
  }
  void onDisconnect(BLEServer* server) override {
    bleConnected = false;
    digitalWrite(STATUS_LED, LOW);
    Serial.println("[BLE] Client disconnected – restarting advertising");
    delay(500);
    bleServer->getAdvertising()->start();
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* chr) override {
    std::string val = chr->getValue();
    if (val.empty()) return;

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, val.c_str());
    if (err) { Serial.println("[CMD] JSON parse error"); return; }

    const char* cmd = doc["cmd"];
    if (!cmd) return;

    // Manual valve control: {"cmd":"valve","zone":0,"state":true}
    if (strcmp(cmd, "valve") == 0) {
      int zone = doc["zone"] | -1;
      bool state = doc["state"] | false;
      if (zone >= 0 && zone < 4) {
        setValve(zone, state);
        sendNotification("valve", zone, state);
      }
    }
    // Sprinkler control: {"cmd":"sprinkler","state":true}
    else if (strcmp(cmd, "sprinkler") == 0) {
      bool state = doc["state"] | false;
      setSprinkler(state);
      sendNotification("sprinkler", -1, state);
    }
    // Pump control: {"cmd":"pump","state":true}
    else if (strcmp(cmd, "pump") == 0) {
      bool state = doc["state"] | false;
      setPump(state);
    }
    // Force irrigation cycle on zone: {"cmd":"irrigate","zone":0}
    else if (strcmp(cmd, "irrigate") == 0) {
      int zone = doc["zone"] | -1;
      if (zone >= 0 && zone < 4) startIrrigation(zone);
    }
    // Stop all: {"cmd":"stop_all"}
    else if (strcmp(cmd, "stop_all") == 0) {
      stopAllValves();
      setSprinkler(false);
    }
    // Request sensor data: {"cmd":"get_sensors"}
    else if (strcmp(cmd, "get_sensors") == 0) {
      publishSensorData();
    }

    Serial.printf("[CMD] Received: %s\n", cmd);
  }
};

class ConfigCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* chr) override {
    std::string val = chr->getValue();
    if (val.empty()) return;

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, val.c_str());
    if (err) { Serial.println("[CFG] JSON parse error"); return; }

    // Update zone crop assignment: {"zone":0,"cropIndex":2,"enabled":true,"autoMode":true}
    if (doc.containsKey("zone")) {
      int z = doc["zone"];
      if (z >= 0 && z < 4) {
        sysConfig.zones[z].cropIndex = doc["cropIndex"] | sysConfig.zones[z].cropIndex;
        sysConfig.zones[z].enabled   = doc["enabled"]   | sysConfig.zones[z].enabled;
        sysConfig.zones[z].autoMode  = doc["autoMode"]  | sysConfig.zones[z].autoMode;
        Serial.printf("[CFG] Zone %d → crop: %s\n", z, cropProfiles[sysConfig.zones[z].cropIndex].name);
      }
    }
    // System enable/disable: {"systemEnabled":true}
    if (doc.containsKey("systemEnabled")) {
      sysConfig.systemEnabled = doc["systemEnabled"];
    }
    saveConfig();
    publishConfig();
  }
};

// ─── Sensor Reading ──────────────────────────────────────────────────────────

uint8_t readMoisture(uint8_t pin) {
  // Capacitive sensor: ~3400 dry, ~1200 wet (12-bit ADC)
  int raw = analogRead(pin);
  int pct = map(raw, 3400, 1200, 0, 100);
  return (uint8_t)constrain(pct, 0, 100);
}

void readSensors() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(h) && !isnan(t)) {
    sensorData.airTemp  = t;
    sensorData.humidity = h;
  }

  ds18b20.requestTemperatures();
  float soilT = ds18b20.getTempCByIndex(0);
  if (soilT != DEVICE_DISCONNECTED_C) {
    sensorData.soilTemp = soilT;
  }

  sensorData.soilMoisture[0] = readMoisture(SOIL_SENSOR_1);
  sensorData.soilMoisture[1] = readMoisture(SOIL_SENSOR_2);
  sensorData.soilMoisture[2] = readMoisture(SOIL_SENSOR_3);
  sensorData.soilMoisture[3] = readMoisture(SOIL_SENSOR_4);

  sensorData.timestamp = millis();

  Serial.printf("[SEN] Air: %.1f°C  Hum: %.1f%%  Soil: %d %d %d %d  SoilT: %.1f°C\n",
    sensorData.airTemp, sensorData.humidity,
    sensorData.soilMoisture[0], sensorData.soilMoisture[1],
    sensorData.soilMoisture[2], sensorData.soilMoisture[3],
    sensorData.soilTemp);
}

// ─── Actuator Control ────────────────────────────────────────────────────────

const uint8_t VALVE_PINS[] = {VALVE_1_PIN, VALVE_2_PIN, VALVE_3_PIN, VALVE_4_PIN};

void setValve(uint8_t zone, bool state) {
  digitalWrite(VALVE_PINS[zone], state ? HIGH : LOW);
  sensorData.valveState[zone] = state;
  Serial.printf("[ACT] Valve %d → %s\n", zone+1, state ? "OPEN" : "CLOSED");
}

void setPump(bool state) {
  digitalWrite(PUMP_PIN, state ? HIGH : LOW);
  sensorData.pumpState = state;
  Serial.printf("[ACT] Pump → %s\n", state ? "ON" : "OFF");
}

void setSprinkler(bool state) {
  digitalWrite(SPRINKLER_PIN, state ? HIGH : LOW);
  sensorData.sprinklerState = state;
  Serial.printf("[ACT] Sprinkler → %s\n", state ? "ON" : "OFF");
}

void stopAllValves() {
  for (int i = 0; i < 4; i++) setValve(i, false);
  setPump(false);
  memset(irrigating, false, sizeof(irrigating));
}

void startIrrigation(uint8_t zone) {
  if (irrigating[zone]) return;
  irrigating[zone] = true;
  irrigationStartTime[zone] = millis();
  setValve(zone, true);
  setPump(true);
  Serial.printf("[IRR] Zone %d irrigation started\n", zone+1);
}

void stopIrrigation(uint8_t zone) {
  irrigating[zone] = false;
  setValve(zone, false);
  // Check if any other zone still irrigating before stopping pump
  bool anyActive = false;
  for (int i = 0; i < 4; i++) if (irrigating[i]) { anyActive = true; break; }
  if (!anyActive) setPump(false);
  Serial.printf("[IRR] Zone %d irrigation stopped\n", zone+1);
}

// ─── Irrigation Logic ────────────────────────────────────────────────────────

void runIrrigationLogic() {
  if (!sysConfig.systemEnabled) return;

  for (uint8_t z = 0; z < 4; z++) {
    const ZoneConfig& zc = sysConfig.zones[z];
    if (!zc.enabled || !zc.autoMode) continue;

    const CropProfile& crop = cropProfiles[zc.cropIndex];

    // Stop irrigation if duration exceeded
    if (irrigating[z]) {
      unsigned long elapsed = (millis() - irrigationStartTime[z]) / 1000UL;
      if (elapsed >= crop.irrigationDuration) {
        stopIrrigation(z);
        sendNotification("irrigation_stop", z, false);
      }
      continue;
    }

    // Start irrigation if moisture below threshold
    uint8_t moisture = sensorData.soilMoisture[z];
    if (moisture < crop.moistureMin) {
      startIrrigation(z);
      sendNotification("irrigation_start", z, true);
    }
  }

  // Sprinkler: activate if air temp exceeds any zone's max temp
  bool needSprinkler = false;
  for (uint8_t z = 0; z < 4; z++) {
    if (!sysConfig.zones[z].enabled) continue;
    const CropProfile& crop = cropProfiles[sysConfig.zones[z].cropIndex];
    if (sensorData.airTemp > crop.tempMax || sensorData.humidity < 40.0f) {
      needSprinkler = true;
      break;
    }
  }
  if (needSprinkler != sensorData.sprinklerState) {
    setSprinkler(needSprinkler);
    sendNotification("sprinkler", -1, needSprinkler);
  }
}

// ─── BLE Publishing ──────────────────────────────────────────────────────────

void publishSensorData() {
  if (!bleConnected) return;

  StaticJsonDocument<512> doc;
  doc["type"]    = "sensors";
  doc["airTemp"] = sensorData.airTemp;
  doc["humidity"]= sensorData.humidity;
  doc["soilTemp"]= sensorData.soilTemp;

  JsonArray m = doc.createNestedArray("moisture");
  for (int i = 0; i < 4; i++) m.add(sensorData.soilMoisture[i]);

  JsonArray v = doc.createNestedArray("valves");
  for (int i = 0; i < 4; i++) v.add(sensorData.valveState[i]);

  doc["sprinkler"] = sensorData.sprinklerState;
  doc["pump"]      = sensorData.pumpState;
  doc["ts"]        = sensorData.timestamp;

  char buf[512];
  serializeJson(doc, buf);
  sensorChar->setValue(buf);
  sensorChar->notify();
}

void publishConfig() {
  if (!bleConnected) return;

  StaticJsonDocument<512> doc;
  doc["type"] = "config";
  doc["sysEnabled"] = sysConfig.systemEnabled;

  JsonArray zones = doc.createNestedArray("zones");
  for (int i = 0; i < 4; i++) {
    JsonObject z = zones.createNestedObject();
    z["crop"]     = cropProfiles[sysConfig.zones[i].cropIndex].name;
    z["cropIdx"]  = sysConfig.zones[i].cropIndex;
    z["enabled"]  = sysConfig.zones[i].enabled;
    z["autoMode"] = sysConfig.zones[i].autoMode;
  }

  JsonArray profiles = doc.createNestedArray("profiles");
  for (int i = 0; i < NUM_PROFILES; i++) {
    JsonObject p = profiles.createNestedObject();
    p["name"]     = cropProfiles[i].name;
    p["moistMin"] = cropProfiles[i].moistureMin;
    p["moistMax"] = cropProfiles[i].moistureMax;
    p["tempMin"]  = cropProfiles[i].tempMin;
    p["tempMax"]  = cropProfiles[i].tempMax;
  }

  char buf[512];
  serializeJson(doc, buf);
  configChar->setValue(buf);
  configChar->notify();
}

void sendNotification(const char* event, int zone, bool state) {
  if (!bleConnected) return;

  StaticJsonDocument<128> doc;
  doc["type"]  = "event";
  doc["event"] = event;
  if (zone >= 0) doc["zone"] = zone;
  doc["state"] = state;
  doc["ts"]    = millis();

  char buf[128];
  serializeJson(doc, buf);
  notifyChar->setValue(buf);
  notifyChar->notify();
}

// ─── EEPROM Config ───────────────────────────────────────────────────────────

void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC) {
    // Default config
    sysConfig.systemEnabled = true;
    for (int i = 0; i < 4; i++) {
      sysConfig.zones[i] = {0, true, true};  // Tomato, enabled, auto
    }
    saveConfig();
    Serial.println("[CFG] Loaded defaults");
  } else {
    EEPROM.get(EEPROM_CONFIG_ADDR, sysConfig);
    Serial.println("[CFG] Loaded from EEPROM");
  }
}

void saveConfig() {
  EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  EEPROM.put(EEPROM_CONFIG_ADDR, sysConfig);
  EEPROM.commit();
  Serial.println("[CFG] Saved to EEPROM");
}

// ─── BLE Setup ───────────────────────────────────────────────────────────────

void setupBLE() {
  BLEDevice::init("GreenHouse-ESP32");
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService* service = bleServer->createService(SERVICE_UUID);

  // Sensor data (read + notify)
  sensorChar = service->createCharacteristic(
    SENSOR_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  sensorChar->addDescriptor(new BLE2902());

  // Command (write)
  commandChar = service->createCharacteristic(
    COMMAND_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  commandChar->setCallbacks(new CommandCallbacks());

  // Config (read + write + notify)
  configChar = service->createCharacteristic(
    CONFIG_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  configChar->setCallbacks(new ConfigCallbacks());
  configChar->addDescriptor(new BLE2902());

  // Notifications / events
  notifyChar = service->createCharacteristic(
    NOTIFY_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  notifyChar->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  BLEDevice::startAdvertising();

  Serial.println("[BLE] Advertising as 'GreenHouse-ESP32'");
}

// ─── Setup & Loop ────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Smart Greenhouse Irrigation System ===");

  pinMode(STATUS_LED, OUTPUT);
  pinMode(VALVE_1_PIN, OUTPUT);
  pinMode(VALVE_2_PIN, OUTPUT);
  pinMode(VALVE_3_PIN, OUTPUT);
  pinMode(VALVE_4_PIN, OUTPUT);
  pinMode(SPRINKLER_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  // All actuators OFF at startup
  stopAllValves();
  setSprinkler(false);

  dht.begin();
  ds18b20.begin();
  loadConfig();
  setupBLE();

  // Blink to indicate ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED, HIGH); delay(200);
    digitalWrite(STATUS_LED, LOW);  delay(200);
  }

  Serial.println("[SYS] Ready!");
}

void loop() {
  unsigned long now = millis();

  // Read sensors periodically
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    readSensors();
    publishSensorData();
  }

  // Run irrigation logic periodically
  if (now - lastIrrigationCheck >= IRRIGATION_CHECK_INTERVAL) {
    lastIrrigationCheck = now;
    runIrrigationLogic();
  }

  // Check irrigation timeouts every second
  static unsigned long lastTimeoutCheck = 0;
  if (now - lastTimeoutCheck >= 1000) {
    lastTimeoutCheck = now;
    for (uint8_t z = 0; z < 4; z++) {
      if (irrigating[z]) {
        const CropProfile& crop = cropProfiles[sysConfig.zones[z].cropIndex];
        unsigned long elapsed = (now - irrigationStartTime[z]) / 1000UL;
        if (elapsed >= crop.irrigationDuration) {
          stopIrrigation(z);
        }
      }
    }
  }
}
