#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

const char* ssid = "Your-ssid";
const char* password = "Your-passwd";

const char* host = "getyourprojectdone.in";
const int httpsPort = 443;

const char* api_key = "Your-api-key";

WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected!");

  client.setInsecure(); // Skip SSL validation (for testing)
}

void loop() {
  // --- Simulate or read actual values ---
  int ledState = digitalRead(LED_BUILTIN); // Or manually manage state
  float temperature = 28.3; // Simulated temperature (replace with sensor reading)
  int distance = 120;       // Simulated ultrasonic value (replace with actual reading)

  // Turn LED ON/OFF
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);

  // --- Build JSON ---
  String json = "{";
  json += "\"api_key\":\"" + String(api_key) + "\",";
  json += "\"devices\":[";
  
  // LED Device
  json += "{";
  json += "\"device_name\":\"led\",";
  json += "\"sensors\":[{\"type\":\"ledlight\",\"value\":\"" + String(ledState) + "\",\"unit\":\"w\"}]";
  json += "},";

  // Temperature Device
  json += "{";
  json += "\"device_name\":\"temp\",";
  json += "\"sensors\":[{\"type\":\"temperature\",\"value\":\"" + String(temperature) + "\",\"unit\":\"C\"}]";
  json += "},";

  // Ultrasonic Device
  json += "{";
  json += "\"device_name\":\"ultra\",";
  json += "\"sensors\":[{\"type\":\"distance\",\"value\":\"" + String(distance) + "\",\"unit\":\"cm\"}]";
  json += "}";

  json += "]}";

  // --- POST Request ---
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection to server failed");
    return;
  }

  String request =
    String("POST ") + "/iot_platform/api/insert_data.php HTTP/1.1\r\n" +
    "Host: " + host + "\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + json.length() + "\r\n" +
    "Connection: close\r\n\r\n" +
    json;

  client.print(request);
  Serial.println("Data sent:");
  Serial.println(json);

  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }

  client.stop();

  delay(10000); // Send every 10 seconds
}
