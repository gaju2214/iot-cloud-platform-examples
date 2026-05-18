# IoT Cloud Platform — Code Examples

Connect, monitor, and control your IoT devices using the **[Get Your Projects Done IoT Cloud Platform](https://iot.getyourprojectdone.in)** — a fully free platform where you can create unlimited channels, add unlimited devices, monitor live sensor data, and control hardware remotely.

![Platform Screenshot](https://github.com/gaju2214/iot-cloud-platform-examples/blob/main/platform.png)

---

## What's Included

- **Completely free** — unlimited channels, unlimited devices, no subscription
- **Real-time device control** — send ON/OFF/SET commands to your hardware directly from the dashboard, no app needed
- **Raspberry Pi Pico W** — MicroPython monitoring & control examples
- **ESP32 / ESP8266** — MicroPython & Arduino examples
- **Raspberry Pi** — Python sensor monitoring scripts
- **Arduino** — Arduino IDE HTTP data push examples
- **Raw HTTPS support** — production-ready, works with TLS 1.2 and HTTP/1.1

---

## Folder Structure

| Folder | Board | Language |
|--------|-------|----------|
| `pico_w/` | Raspberry Pi Pico W | MicroPython |
| `esp32/` | ESP32 / ESP8266 | MicroPython / Arduino |
| `rpi/` | Raspberry Pi (Linux) | Python 3 |
| `arduino/` | Arduino (Uno, Mega, Nano) | C++ / Arduino IDE |

### Key Files

| File | Description |
|------|-------------|
| `esp8266_led_control.ino` | ESP8266 — Control LED from dashboard in real time |
| `esp8266_control.ino` | ESP8266 — Multi-device remote control (relay, fan, servo…) |
| `arduino_control.ino` | Arduino — Multi-device remote control via HTTP |
| `arduino_monitor.ino` | Arduino — Multi-sensor data push to platform |
| `raspberry_monitor.py` | Pico W — Multi-sensor monitor (plug-in architecture) |
| `raspberry_control.py` | Pico W — Multi-device remote control (relay, fan, servo…) |

---

## Quick Start

### 1. Register & Create a Channel
1. Go to [iot.getyourprojectdone.in/register.php](https://iot.getyourprojectdone.in/register.php) and create a free account
2. Login → create a **channel** → copy your **API Key**

### 2. Add Your Devices
Inside your channel, add devices with names that exactly match the `device_name` values in your code (e.g. `temp_sensor`, `hum_sensor`, `pico_temp`)

### 3. Clone & Flash
```bash
git clone https://github.com/gaju2214/iot-cloud-platform-examples.git
```
Open the relevant `.py` file in **Thonny**, update your WiFi credentials and API Key, then run it on your board.

---

## Configuration (all files)

```python
WIFI_SSID     = "Your-WiFi-Name"
WIFI_PASSWORD = "Your-WiFi-Password"
API_KEY       = "Your-API-Key"       # from dashboard channel page
```

---

## HTTPS Note (Pico W / MicroPython)

All examples use **raw sockets with `ssl.SSLContext`** instead of `urequests` to avoid the common MicroPython HTTPS errors:

```
MBEDTLS_ERR_SSL_CONN_EOF
ImportError: no module named 'ussl'
HTTP Error: (-2)
ECONNABORTED
```

This approach forces **HTTP/1.1** and **TLS 1.2**, which is compatible with all MicroPython versions on Pico W.

---

## Tutorials & Docs

- [Official Platform Guide](https://iot.getyourprojectdone.in/examples.php)
- [Register Your Device](https://iot.getyourprojectdone.in/register.php)

---

## License

This project is licensed under the **MIT License**. See the `LICENSE` file for details.
