/*
 * ════════════════════════════════════════════════════════════════
 *  Arduino Uno/Mega + ESP8266 Module — Multi-Sensor Monitor
 *   • Add unlimited sensors in 2 steps •
 * ════════════════════════════════════════════════════════════════
 *
 *  WIRING (ESP8266 as serial WiFi via AT commands):
 *   Arduino D2  → ESP8266 TX
 *   Arduino D3  → ESP8266 RX  (use 3.3 V level-shifter)
 *   ESP8266 VCC → 3.3 V (NOT 5 V — will damage module)
 *   ESP8266 GND → GND
 *   ESP8266 CH_EN → 3.3 V
 *
 *  HOW TO ADD A NEW SENSOR:
 *   Step 1 — Write a float function that reads the sensor value
 *   Step 2 — Add one row to the SENSORS[] table below
 *
 *  LIBRARY: WiFiEsp by bportaluri (Arduino Library Manager)
 *  SERIAL MONITOR: 9600 baud
 * ════════════════════════════════════════════════════════════════
 */

#include <WiFiEsp.h>
#include <SoftwareSerial.h>

// ── STEP 0 — Credentials & server ────────────────────────────────
const char* WIFI_SSID     = "Your-SSID";
const char* WIFI_PASSWORD = "Your-Password";
const char* API_KEY       = "Your-API-Key";

// Production server
const char* SERVER_HOST = "iot.getyourprojectdone.in";
const int   SERVER_PORT = 443;          // 443 = HTTPS/SSL


// API path on the server
const char* API_PATH = "/api/insert_data.php";

const unsigned long SEND_INTERVAL = 15000;  // 15 seconds (AT commands are slower)
// ─────────────────────────────────────────────────────────────────


// ════════════════════════════════════════════════════════════════
//  STEP 1 — Write one float function per sensor
// ════════════════════════════════════════════════════════════════

float readTemperature() {
  // TODO: e.g. return dht.readTemperature();
  return 27.5;
}

float readHumidity() {
  // TODO: e.g. return dht.readHumidity();
  return 58.0;
}

float readGas() {
  // TODO: e.g. return analogRead(A0) * (5.0 / 1023.0) * 100;
  return 380.0;
}

float readDistance() {
  // TODO: e.g. ultrasonic pulseIn measurement
  return 100.0;
}


// ════════════════════════════════════════════════════════════════
//  STEP 2 — Register sensors in this table
//  "device_name" must EXACTLY match name on your Devices page
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
};

// ─────────────────────────────────────────────────────────────────
// Internals
// ─────────────────────────────────────────────────────────────────

const int SENSOR_COUNT = sizeof(SENSORS) / sizeof(SENSORS[0]);

SoftwareSerial espSerial(2, 3);   // RX=D2, TX=D3
WiFiEspClient  client;

unsigned long lastSend = 0;
int cycleNum = 0;

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  delay(2000);

  Serial.println(F("\n══════════════════════════════════"));
  Serial.println(F("  Arduino Multi-Sensor Monitor"));
  Serial.print(F("  Sensors: ")); Serial.println(SENSOR_COUNT);
  Serial.println(F("══════════════════════════════════\n"));

  WiFiEsp.init(&espSerial);

  if (WiFiEsp.status() == WL_NO_SHIELD) {
    Serial.println(F("[ERROR] ESP8266 not detected — check wiring"));
    while (true);
  }

  connectWiFi();
  lastSend = millis() - SEND_INTERVAL;
}

void loop() {
  if (WiFiEsp.status() != WL_CONNECTED) {
    Serial.println(F("[WiFi] Reconnecting..."));
    connectWiFi();
    return;
  }

  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    cycleNum++;
    Serial.print(F("\n─── Cycle #")); Serial.print(cycleNum); Serial.println(F(" ───"));
    sendAllSensors();
  }
}

// ── Build JSON and POST ───────────────────────────────────────────
void sendAllSensors() {

  float values[SENSOR_COUNT];
  for (int i = 0; i < SENSOR_COUNT; i++) {
    values[i] = SENSORS[i].readFn();
    Serial.print(F("[Sensor] ")); Serial.print(SENSORS[i].deviceName);
    Serial.print(F(" = ")); Serial.print(values[i]); Serial.println(SENSORS[i].unit);
  }

  // Build JSON manually (no ArduinoJson on Uno — saves RAM)
  String json = F("{\"api_key\":\"");
  json += API_KEY;
  json += F("\",\"devices\":[");

  for (int i = 0; i < SENSOR_COUNT; i++) {
    if (i > 0) json += ',';
    json += F("{\"device_name\":\"");
    json += SENSORS[i].deviceName;
    json += F("\",\"sensors\":[{\"type\":\"");
    json += SENSORS[i].type;
    json += F("\",\"value\":\"");
    json += String(values[i], 2);
    json += F("\",\"unit\":\"");
    json += SENSORS[i].unit;
    json += F("\"}]}");
  }
  json += F("]}");

  Serial.print(F("[HTTP] Payload length: ")); Serial.println(json.length());
  sendPost(json);
}

// ── AT-command HTTP/HTTPS POST ────────────────────────────────────
void sendPost(const String& json) {
  bool useSSL = (SERVER_PORT == 443);

  // Build HTTP request string
  String request  = F("POST ");
  request += API_PATH;
  request += F(" HTTP/1.1\r\nHost: ");
  request += SERVER_HOST;
  request += F("\r\nContent-Type: application/json\r\nContent-Length: ");
  request += json.length();
  request += F("\r\nConnection: close\r\n\r\n");
  request += json;

  // Connect via SSL or TCP
  int ok;
  if (useSSL) {
    ok = client.connectSSL(SERVER_HOST, SERVER_PORT);
  } else {
    ok = client.connect(SERVER_HOST, SERVER_PORT);
  }

  if (!ok) {
    Serial.println(F("[HTTP] Connection failed"));
    return;
  }

  client.print(request);
  Serial.println(F("[HTTP] Request sent — reading response..."));

  // Read response (skip headers, grab body)
  String body = "";
  bool   inBody = false;
  unsigned long t = millis();

  while ((client.connected() || client.available()) && millis() - t < 8000) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      if (!inBody && line == "\r") { inBody = true; continue; }
      if (inBody) { body += line; }
    }
  }
  client.stop();

  body.trim();
  Serial.print(F("[Server] ")); Serial.println(body.isEmpty() ? F("(no body)") : body.c_str());
}

// ── WiFi ──────────────────────────────────────────────────────────
void connectWiFi() {
  Serial.print(F("[WiFi] Connecting to "));
  Serial.print(WIFI_SSID);

  int status = WL_IDLE_STATUS;
  int tries  = 0;
  while (status != WL_CONNECTED && tries < 10) {
    status = WiFiEsp.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print('.');
    tries++;
  }

  if (WiFiEsp.status() == WL_CONNECTED) {
    Serial.println(F(" connected!"));
    Serial.print(F("[WiFi] IP: "));
    Serial.println(WiFiEsp.localIP());
  } else {
    Serial.println(F(" FAILED — check SSID/password or ESP8266 baud rate"));
  }
}
