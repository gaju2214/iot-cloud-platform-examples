#include <WiFiEsp.h>
#include <SoftwareSerial.h>

SoftwareSerial esp(2, 3); // RX, TX

const char* ssid = "YOUR-SSID";
const char* password = "YOUR-PASSWD";
const char* host = "getyourprojectdone.in";
const int httpPort = 80;
const char* api_key = "YOUR-API-KEY";

void setup() {
  Serial.begin(9600);
  esp.begin(9600);
  delay(2000);
  
  sendCommand("AT", "OK", 2000);
  sendCommand("AT+CWMODE=1", "OK", 1000);
  
  String cmd = "AT+CWJAP=\"" + String(ssid) + "\",\"" + String(password) + "\"";
  sendCommand(cmd.c_str(), "WIFI CONNECTED", 10000);
  
  Serial.println("WiFi connected!");
}

void loop() {
   digitalWrite(LED_BUILTIN, LOW);
  delay(1000);

  // Read and convert to logical LED state (1 = ON)
  int ledState = digitalRead(LED_BUILTIN) == LOW ? 1 : 0;
  sendData(ledState);

  // Toggle LED OFF
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);

  ledState = digitalRead(LED_BUILTIN) == LOW ? 1 : 0;
  sendData(ledState);
}    // simulate ON
  float temp = 28.3;      // simulate temperature
  int distance = 120;     // simulate distance

  String json = "{";
  json += "\"api_key\":\"" + String(api_key) + "\",";
  json += "\"devices\":[";
  json += "{";
  json += "\"device_name\":\"YOUR-DEVICE-NAME(eg-led)\",";
  json += "\"sensors\":[{\"type\":\"ledlight\",\"value\":\"" + String(ledState) + "\",\"unit\":\"w\"}]";
  json += "},";
  json += "{";
  json += "\"device_name\":\"YOUR-DEVICE-NAME(eg-temp)\",";
  json += "\"sensors\":[{\"type\":\"temperature\",\"value\":\"" + String(temp) + "\",\"unit\":\"C\"}]";
  json += "},";
  json += "{";
  json += "\"device_name\":\"YOUR-DEVICE-NAME(eg-ultra)\",";
  json += "\"sensors\":[{\"type\":\"distance\",\"value\":\"" + String(distance) + "\",\"unit\":\"cm\"}]";
  json += "}";
  json += "]}";

  String postRequest =
    "POST /iot_platform/api/insert_data.php HTTP/1.1\r\n"
    "Host: getyourprojectdone.in\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: " + String(json.length()) + "\r\n"
    "Connection: close\r\n\r\n" +
    json;

  sendCommand("AT+CIPMUX=0", "OK", 1000);
  String connectCmd = "AT+CIPSTART=\"TCP\",\"" + String(host) + "\"," + String(httpPort);
  if (sendCommand(connectCmd.c_str(), "CONNECT", 5000)) {
    String sendLen = "AT+CIPSEND=" + String(postRequest.length());
    if (sendCommand(sendLen.c_str(), ">", 2000)) {
      esp.print(postRequest);
      Serial.println("Data sent:");
      Serial.println(json);
    }
  }

  delay(10000); // Wait 10s
}

bool sendCommand(const char* cmd, const char* ack, int timeout) {
  esp.println(cmd);
  long int time = millis();
  String response = "";

  while ((time + timeout) > millis()) {
    while (esp.available()) {
      char c = esp.read();
      response += c;
    }
    if (response.indexOf(ack) != -1) {
      Serial.println("Command OK: " + String(cmd));
      return true;
    }
  }
  Serial.println("Command FAIL: " + String(cmd));
  return false;
}
