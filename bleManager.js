import {
  BLE_SERVICE_UUID,
  SENSOR_CHAR_UUID,
  COMMAND_CHAR_UUID,
  CONFIG_CHAR_UUID,
  NOTIFY_CHAR_UUID,
} from "./bleConstants.js";

class BLEManager extends EventTarget {
  constructor() {
    super();
    this.device = null;
    this.server = null;
    this.service = null;
    this.chars = {};
    this.connected = false;
    this._reconnectTimer = null;
  }

  get supported() {
    return "bluetooth" in navigator;
  }

  async connect() {
    if (!this.supported) throw new Error("Web Bluetooth not supported in this browser");

    this.device = await navigator.bluetooth.requestDevice({
      filters: [{ name: "GreenHouse-ESP32" }],
      optionalServices: [BLE_SERVICE_UUID],
    });

    this.device.addEventListener("gattserverdisconnected", () => this._onDisconnect());

    await this._connectGATT();
  }

  async _connectGATT() {
    this.server  = await this.device.gatt.connect();
    this.service = await this.server.getPrimaryService(BLE_SERVICE_UUID);

    this.chars.sensor  = await this.service.getCharacteristic(SENSOR_CHAR_UUID);
    this.chars.command = await this.service.getCharacteristic(COMMAND_CHAR_UUID);
    this.chars.config  = await this.service.getCharacteristic(CONFIG_CHAR_UUID);
    this.chars.notify  = await this.service.getCharacteristic(NOTIFY_CHAR_UUID);

    // Subscribe to sensor updates
    await this.chars.sensor.startNotifications();
    this.chars.sensor.addEventListener("characteristicvaluechanged", (e) =>
      this._onSensorData(e)
    );

    // Subscribe to event notifications
    await this.chars.notify.startNotifications();
    this.chars.notify.addEventListener("characteristicvaluechanged", (e) =>
      this._onNotification(e)
    );

    // Subscribe to config changes
    await this.chars.config.startNotifications();
    this.chars.config.addEventListener("characteristicvaluechanged", (e) =>
      this._onConfigData(e)
    );

    this.connected = true;
    this.dispatchEvent(new Event("connected"));

    // Request initial data
    await this.sendCommand({ cmd: "get_sensors" });
    await this.requestConfig();
  }

  _onDisconnect() {
    this.connected = false;
    this.dispatchEvent(new Event("disconnected"));
    // Auto-reconnect after 3 seconds
    this._reconnectTimer = setTimeout(() => this._reconnect(), 3000);
  }

  async _reconnect() {
    if (this.connected) return;
    try {
      await this._connectGATT();
      this.dispatchEvent(new Event("reconnected"));
    } catch {
      this._reconnectTimer = setTimeout(() => this._reconnect(), 5000);
    }
  }

  async disconnect() {
    if (this._reconnectTimer) clearTimeout(this._reconnectTimer);
    if (this.device?.gatt.connected) this.device.gatt.disconnect();
    this.connected = false;
  }

  _decode(event) {
    const val = event.target.value;
    const dec = new TextDecoder();
    return JSON.parse(dec.decode(val));
  }

  _onSensorData(event) {
    try {
      const data = this._decode(event);
      this.dispatchEvent(new CustomEvent("sensors", { detail: data }));
    } catch (e) { console.warn("Sensor parse error", e); }
  }

  _onNotification(event) {
    try {
      const data = this._decode(event);
      this.dispatchEvent(new CustomEvent("notification", { detail: data }));
    } catch (e) { console.warn("Notification parse error", e); }
  }

  _onConfigData(event) {
    try {
      const data = this._decode(event);
      this.dispatchEvent(new CustomEvent("config", { detail: data }));
    } catch (e) { console.warn("Config parse error", e); }
  }

  async sendCommand(payload) {
    if (!this.connected) throw new Error("Not connected");
    const enc = new TextEncoder();
    await this.chars.command.writeValue(enc.encode(JSON.stringify(payload)));
  }

  async sendConfig(payload) {
    if (!this.connected) throw new Error("Not connected");
    const enc = new TextEncoder();
    await this.chars.config.writeValue(enc.encode(JSON.stringify(payload)));
  }

  async requestConfig() {
    if (!this.connected) return;
    const val = await this.chars.config.readValue();
    const dec = new TextDecoder();
    try {
      const data = JSON.parse(dec.decode(val));
      this.dispatchEvent(new CustomEvent("config", { detail: data }));
    } catch {}
  }

  // ─── Convenience commands ──────────────────────────────────────────────────

  async setValve(zone, state)    { await this.sendCommand({ cmd: "valve", zone, state }); }
  async setSprinkler(state)      { await this.sendCommand({ cmd: "sprinkler", state }); }
  async setPump(state)           { await this.sendCommand({ cmd: "pump", state }); }
  async irrigateZone(zone)       { await this.sendCommand({ cmd: "irrigate", zone }); }
  async stopAll()                { await this.sendCommand({ cmd: "stop_all" }); }
  async setZoneCrop(zone, cropIndex, enabled, autoMode) {
    await this.sendConfig({ zone, cropIndex, enabled, autoMode });
  }
}

export const bleManager = new BLEManager();
