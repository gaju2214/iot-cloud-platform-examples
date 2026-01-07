import network
import urequests
import utime
import json

WIFI_SSID = "Your-ssid"
WIFI_PASSWORD = "Your-pass"
API_KEY = "Your-api-key"
API_URL = "https://iot.getyourprojectdone.in/api/insert_data.php"

def connect_to_wifi(ssid, password):
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(ssid, password)
    while not wlan.isconnected():
        utime.sleep(1)
    print("Connected:", wlan.ifconfig())
    return wlan

def build_payload(temp):
    return {
        "api_key": API_KEY,
        "devices": [
            {
                "device_name": "temperature1",
                "sensors": [
                    {"type": "temperature", "value": temp, "unit": "C"}
                ]
            },
            {
                "device_name": "humidity_sensor",
                "sensors": [
                    {"type": "humidity", "value": 60.2, "unit": "%"}
                ]
            }
        ]
    }

def send_data(payload):
    headers = {"Content-Type": "application/json"}
    try:
        response = urequests.post(API_URL, data=json.dumps(payload), headers=headers)
        print("Server Response:", response.text)
        response.close()
    except Exception as e:
        print("Error:", e)

if __name__ == "__main__":
    wlan = connect_to_wifi(WIFI_SSID, WIFI_PASSWORD)
    while True:
        temperature = 25.3
        payload = build_payload(temperature)
        send_data(payload)
        utime.sleep(10)
