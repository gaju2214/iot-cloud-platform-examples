"""
════════════════════════════════════════════════════════════════
 Raspberry Pi Pico W — Multi-Device Control  (MicroPython)
  •  Add unlimited control devices in 2 steps  • 
════════════════════════════════════════════════════════════════

 HOW IT WORKS:
  Every POLL_INTERVAL seconds → fetches pending commands
  Calls the matching handler function for each command
  Acknowledges each command back to the server

 HOW TO ADD A NEW CONTROL DEVICE:
  Step 1 — Write a handler function for the device
  Step 2 — Add one dict to DEVICES list below

 Command types from dashboard:
   "ON"        → switch on  (relay, LED, buzzer …)
   "OFF"       → switch off
   "SET_VALUE" → set to a level (fan speed %, servo angle …)
════════════════════════════════════════════════════════════════
"""

import network
import urequests
import ujson
import utime
from machine import Pin, PWM

# ── STEP 0 — Credentials & server ────────────────────────────────
WIFI_SSID     = "Your-SSID"
WIFI_PASSWORD = "Your-Password"
API_KEY       = "Your-API-Key"

SERVER = "https://iot.getyourprojectdone.in"


POLL_INTERVAL = 5   # seconds
# ─────────────────────────────────────────────────────────────────


# ════════════════════════════════════════════════════════════════
#  STEP 1 — Define pins and write one handler per device
#
#  Handler signature:  def handle_xxx(cmd_type, cmd_value):
#    cmd_type  : "ON" | "OFF" | "SET_VALUE"
#    cmd_value : string, e.g. "75" for SET_VALUE
# ════════════════════════════════════════════════════════════════

# ── Onboard LED (GP25 on Pico W) ─────────────────────────────────
_led = Pin("LED", Pin.OUT)   # built-in LED on Pico W
def handle_led(cmd_type, cmd_value):
    if cmd_type == "ON":
        _led.on()
        print("[LED] ON")
    elif cmd_type == "OFF":
        _led.off()
        print("[LED] OFF")

# ── External Relay (active HIGH — wire to any GP pin) ─────────────
_relay = Pin(15, Pin.OUT, value=0)   # GP15
def handle_relay(cmd_type, cmd_value):
    if cmd_type == "ON":
        _relay.on()
        print("[Relay] ON")
    elif cmd_type == "OFF":
        _relay.off()
        print("[Relay] OFF")

# ── Fan via PWM (SET_VALUE = 0-100 percent) ───────────────────────
_fan_pwm = PWM(Pin(14))              # GP14
_fan_pwm.freq(1000)
def handle_fan(cmd_type, cmd_value):
    if cmd_type == "ON":
        _fan_pwm.duty_u16(65535)
        print("[Fan] FULL ON")
    elif cmd_type == "OFF":
        _fan_pwm.duty_u16(0)
        print("[Fan] OFF")
    elif cmd_type == "SET_VALUE":
        pct = max(0, min(100, int(cmd_value)))
        duty = int(pct / 100 * 65535)
        _fan_pwm.duty_u16(duty)
        print(f"[Fan] Speed = {pct}%")

# ── Servo (SET_VALUE = angle 0-180) ──────────────────────────────
# _servo_pwm = PWM(Pin(13))    # GP13
# _servo_pwm.freq(50)          # 50 Hz for servo
# def _angle_to_duty(deg):
#     # 0.5ms=0° → 2.5ms=180° at 50Hz (period=20ms)
#     us = 500 + int(deg / 180 * 2000)
#     return int(us / 20000 * 65535)
# def handle_servo(cmd_type, cmd_value):
#     if cmd_type == "SET_VALUE":
#         angle = max(0, min(180, int(cmd_value)))
#         _servo_pwm.duty_u16(_angle_to_duty(angle))
#         print(f"[Servo] Angle = {angle}°")
#     elif cmd_type == "ON":  _servo_pwm.duty_u16(_angle_to_duty(180))
#     elif cmd_type == "OFF": _servo_pwm.duty_u16(_angle_to_duty(0))

# ── Buzzer ────────────────────────────────────────────────────────
# _buzzer = Pin(12, Pin.OUT, value=0)
# def handle_buzzer(cmd_type, cmd_value):
#     if cmd_type == "ON":        _buzzer.on()
#     elif cmd_type == "OFF":     _buzzer.off()
#     elif cmd_type == "SET_VALUE":
#         _buzzer.on(); utime.sleep_ms(int(cmd_value)); _buzzer.off()


# ════════════════════════════════════════════════════════════════
#  STEP 2 — Register devices in this list
#  "device" must EXACTLY match name on your Devices page
#  (Device Type must be a control type: relay, led, fan, servo…)
# ════════════════════════════════════════════════════════════════

DEVICES = [
    # { "device": "device_name", "handler": handle_function },
    { "device": "led",   "handler": handle_led   },
    { "device": "relay", "handler": handle_relay },
    { "device": "fan",   "handler": handle_fan   },

    # Uncomment to add:
    # { "device": "servo",   "handler": handle_servo  },
    # { "device": "buzzer",  "handler": handle_buzzer },
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
        utime.sleep(1); print(".", end="")

    if wlan.isconnected():
        print(f" connected!\n[WiFi] IP: {wlan.ifconfig()[0]}")
    else:
        print("\n[WiFi] FAILED")
    return wlan


def http_post(url, body):
    try:
        resp = urequests.post(
            url,
            data=body,
            headers={"Content-Type": "application/json"}
        )
        text = resp.text
        resp.close()
        return text
    except OSError as e:
        print(f"[HTTP] Error: {e}")
        return ""


def poll_device(device_name, handler):
    payload = ujson.dumps({"api_key": API_KEY, "device_name": device_name})
    url  = SERVER + "/api/device_poll.php"
    resp = http_post(url, payload)

    if not resp:
        return

    try:
        data = ujson.loads(resp)
    except Exception:
        print(f"[Poll] Parse error: {resp}")
        return

    if data.get("status") != "success":
        print(f"[Poll] {device_name} — {data.get('message', 'error')}")
        return

    commands = data.get("commands", [])
    if not commands:
        return

    print(f"[Poll] {device_name} — {len(commands)} command(s)")

    for cmd in commands:
        cmd_id  = cmd.get("id", 0)
        c_type  = cmd.get("command_type", "")
        c_value = cmd.get("command_value", "")

        print(f"[CMD]  #{cmd_id}  {device_name} → type={c_type}  value={c_value}")

        try:
            handler(c_type, c_value)
            ack_status = "executed"
        except Exception as e:
            print(f"[CMD]  Handler error: {e}")
            ack_status = "failed"

        ack_command(cmd_id, ack_status)
        utime.sleep_ms(100)


def ack_command(cmd_id, status):
    payload = ujson.dumps({
        "api_key":    API_KEY,
        "command_id": cmd_id,
        "status":     status
    })
    resp = http_post(SERVER + "/api/device_ack.php", payload)
    print(f"[Ack]  #{cmd_id} → {resp if resp else 'no response'}")


def main():
    print("\n══════════════════════════════════")
    print("  Pico W Multi-Device Control")
    print(f"  Devices  : {len(DEVICES)}")
    print(f"  Interval : {POLL_INTERVAL}s")
    print("══════════════════════════════════\n")

    wlan = connect_wifi()
    cycle = 0

    while True:
        if not wlan.isconnected():
            print("[WiFi] Reconnecting...")
            wlan = connect_wifi()

        cycle += 1
        print(f"\n─── Poll cycle #{cycle} ───")

        for dev in DEVICES:
            poll_device(dev["device"], dev["handler"])

        utime.sleep(POLL_INTERVAL)


main()
