/*
 * ════════════════════════════════════════════════════════════════
 *  Arduino Uno/Mega + ESP8266 Module — Multi-Device Control
 *   • Add unlimited control devices in 2 steps  •
 * ════════════════════════════════════════════════════════════════
 *
 *  WIRING:
 *   Arduino D2 → ESP8266 TX   |   Arduino D3 → ESP8266 RX
 *   ESP8266 VCC → 3.3 V       |   ESP8266 GND → GND
 *   ESP8266 CH_EN → 3.3 V
 *
 *  HOW TO ADD A NEW CONTROL DEVICE:
 *   Step 1 — Write a void handler function for the device
 *   Step 2 — Add one row to DEVICES[] table below
 *
 *  LIBRARY: WiFiEsp by bportaluri (Library Manager)
 *  SERIAL MONITOR: 9600 baud
 * ════════════════════════════════════════════════════════════════
 */

#include <WiFiEsp.h>
#include <SoftwareSerial.h>

// ── STEP 0 — Credentials & server ────────────────────────────────
const char* WIFI_SSID     = "Your-SSID";
const char* WIFI_PASSWORD = "Your-Password";
const char* API_KEY       = "Your-API-Key";

const char* SERVER_HOST   = "iot.getyourprojectdone.in";
const int   SERVER_PORT   = 443;


const char* POLL_PATH = "/api/device_poll.php";
const char* ACK_PATH  = "/api/device_ack.php";
// Local LAMPP:
// const char* POLL_PATH = "/iot_platform/api/device_poll.php";
// const char* ACK_PATH  = "/iot_platform/api/device_ack.php";

const unsigned long POLL_INTERVAL = 8000;  // poll every 8 s
// ─────────────────────────────────────────────────────────────────


// ════════════════════════════════════════════════════════════════
//  STEP 1 — Define pins and write one handler per device
//
//  Handler receives: cmdType ("ON" | "OFF" | "SET_VALUE")
//                    cmdValue (numeric string, e.g. "75")
// ════════════════════════════════════════════════════════════════

// ── Built-in LED (pin 13, active HIGH on most Arduino boards) ────
#define PIN_LED 13
void handleLED(const char* type, const char* val) {
  if (strcmp(type, "ON")  == 0) { digitalWrite(PIN_LED, HIGH); Serial.println(F("[LED] ON"));  }
  if (strcmp(type, "OFF") == 0) { digitalWrite(PIN_LED, LOW);  Serial.println(F("[LED] OFF")); }
}

// ── Relay (active HIGH) ───────────────────────────────────────────
#define PIN_RELAY 7
void handleRelay(const char* type, const char* val) {
  if (strcmp(type, "ON")  == 0) { digitalWrite(PIN_RELAY, HIGH); Serial.println(F("[Relay] ON"));  }
  if (strcmp(type, "OFF") == 0) { digitalWrite(PIN_RELAY, LOW);  Serial.println(F("[Relay] OFF")); }
}

// ── Fan via PWM (SET_VALUE = 0-100 percent) ───────────────────────
// Arduino Uno PWM pins: 3,5,6,9,10,11
#define PIN_FAN 9
void handleFan(const char* type, const char* val) {
  if (strcmp(type, "ON")  == 0) { analogWrite(PIN_FAN, 255); Serial.println(F("[Fan] FULL ON")); }
  if (strcmp(type, "OFF") == 0) { analogWrite(PIN_FAN, 0);   Serial.println(F("[Fan] OFF"));     }
  if (strcmp(type, "SET_VALUE") == 0) {
    int pct = constrain(atoi(val), 0, 100);
    analogWrite(PIN_FAN, map(pct, 0, 100, 0, 255));
    Serial.print(F("[Fan] Speed = ")); Serial.print(pct); Serial.println('%');
  }
}

// ── Buzzer (ON/OFF or SET_VALUE = duration ms) ────────────────────
// #define PIN_BUZZER 6
// void handleBuzzer(const char* type, const char* val) {
//   if (strcmp(type, "ON")  == 0) { digitalWrite(PIN_BUZZER, HIGH); }
//   if (strcmp(type, "OFF") == 0) { digitalWrite(PIN_BUZZER, LOW);  }
//   if (strcmp(type, "SET_VALUE") == 0) {
//     digitalWrite(PIN_BUZZER, HIGH);
//     delay(atoi(val));
//     digitalWrite(PIN_BUZZER, LOW);
//   }
// }


// ════════════════════════════════════════════════════════════════
//  STEP 2 — Register devices in this table
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
  // { "buzzer",    handleBuzzer },
};

// ─────────────────────────────────────────────────────────────────
// Internals
// ─────────────────────────────────────────────────────────────────

const int DEVICE_COUNT = sizeof(DEVICES) / sizeof(DEVICES[0]);

SoftwareSerial espSerial(2, 3);
WiFiEspClient  client;
unsigned long  lastPoll = 0;

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  delay(2000);

  Serial.println(F("\n══════════════════════════════════"));
  Serial.println(F("  Arduino Multi-Device Control"));
  Serial.print(F("  Devices : ")); Serial.println(DEVICE_COUNT);
  Serial.println(F("══════════════════════════════════\n"));

  pinMode(PIN_LED,   OUTPUT); digitalWrite(PIN_LED,   LOW);
  pinMode(PIN_RELAY, OUTPUT); digitalWrite(PIN_RELAY, LOW);
  pinMode(PIN_FAN,   OUTPUT); analogWrite(PIN_FAN,    0);

  WiFiEsp.init(&espSerial);
  if (WiFiEsp.status() == WL_NO_SHIELD) {
    Serial.println(F("[ERROR] ESP8266 not detected"));
    while (true);
  }

  connectWiFi();
  lastPoll = millis() - POLL_INTERVAL;
}

void loop() {
  if (WiFiEsp.status() != WL_CONNECTED) {
    connectWiFi(); return;
  }

  if (millis() - lastPoll >= POLL_INTERVAL) {
    lastPoll = millis();
    for (int i = 0; i < DEVICE_COUNT; i++) {
      pollDevice(DEVICES[i].deviceName, DEVICES[i].handler);
    }
  }
}

// ── Poll one device ───────────────────────────────────────────────
void pollDevice(const char* devName, void(*handler)(const char*, const char*)) {
  // Build request JSON
  String json = F("{\"api_key\":\"");
  json += API_KEY;
  json += F("\",\"device_name\":\"");
  json += devName;
  json += F("\"}");

  String resp = doPost(POLL_PATH, json);
  if (resp.isEmpty()) return;

  // Find "commands" array manually (avoids ArduinoJson RAM on Uno)
  // If response contains "command_type" there are commands
  if (resp.indexOf(F("command_type")) == -1) return;

  Serial.print(F("[Poll] ")); Serial.print(devName);
  Serial.println(F(" — commands found"));

  // Parse up to 5 commands
  int searchFrom = 0;
  for (int c = 0; c < 5; c++) {
    int idIdx   = resp.indexOf(F("\"id\":"),  searchFrom);
    if (idIdx == -1) break;

    // Extract id
    int idStart = idIdx + 5;
    int idEnd   = resp.indexOf(',', idStart);
    int cmdId   = resp.substring(idStart, idEnd).toInt();

    // Extract command_type
    int typeIdx = resp.indexOf(F("\"command_type\":\""), searchFrom);
    if (typeIdx == -1) break;
    int tStart  = typeIdx + 16;
    int tEnd    = resp.indexOf('"', tStart);
    String cmdType = resp.substring(tStart, tEnd);

    // Extract command_value
    int valIdx  = resp.indexOf(F("\"command_value\":\""), searchFrom);
    int vStart  = valIdx + 17;
    int vEnd    = resp.indexOf('"', vStart);
    String cmdVal = (valIdx != -1) ? resp.substring(vStart, vEnd) : "";

    Serial.print(F("[CMD]  #")); Serial.print(cmdId);
    Serial.print(F("  type=")); Serial.print(cmdType);
    Serial.print(F("  value=")); Serial.println(cmdVal);

    handler(cmdType.c_str(), cmdVal.c_str());
    ackCommand(cmdId, "executed");

    // Move search cursor past this command
    searchFrom = tEnd + 1;
    delay(100);
  }
}

// ── Acknowledge command ───────────────────────────────────────────
void ackCommand(int cmdId, const char* status) {
  String json = F("{\"api_key\":\"");
  json += API_KEY;
  json += F("\",\"command_id\":");
  json += cmdId;
  json += F(",\"status\":\"");
  json += status;
  json += F("\"}");

  String resp = doPost(ACK_PATH, json);
  Serial.print(F("[Ack]  #")); Serial.print(cmdId);
  Serial.print(F(" → "));
  Serial.println(resp.isEmpty() ? F("no response") : resp.c_str());
}

// ── AT-command POST (SSL or plain HTTP) ──────────────────────────
String doPost(const char* path, const String& json) {
  bool useSSL = (SERVER_PORT == 443);

  String request  = F("POST ");
  request += path;
  request += F(" HTTP/1.1\r\nHost: ");
  request += SERVER_HOST;
  request += F("\r\nContent-Type: application/json\r\nContent-Length: ");
  request += json.length();
  request += F("\r\nConnection: close\r\n\r\n");
  request += json;

  int ok = useSSL ? client.connectSSL(SERVER_HOST, SERVER_PORT)
                  : client.connect(SERVER_HOST, SERVER_PORT);
  if (!ok) { Serial.println(F("[HTTP] Connect failed")); return ""; }

  client.print(request);

  String body = "";
  bool   inBody = false;
  unsigned long t = millis();

  while ((client.connected() || client.available()) && millis() - t < 8000) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      if (!inBody && line == "\r") { inBody = true; continue; }
      if (inBody) body += line;
    }
  }
  client.stop();
  body.trim();
  return body;
}

// ── WiFi ──────────────────────────────────────────────────────────
void connectWiFi() {
  Serial.print(F("[WiFi] Connecting"));
  int status = WL_IDLE_STATUS, tries = 0;
  while (status != WL_CONNECTED && tries < 10) {
    status = WiFiEsp.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print('.'); tries++;
  }
  if (WiFiEsp.status() == WL_CONNECTED) {
    Serial.println(F(" connected!"));
    Serial.print(F("[WiFi] IP: ")); Serial.println(WiFiEsp.localIP());
  } else {
    Serial.println(F(" FAILED"));
  }
}
