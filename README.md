# Embedded Disaster Alert System — Fire & Earthquake Simulator

A distributed embedded system designed to simulate and detect **earthquake** and **fire** events in real time. Five microcontrollers communicate over serial links to handle sensor acquisition, physical actuation, GSM SMS alerting, and IoT telemetry simultaneously — all coordinated by a central Arduino Mega controller.

---

## Demo

**Fire Simulation**

https://youtu.be/gObH_3M5nrk?si=5tWtFOhanTaUil6P

**Earthquake Simulation**

https://youtu.be/bN8KYFFl7Zw?si=bD3d5yVtjDiuSPyW

---

## System Architecture

```
+------------------------------------+
|     Mobile App (Firebase RTDB)     |
+------------------------------------+
                  |
               [Wi-Fi]
                  |
          +-------+--------+
          |   ESP32         |<---- ThingSpeak
          |   (IoT Bridge)  |
          +-------+---------+
                  |
              [UART Serial]
                  |
    +-------------+-----------------+
    |  Arduino Mega - Main Controller|
    |  LCD | Relays | Lights | Audio |
    +---+-------------------+--------+
        |                   |
     [UART]              [UART]
        |                   |
+-------+------+   +--------+---------------+
| Arduino Uno  |   | Arduino Mega           |
| Fire Sensors |   | Earthquake Simulator   |
| 5x IR Flame  |   | ADXL335 + DC Motor     |
+--------------+   +------------------------+
                            |
                    +-------+----------+
                    | ESP8266 NodeMCU  |
                    | GSM SMS (SIM900A)|
                    +------------------+
```

### Communication Flow
- **ESP32 → Main Mega:** Mode commands, phone number, intensity level (UART)
- **Main Mega → Uno:** Fire poll (UART Serial1)
- **Main Mega ↔ Earthquake Mega:** Intensity commands + accelerometer readings (UART Serial2)
- **Main Mega → ESP8266:** Fire/earthquake state for SMS dispatch (SoftwareSerial)
- **Main Mega → ESP32:** Live sensor values for Firebase/ThingSpeak upload (UART Serial3)

---

## Repository Structure

```
Embedded-Disaster-Alert-System-Fire-Earthquake-Simulator/
├── mega_receiver/
│   └── mega_receiver.ino       # Main controller — relay logic, LCD, serial hub
├── mega_earthquake/
│   └── mega_earthquake.ino     # Accelerometer reading + motor PWM control
├── uno_fire_sensor/
│   └── uno_fire_sensor.ino     # IR flame sensor acquisition + hold timer logic
├── esp8266_sms/
│   └── esp8266_sms.ino         # GSM SMS dispatch with deduplication
├── esp32_iot/
│   └── esp32_iot.ino           # Firebase RTDB polling + ThingSpeak telemetry
└── README.md
```

---

## Hardware

| Component | Qty | Role |
|---|---|---|
| Arduino Mega 2560 | 2 | Main controller + Earthquake simulator |
| Arduino Uno | 1 | Fire sensor acquisition node |
| ESP32 | 1 | IoT bridge — Firebase + ThingSpeak |
| ESP8266 NodeMCU | 1 | GSM SMS notification gateway |
| ADXL335 Accelerometer | 1 | 3-axis vibration sensing |
| IR Flame Sensor | 5 | Per-zone fire detection (4 floors + house) |
| SIM900A GSM Module | 1 | SMS alert delivery |
| 16x2 I2C LCD | 1 | Real-time system status display |
| DC Motor + L298N Driver | 1 | Physical earthquake shaking table |
| Relay Modules | 9 | Fan zones, alarm lights, speaker switching |

---

## Design Decisions

**Single-character serial delimiters instead of JSON**
The Arduino Uno has only 2KB of SRAM. Using a lightweight delimiter-based protocol (e.g. `1A0B1C0D0E\n`) over JSON keeps parsing fast and avoids heap fragmentation on constrained devices.

**Zone fingerprint for SMS deduplication**
Rather than sending an SMS on every loop iteration that detects fire, the ESP8266 computes a numeric fingerprint of the active zone combination (`fire1 + fire2×10 + fire3×100...`). An SMS is only dispatched when the fingerprint changes — preventing alert spam during sustained detections.

**Richter-scale intensity mapping from acceleration magnitude**
The ADXL335 outputs raw analog values. The earthquake Mega computes a 2D acceleration magnitude (`√(ΔX² + ΔY²)`) and maps it to a Richter-scale range (3.0–9.0) using linearly interpolated bands calibrated against a baseline average taken at startup.

**Hold timer on fire sensors**
IR flame sensors can produce brief drop-outs in real flame conditions. A cycle-based hold timer keeps a zone flagged as active for a configurable window after the sensor clears — reducing false all-clear signals during active fires.

**Startup motor burst on new intensity**
When the commanded earthquake intensity changes, the motor runs at full speed for 3 seconds before settling to the target PWM level. This overcomes the motor's static friction and ensures consistent startup behavior across all intensity levels.

---

## Intensity to PWM Mapping

| Richter Intensity | PWM Value | Motor Behavior |
|---|---|---|
| 4 | 36 | Mild vibration |
| 5 | 47 | Moderate vibration |
| 6 | 92 | Strong vibration |
| 7 | 142 | Very strong |
| 8 | 225 | Severe |
| 0 | 0 | Motor off |

---

## SMS Alert Examples

**Fire (dynamic zone list):**
```
Warning: Fire is detected on Second Floor and Fourth Floor.
Please stay calm and evacuate the premises immediately.
```

**Earthquake:**
```
Warning! Intensity 6.42 earthquake has been detected.
```

---

## Setup & Deployment

### 1. Install Libraries
In Arduino IDE → **Sketch → Include Library → Manage Libraries:**
- `LiquidCrystal I2C` by Frank de Brabander
- `FirebaseESP32` by Mobizt
- `ThingSpeak` by MathWorks

### 2. Configure Credentials
Open `esp32_iot/esp32_iot.ino` and fill in your credentials:
```cpp
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define FIREBASE_HOST   "https://YOUR_PROJECT_ID-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH   "YOUR_FIREBASE_SECRET_KEY"
```

### 3. Flash Each Board

| Sketch | Target Board | Notes |
|---|---|---|
| `mega_receiver.ino` | Arduino Mega | Flash first |
| `mega_earthquake.ino` | Arduino Mega | Keep powered during calibration |
| `uno_fire_sensor.ino` | Arduino Uno | — |
| `esp8266_sms.ino` | ESP8266 NodeMCU | Ensure SIM card is inserted |
| `esp32_iot.ino` | ESP32 | Flash last after Wi-Fi credentials set |

### 4. Power-On Sequence
1. Arduino Uno (fire sensors)
2. Arduino Mega — Earthquake
3. Arduino Mega — Main Controller
4. ESP8266
5. ESP32

---

## Known Issues / Limitations

- `SoftwareSerial` on the ESP8266 can miss bytes at higher baud rates — keep all ports at 9600 baud
- ThingSpeak free tier enforces a 15-second minimum between updates; the 3-second interval in the code will result in throttled uploads on free accounts
- ADXL335 calibration is done at boot — ensure the hardware is completely stationary during the 3-second startup window

---

## Author

**Mark Anthony A. Lim**
[LinkedIn](https://www.linkedin.com/in/mark-lim-487278398/) 
[GitHub]
(https://github.com/MarkLim15)
