/*
 * ════════════════════════════════════════════════════════════════
 *  ESP8266 — Multi-Device Control  |  IoT Platform
 *   • Add unlimited control devices in 2 steps
 * ════════════════════════════════════════════════════════════════
 *
 *  HOW IT WORKS:
 *   1. Every POLL_INTERVAL ms → asks server for pending commands
 *   2. Executes each command on the correct GPIO / pin
 *   3. Acknowledges each command back to the server
 *
 *  HOW TO ADD A NEW CONTROL DEVICE:
 *   Step 1 — Write a handler void function for the device
 *   Step 2 — Add one row to DEVICES[] table below
 *
 *  BOARD   : NodeMCU 1.0 (ESP-12E Module)
 *  LIBRARIES: ESP8266WiFi, ESP8266HTTPClient, ArduinoJson v6.x
 *  SERIAL MONITOR: 115200 baud
 * ════════════════════════════════════════════════════════════════
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

// ── STEP 0 — Credentials & server ────────────────────────────────
const char* WIFI_SSID     = "Your-SSID";
const char* WIFI_PASSWORD = "Your-Password";
const char* API_KEY       = "Your-API-Key";

// Production server (HTTPS)
const char* SERVER = "https://iot.getyourprojectdone.in";



const unsigned long POLL_INTERVAL = 5000;  // poll commands every 5 s
// ─────────────────────────────────────────────────────────────────


// ════════════════════════════════════════════════════════════════
//  STEP 1 — Define pins and write one handler per control device
//
//  Command types sent by the dashboard:
//    "ON"        → switch ON  (relay, LED, fan, buzzer …)
//    "OFF"       → switch OFF
//    "SET_VALUE" → set to a value (dimmer %, servo angle, PWM …)
//                  cmdValue = the numeric string e.g. "75"
//
//  Handler signature:  void myHandler(const char* type, const char* value)
// ════════════════════════════════════════════════════════════════

// ── Built-in LED (D4 / GPIO2, active LOW) ────────────────────────
#define PIN_LED   LED_BUILTIN  // D4
void handleLED(const char* type, const char* val) {
  if (strcmp(type, "ON")  == 0) { digitalWrite(PIN_LED, LOW);  Serial.println(F("[LED] ON"));  }
  if (strcmp(type, "OFF") == 0) { digitalWrite(PIN_LED, HIGH); Serial.println(F("[LED] OFF")); }
}

// ── Relay (active HIGH — connect relay IN pin here) ───────────────
#define PIN_RELAY D1  // GPIO5
void handleRelay(const char* type, const char* val) {
  if (strcmp(type, "ON")  == 0) { digitalWrite(PIN_RELAY, HIGH); Serial.println(F("[Relay] ON"));  }
  if (strcmp(type, "OFF") == 0) { digitalWrite(PIN_RELAY, LOW);  Serial.println(F("[Relay] OFF")); }
}

// ── Fan / Motor (PWM speed control via SET_VALUE 0-100) ──────────
#define PIN_FAN D2  // GPIO4 — must be PWM-capable
void handleFan(const char* type, const char* val) {
  if (strcmp(type, "ON")  == 0) { analogWrite(PIN_FAN, 255); Serial.println(F("[Fan] FULL ON"));  }
  if (strcmp(type, "OFF") == 0) { analogWrite(PIN_FAN, 0);   Serial.println(F("[Fan] OFF"));       }
  if (strcmp(type, "SET_VALUE") == 0) {
    int pct = constrain(atoi(val), 0, 100);
    int pwm = map(pct, 0, 100, 0, 255);
    analogWrite(PIN_FAN, pwm);
    Serial.printf("[Fan] Speed = %d%%\n", pct);
  }
}

// ── Servo (SET_VALUE = angle 0-180) ──────────────────────────────
// #include <Servo.h>
// Servo myServo;
// #define PIN_SERVO D5
// void handleServo(const char* type, const char* val) {
//   if (strcmp(type, "SET_VALUE") == 0) {
//     int angle = constrain(atoi(val), 0, 180);
//     myServo.write(angle);
//     Serial.printf("[Servo] Angle = %d\n", angle);
//   }
//   if (strcmp(type, "ON")  == 0) { myServo.write(180); }
//   if (strcmp(type, "OFF") == 0) { myServo.write(0);   }
// }

// ── Buzzer (ON/OFF or SET_VALUE = frequency Hz) ───────────────────
// #define PIN_BUZZER D6
// void handleBuzzer(const char* type, const char* val) {
//   if (strcmp(type, "ON")  == 0) { tone(PIN_BUZZER, 1000); }
//   if (strcmp(type, "OFF") == 0) { noTone(PIN_BUZZER);     }
//   if (strcmp(type, "SET_VALUE") == 0) { tone(PIN_BUZZER, atoi(val)); }
// }


// ════════════════════════════════════════════════════════════════
//  STEP 2 — Register control devices in this table
//  "device_name" must EXACTLY match the name on your Devices page
//  (Device Type must be a control type: relay, led, fan, servo …)
// ════════════════════════════════════════════════════════════════

struct ControlDevice {
  const char* deviceName;
  void (*handler)(const char* cmdType, const char* cmdValue);
};

ControlDevice DEVICES[] = {
// { "device_name",  handlerFunction }
  { "led",          handleLED   },
  { "relay",        handleRelay },
  { "fan",          handleFan   },

  // Uncomment to add:
  // { "servo",     handleServo  },
  // { "buzzer",    handleBuzzer },
};

// ─────────────────────────────────────────────────────────────────
// Internals — no changes needed below this line
// ─────────────────────────────────────────────────────────────────

const int DEVICE_COUNT = sizeof(DEVICES) / sizeof(DEVICES[0]);

BearSSL::WiFiClientSecure secureClient;
WiFiClient                plainClient;
unsigned long             lastPoll = 0;

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println(F("\n══════════════════════════════════"));
  Serial.println(F("  ESP8266 Multi-Device Control"));
  Serial.printf( "  Devices registered : %d\n", DEVICE_COUNT);
  Serial.printf( "  Poll interval      : %lu ms\n", POLL_INTERVAL);
  Serial.println(F("══════════════════════════════════\n"));

  // Init all control pins
  pinMode(PIN_LED,   OUTPUT); digitalWrite(PIN_LED,   HIGH); // LED off
  pinMode(PIN_RELAY, OUTPUT); digitalWrite(PIN_RELAY, LOW);  // Relay off
  pinMode(PIN_FAN,   OUTPUT); analogWrite(PIN_FAN,    0);    // Fan off

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

// ── Poll commands for every registered device ─────────────────────
void pollAllDevices() {
  for (int i = 0; i < DEVICE_COUNT; i++) {
    pollDevice(DEVICES[i].deviceName, DEVICES[i].handler);
  }
}

void pollDevice(const char* deviceName, void(*handler)(const char*, const char*)) {
  // Build poll request
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
    int         id   = cmd["id"]            | 0;
    const char* type = cmd["command_type"]  | "";
    const char* val  = cmd["command_value"] | "";

    Serial.printf("[CMD]  #%d  %s → type=%s  value=%s\n", id, deviceName, type, val);

    handler(type, val);
    ackCommand(id, "executed");
    delay(50);
  }
}

// ── Acknowledge a command ─────────────────────────────────────────
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
    Serial.printf("[Ack]  #%d → %s\n", cmdId, rdoc["message"] | resp.c_str());
  }
}

// ── HTTP POST helper ──────────────────────────────────────────────
String httpPost(const String& url, const String& body) {
  bool isHttps = url.startsWith("https");
  HTTPClient http;

  if (isHttps) http.begin(secureClient, url);
  else         http.begin(plainClient,  url);

  http.addHeader(F("Content-Type"), F("application/json"));
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
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println(F(" FAILED"));
  }
}
