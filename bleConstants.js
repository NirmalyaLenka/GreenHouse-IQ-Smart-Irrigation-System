// BLE UUIDs - must match firmware
export const BLE_SERVICE_UUID    = "12345678-1234-5678-1234-56789abcdef0";
export const SENSOR_CHAR_UUID    = "12345678-1234-5678-1234-56789abcdef1";
export const COMMAND_CHAR_UUID   = "12345678-1234-5678-1234-56789abcdef2";
export const CONFIG_CHAR_UUID    = "12345678-1234-5678-1234-56789abcdef3";
export const NOTIFY_CHAR_UUID    = "12345678-1234-5678-1234-56789abcdef4";

export const CROP_PROFILES = [
  { name: "Tomato",     moistMin: 40, moistMax: 70, tempMin: 18, tempMax: 30, duration: 60,  interval: 30 },
  { name: "Lettuce",    moistMin: 50, moistMax: 80, tempMin: 10, tempMax: 24, duration: 45,  interval: 20 },
  { name: "Pepper",     moistMin: 35, moistMax: 65, tempMin: 20, tempMax: 32, duration: 90,  interval: 40 },
  { name: "Cucumber",   moistMin: 45, moistMax: 75, tempMin: 18, tempMax: 28, duration: 75,  interval: 25 },
  { name: "Basil",      moistMin: 40, moistMax: 70, tempMin: 16, tempMax: 26, duration: 50,  interval: 30 },
  { name: "Spinach",    moistMin: 55, moistMax: 85, tempMin: 8,  tempMax: 22, duration: 40,  interval: 20 },
  { name: "Strawberry", moistMin: 45, moistMax: 70, tempMin: 15, tempMax: 25, duration: 55,  interval: 35 },
  { name: "Custom",     moistMin: 40, moistMax: 70, tempMin: 15, tempMax: 30, duration: 60,  interval: 30 },
];
