/*
 * ════════════════════════════════════════════════════════════════
 *  ESP8266 — Multi-Sensor Monitor  |  IoT Platform
 *   • Add unlimited sensors in 2 steps •
 * ════════════════════════════════════════════════════════════════
 *
 *  HOW TO ADD A NEW SENSOR:
 *   Step 1 — Write a float function that reads the sensor value
 *   Step 2 — Add one row to the SENSORS[] table below
 *   That's it. No other code needs to change.
 *
 *  BOARD   : NodeMCU 1.0 (ESP-12E Module)
 *  LIBRARIES (Arduino Library Manager):
 *    • ESP8266WiFi        — part of ESP8266 board package
 *    • ESP8266HTTPClient  — part of ESP8266 board package
 *    • ArduinoJson        — by Benoit Blanchon  v6.x
 *
 *  SERIAL MONITOR: 115200 baud
 * ════════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

// ── STEP 0 — Server & credentials ────────────────────────────────
const char* WIFI_SSID     = "Your-SSID";
const char* WIFI_PASSWORD = "Your-Password";
const char* API_KEY       = "Your-API-Key";  // from dashboard → API Key pill

// Production server (HTTPS)
const char* SERVER = "https://iot.getyourprojectdone.in";


// Send interval (milliseconds)
const unsigned long SEND_INTERVAL = 10000;  // 10 seconds
// ─────────────────────────────────────────────────────────────────


// ════════════════════════════════════════════════════════════════
//  STEP 1 — Write one float function per sensor
//  Replace the simulated return values with real sensor reads.
//  See sensor_library.h (accessory file) for copy-paste examples
//  for DHT11, HC-SR04, MQ-2, BMP280, DS18B20, LDR, and more.
// ════════════════════════════════════════════════════════════════

float readTemperature() {
  // TODO: e.g. return dht.readTemperature();
  return 28.5;
}

float readHumidity() {
  // TODO: e.g. return dht.readHumidity();
  return 62.0;
}

float readGas() {
  // TODO: e.g. return analogRead(A0) * (5.0/1023.0) * 100;
  return 350.0;
}

float readDistance() {
  // TODO: e.g. return ultrasonicCm(TRIG_PIN, ECHO_PIN);
  return 120.0;
}

// ════════════════════════════════════════════════════════════════
//  STEP 2 — Register sensors in this table
//  Add or remove rows freely — nothing else needs to change.
//
//  Columns:
//    "device_name" — must EXACTLY match name on Devices page
//    "sensor_type" — temperature | humidity | gas | distance |
//                    pressure | light | ph | co2 | motion | custom
//    "unit"        — C | F | % | ppm | cm | hPa | lux | V | etc.
//    readFunction  — the function you wrote in Step 1
// ════════════════════════════════════════════════════════════════

struct Sensor {
  const char* deviceName;
  const char* type;
  const char* unit;
  float (*readFn)();
};

Sensor SENSORS[] = {
// { "device_name",   "sensor_type",  "unit",  readFunction   }
  { "temp_sensor",   "temperature",  "C",     readTemperature },
  { "hum_sensor",    "humidity",     "%",     readHumidity    },
  { "gas_sensor",    "gas",          "ppm",   readGas         },
  { "ultra_sensor",  "distance",     "cm",    readDistance    },

  // Add more rows here — copy the format above
};

// ─────────────────────────────────────────────────────────────────
// Internals — no changes needed below this line
// ─────────────────────────────────────────────────────────────────

const int SENSOR_COUNT = sizeof(SENSORS) / sizeof(SENSORS[0]);

BearSSL::WiFiClientSecure secureClient;
WiFiClient                plainClient;
unsigned long             lastSend  = 0;
int                       cycleNum  = 0;

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println(F("\n══════════════════════════════════"));
  Serial.println(F("  ESP8266 Multi-Sensor Monitor"));
  Serial.printf( "  Sensors registered : %d\n", SENSOR_COUNT);
  Serial.printf( "  Send interval      : %lu ms\n", SEND_INTERVAL);
  Serial.println(F("══════════════════════════════════\n"));

  secureClient.setInsecure();  // skip SSL cert verification (fine for IoT)
  connectWiFi();

  lastSend = millis() - SEND_INTERVAL;  // send immediately on first loop
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[WiFi] Disconnected — reconnecting..."));
    connectWiFi();
    return;
  }

  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    cycleNum++;
    Serial.printf("\n─── Cycle #%d ───────────────────────\n", cycleNum);
    sendAllSensors();
  }
}

// ── Build JSON and POST all sensors in one request ────────────────
void sendAllSensors() {

  // Read all sensor values first
  float values[SENSOR_COUNT];
  for (int i = 0; i < SENSOR_COUNT; i++) {
    values[i] = SENSORS[i].readFn();
    if (isnan(values[i])) {
      Serial.printf("[Sensor] WARNING: %s returned NaN — skipping\n", SENSORS[i].deviceName);
      values[i] = -999;
    }
    Serial.printf("[Sensor] %-16s = %.2f %s\n",
                  SENSORS[i].deviceName, values[i], SENSORS[i].unit);
  }

  // Build JSON: { "api_key":"...", "devices":[{...},{...},...] }
  DynamicJsonDocument doc(512 + SENSOR_COUNT * 120);
  doc["api_key"] = API_KEY;

  JsonArray devices = doc.createNestedArray("devices");
  for (int i = 0; i < SENSOR_COUNT; i++) {
    if (values[i] == -999) continue;  // skip failed reads

    JsonObject device  = devices.createNestedObject();
    device["device_name"] = SENSORS[i].deviceName;

    JsonArray  sensors = device.createNestedArray("sensors");
    JsonObject sensor  = sensors.createNestedObject();
    sensor["type"]  = SENSORS[i].type;
    sensor["value"] = String(values[i], 2);  // 2 decimal places
    sensor["unit"]  = SENSORS[i].unit;
  }

  String payload;
  serializeJson(doc, payload);

  // POST request
  String url = String(SERVER) + "/api/insert_data.php";
  String resp = httpPost(url, payload);

  if (resp.isEmpty()) {
    Serial.println(F("[HTTP] No response — check SERVER and network"));
    return;
  }

  // Parse response
  StaticJsonDocument<256> rdoc;
  if (deserializeJson(rdoc, resp) == DeserializationError::Ok) {
    const char* status = rdoc["status"] | "error";
    const char* msg    = rdoc["message"] | "";
    if (strcmp(status, "success") == 0) {
      Serial.printf("[Server] OK — %s\n", msg);
    } else {
      Serial.printf("[Server] ERROR — %s\n", msg);
      if (rdoc.containsKey("errors")) {
        for (const char* e : rdoc["errors"].as<JsonArray>())
          Serial.printf("         • %s\n", e);
      }
    }
  } else {
    Serial.printf("[Server] Raw: %s\n", resp.c_str());
  }
}

// ── HTTP POST helper ──────────────────────────────────────────────
String httpPost(const String& url, const String& body) {
  bool isHttps = url.startsWith("https");
  HTTPClient http;

  if (isHttps) {
    http.begin(secureClient, url);
  } else {
    http.begin(plainClient, url);
  }

  http.addHeader(F("Content-Type"), F("application/json"));
  http.setTimeout(10000);

  int code = http.POST(body);
  String response = "";

  if (code > 0) {
    response = http.getString();
    if (code != 200)
      Serial.printf("[HTTP] Code: %d\n", code);
  } else {
    Serial.printf("[HTTP] Failed: %s\n", http.errorToString(code).c_str());
  }

  http.end();
  return response;
}

// ── WiFi ──────────────────────────────────────────────────────────
void connectWiFi() {
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 40) {
    delay(500); Serial.print('.'); t++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F(" connected!"));
    Serial.printf("[WiFi] IP: %s  Signal: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println(F(" FAILED — check SSID/password"));
  }
}
