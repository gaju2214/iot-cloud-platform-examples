/*
 * ════════════════════════════════════════════════════════════════
 *  ESP8266 — Multi-Sensor Monitor And Control in one code |  IoT Platform
 *   • Add unlimited sensors in 2 steps •
 * ════════════════════════════════════════════════════════════════
 */


#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

// ── STEP 0 — Credentials & server ────────────────────────────────
const char* WIFI_SSID     = "Electrosoft";
const char* WIFI_PASSWORD = "Elec1234";
const char* API_KEY       = "5991EEAGKPGF";

const char* SERVER = "https://iot.getyourprojectdone.in";

const unsigned long POLL_INTERVAL = 5000;
// ─────────────────────────────────────────────────────────────────


// ── Built-in LED (ACTIVE LOW) ────────────────────────────────────
#define PIN_LED LED_BUILTIN

bool ledState = 0; // 🔥 track state (0=OFF,1=ON)

void handleLED(const char* type, const char* val) {
  if (strcmp(type, "ON") == 0) {
    digitalWrite(PIN_LED, LOW);
    ledState = 1;
    Serial.println(F("[LED] ON"));
  }
  if (strcmp(type, "OFF") == 0) {
    digitalWrite(PIN_LED, HIGH);
    ledState = 0;
    Serial.println(F("[LED] OFF"));
  }
}


// ── ONLY LED DEVICE ──────────────────────────────────────────────
struct ControlDevice {
  const char* deviceName;
  void (*handler)(const char* cmdType, const char* cmdValue);
};

ControlDevice DEVICES[] = {
  { "led", handleLED }
};

// ─────────────────────────────────────────────────────────────────
// Internals (unchanged)
// ─────────────────────────────────────────────────────────────────

const int DEVICE_COUNT = sizeof(DEVICES) / sizeof(DEVICES[0]);

BearSSL::WiFiClientSecure secureClient;
WiFiClient plainClient;
unsigned long lastPoll = 0;

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println(F("\n══════════════════════════════════"));
  Serial.println(F("  ESP8266 LED Control + Monitor"));
  Serial.printf( "  Devices registered : %d\n", DEVICE_COUNT);
  Serial.printf( "  Poll interval      : %lu ms\n", POLL_INTERVAL);
  Serial.println(F("══════════════════════════════════\n"));

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  secureClient.setInsecure();
  connectWiFi();

  lastPoll = millis() - POLL_INTERVAL;
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[WiFi] Disconnected — reconnecting..."));
    connectWiFi();
    return;
  }

  if (millis() - lastPoll >= POLL_INTERVAL) {
    lastPoll = millis();
    pollAllDevices();
  }
}

// ── Poll ─────────────────────────────────────────────────────────
void pollAllDevices() {
  for (int i = 0; i < DEVICE_COUNT; i++) {
    pollDevice(DEVICES[i].deviceName, DEVICES[i].handler);
  }
}

void pollDevice(const char* deviceName, void(*handler)(const char*, const char*)) {
  StaticJsonDocument<128> req;
  req["api_key"]     = API_KEY;
  req["device_name"] = deviceName;

  String payload;
  serializeJson(req, payload);

  String url  = String(SERVER) + "/api/device_poll.php";
  String resp = httpPost(url, payload);

  if (resp.isEmpty()) return;

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, resp) != DeserializationError::Ok) return;

  if (strcmp(doc["status"] | "error", "success") != 0) {
    Serial.printf("[Poll] %s — %s\n", deviceName, doc["message"] | "error");
    return;
  }

  JsonArray cmds = doc["commands"].as<JsonArray>();
  if (cmds.size() == 0) return;

  Serial.printf("[Poll] %s — %d command(s)\n", deviceName, (int)cmds.size());

  for (JsonObject cmd : cmds) {
    int id = cmd["id"] | 0;
    const char* type = cmd["command_type"] | "";
    const char* val  = cmd["command_value"] | "";

    Serial.printf("[CMD] #%d %s → %s\n", id, deviceName, type);

    handler(type, val);

    // 🔥 SEND STATUS AFTER EXECUTION
    sendLEDStatus();

    ackCommand(id, "executed");
    delay(50);
  }
}

// ── 🔥 SEND LED STATUS (MONITOR) ────────────────────────────────
void sendLEDStatus() {
  DynamicJsonDocument doc(256);
  doc["api_key"] = API_KEY;

  JsonArray devices = doc.createNestedArray("devices");
  JsonObject device = devices.createNestedObject();
  device["device_name"] = "led";

  JsonArray sensors = device.createNestedArray("sensors");
  JsonObject sensor = sensors.createNestedObject();

  sensor["type"]  = "status";
  sensor["value"] = ledState; // ✅ 0 or 1
  sensor["unit"]  = "";

  String payload;
  serializeJson(doc, payload);

  String url = String(SERVER) + "/api/insert_data.php";
  httpPost(url, payload);

  Serial.printf("[Monitor] LED state sent = %d\n", ledState);
}

// ── Ack ──────────────────────────────────────────────────────────
void ackCommand(int cmdId, const char* status) {
  StaticJsonDocument<128> doc;
  doc["api_key"]    = API_KEY;
  doc["command_id"] = cmdId;
  doc["status"]     = status;

  String payload;
  serializeJson(doc, payload);

  String url  = String(SERVER) + "/api/device_ack.php";
  String resp = httpPost(url, payload);

  if (!resp.isEmpty()) {
    StaticJsonDocument<128> rdoc;
    deserializeJson(rdoc, resp);
    Serial.printf("[Ack] #%d → %s\n", cmdId, rdoc["message"] | resp.c_str());
  }
}

// ── HTTP ─────────────────────────────────────────────────────────
String httpPost(const String& url, const String& body) {
  bool isHttps = url.startsWith("https");
  HTTPClient http;

  if (isHttps) http.begin(secureClient, url);
  else http.begin(plainClient, url);

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);

  int code = http.POST(body);
  String response = "";

  if (code > 0) {
    response = http.getString();
  } else {
    Serial.printf("[HTTP] Error: %s\n", http.errorToString(code).c_str());
  }

  http.end();
  return response;
}

// ── WiFi ─────────────────────────────────────────────────────────
void connectWiFi() {
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 40) {
    delay(500);
    Serial.print(".");
    t++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" connected!");
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println(" FAILED");
  }
}