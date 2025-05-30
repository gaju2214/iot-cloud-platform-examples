
# ESP8266 Temperature Monitoring

This example demonstrates how to use an ESP32 with DHT11 to send temperature data to the IoT Cloud Platform in real-time.

## 🔧 Requirements
- ESP8266 board
- DHT11 sensor
- Wi-Fi connection

## 🚀 Setup Instructions

1. Flash ESP8266.
2. Connect DHT11 to GPIO 4.
3. Upload `monitor.ino` to ESP8266 using Arduino IDE.
4. Edit `monitor.ino` and insert your `Device ID` and `API Key` from your dashboard.
5. Run the script and see live data on the dashboard.

## 📡 Sample Output
```json
{
  "temperature": 27.4,
  "humidity": 60
}
