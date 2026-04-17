"""
════════════════════════════════════════════════════════════════
 Raspberry Pi Pico W — Multi-Sensor Monitor  (MicroPython)
 •  Add unlimited sensors in 2 steps  •
════════════════════════════════════════════════════════════════

 HOW TO ADD A NEW SENSOR:
  Step 1 — Write a function that returns a float value
  Step 2 — Add one dict to the SENSORS list below

 FIRMWARE: MicroPython for Pico W
   https://micropython.org/download/rp2-pico-w/

 NOTE: For a full Raspberry Pi (Linux/Raspbian), see the note
 at the bottom of this file — uses the standard `requests` lib.
════════════════════════════════════════════════════════════════
"""

import network
import urequests
import ujson
import utime

# ── STEP 0 — Credentials & server ────────────────────────────────
WIFI_SSID     = "Your-SSID"
WIFI_PASSWORD = "Your-Password"
API_KEY       = "Your-API-Key"   # from dashboard → API Key pill

# Production server (HTTPS)
SERVER = "https://iot.getyourprojectdone.in"


SEND_INTERVAL = 10   # seconds between sends
# ─────────────────────────────────────────────────────────────────


# ════════════════════════════════════════════════════════════════
#  STEP 1 — Write one function per sensor.
#  Each function must return a float.
#  Replace the simulated values with real sensor reads.
#  See the SENSOR SNIPPETS section at the bottom for DHT11,
#  DS18B20, BMP280, HC-SR04, MQ-2, LDR, and more.
# ════════════════════════════════════════════════════════════════

def read_temperature():
    # TODO: e.g. from machine import ADC (onboard sensor)
    # or use DHT11/DHT22 library
    return 27.5   # simulated

def read_humidity():
    # TODO: e.g. dht_sensor.humidity()
    return 62.0   # simulated

def read_gas():
    # TODO: e.g. ADC(26).read_u16() / 65535 * 100
    return 350.0  # simulated

def read_distance():
    # TODO: HC-SR04 pulse measurement
    return 115.0  # simulated


# ════════════════════════════════════════════════════════════════
#  STEP 2 — Register sensors in this list.
#  Add or remove dicts freely — nothing else needs to change.
#
#  Keys:
#    "device"  — must EXACTLY match name on your Devices page
#    "type"    — temperature | humidity | gas | distance |
#                pressure | light | ph | co2 | motion | custom
#    "unit"    — C | F | % | ppm | cm | hPa | lux | V | etc.
#    "fn"      — the function you wrote in Step 1
# ════════════════════════════════════════════════════════════════

SENSORS = [
    # { "device": "device_name", "type": "sensor_type", "unit": "unit", "fn": read_function },
    { "device": "temp_sensor",  "type": "temperature", "unit": "C",   "fn": read_temperature },
    { "device": "hum_sensor",   "type": "humidity",    "unit": "%",   "fn": read_humidity    },
    { "device": "gas_sensor",   "type": "gas",         "unit": "ppm", "fn": read_gas         },
    { "device": "ultra_sensor", "type": "distance",    "unit": "cm",  "fn": read_distance    },

    # Add more dicts here:
    # { "device": "pressure_sensor", "type": "pressure", "unit": "hPa", "fn": read_pressure },
]

# ─────────────────────────────────────────────────────────────────
# Internals — no changes needed below this line
# ─────────────────────────────────────────────────────────────────

def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if wlan.isconnected():
        return wlan

    print(f"[WiFi] Connecting to {WIFI_SSID}", end="")
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)

    for _ in range(30):
        if wlan.isconnected():
            break
        utime.sleep(1)
        print(".", end="")

    if wlan.isconnected():
        print(f" connected!\n[WiFi] IP: {wlan.ifconfig()[0]}")
    else:
        print("\n[WiFi] FAILED — check SSID/password")

    return wlan


def send_all_sensors():
    # Read all values
    readings = []
    for s in SENSORS:
        try:
            val = s["fn"]()
            readings.append((s, round(float(val), 2)))
            print(f"[Sensor] {s['device']:<18} = {val} {s['unit']}")
        except Exception as e:
            print(f"[Sensor] {s['device']} ERROR: {e}")

    if not readings:
        print("[Send] No valid readings — skipping")
        return

    # Build payload
    payload = {
        "api_key": API_KEY,
        "devices": [
            {
                "device_name": s["device"],
                "sensors": [
                    {
                        "type":  s["type"],
                        "value": str(val),
                        "unit":  s["unit"]
                    }
                ]
            }
            for s, val in readings
        ]
    }

    url  = SERVER + "/api/insert_data.php"
    body = ujson.dumps(payload)

    print(f"[HTTP] POST → {url}")

    try:
        resp = urequests.post(
            url,
            data=body,
            headers={"Content-Type": "application/json"}
        )
        result = resp.text
        resp.close()
        print(f"[Server] {result}")

    except OSError as e:
        print(f"[HTTP] ERROR: {e}")
        print("       Check SERVER and network connection")


def main():
    print("\n══════════════════════════════════")
    print("  Pico W Multi-Sensor Monitor")
    print(f"  Sensors  : {len(SENSORS)}")
    print(f"  Interval : {SEND_INTERVAL}s")
    print("══════════════════════════════════\n")

    wlan = connect_wifi()
    cycle = 0

    while True:
        if not wlan.isconnected():
            print("[WiFi] Connection lost — reconnecting...")
            wlan = connect_wifi()

        cycle += 1
        print(f"\n─── Cycle #{cycle} ───────────────────────")
        send_all_sensors()
        utime.sleep(SEND_INTERVAL)


main()


# ════════════════════════════════════════════════════════════════
#  SENSOR SNIPPETS — copy what you need into Step 1
# ════════════════════════════════════════════════════════════════
#
#  ── Onboard temperature (Pico W only) ──────────────────────────
#  from machine import ADC
#  _adc = ADC(4)
#  def read_temperature():
#      v = _adc.read_u16() * 3.3 / 65535
#      return round(27 - (v - 0.706) / 0.001721, 1)
#
#  ── DHT11 / DHT22 ───────────────────────────────────────────────
#  import dht
#  from machine import Pin
#  _dht = dht.DHT11(Pin(15))   # or DHT22
#  def read_temperature():
#      _dht.measure(); return _dht.temperature()
#  def read_humidity():
#      _dht.measure(); return _dht.humidity()
#
#  ── HC-SR04 Ultrasonic ──────────────────────────────────────────
#  from machine import Pin, time_pulse_us
#  _trig = Pin(14, Pin.OUT)
#  _echo = Pin(15, Pin.IN)
#  def read_distance():
#      _trig.low(); utime.sleep_us(2)
#      _trig.high(); utime.sleep_us(10); _trig.low()
#      dur = time_pulse_us(_echo, 1, 30000)
#      return round(dur / 58.0, 1) if dur > 0 else 0
#
#  ── MQ-2 Gas sensor (ADC) ───────────────────────────────────────
#  from machine import ADC
#  _adc = ADC(26)              # GP26 = ADC0
#  def read_gas():
#      return round(_adc.read_u16() / 65535 * 100, 1)
#
#  ── LDR Light sensor ────────────────────────────────────────────
#  from machine import ADC
#  _ldr = ADC(27)              # GP27 = ADC1
#  def read_light():
#      return round(_ldr.read_u16() / 65535 * 100, 1)
#
# ════════════════════════════════════════════════════════════════
#  FOR FULL RASPBERRY PI (Raspbian / Linux)
#  Replace the entire file with this simpler version:
#
#  pip3 install requests
#
#  import requests, time
#
#  SERVER = "https://iot.getyourprojectdone.in"
#  API_KEY = "Your-API-Key"
#
#  SENSORS = [
#      { "device": "temp_sensor",  "type": "temperature", "unit": "C",  "fn": lambda: 28.5 },
#      { "device": "hum_sensor",   "type": "humidity",    "unit": "%",  "fn": lambda: 60.0 },
#  ]
#
#  while True:
#      payload = {
#          "api_key": API_KEY,
#          "devices": [
#              {"device_name": s["device"],
#               "sensors": [{"type": s["type"], "value": str(s["fn"]()), "unit": s["unit"]}]}
#              for s in SENSORS
#          ]
#      }
#      r = requests.post(SERVER + "/api/insert_data.php", json=payload)
#      print(r.json())
#      time.sleep(10)
# ════════════════════════════════════════════════════════════════
