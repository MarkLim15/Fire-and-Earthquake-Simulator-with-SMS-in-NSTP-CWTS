# 🔥 Earthquake and Fire Simulation System

A multi-board embedded system that simulates and detects **earthquake** and **fire** events in real time. Built as a capstone thesis project at Rizal Technological University.

The system uses multiple microcontrollers working together to detect sensor events, trigger physical simulations, display status on an LCD, send SMS alerts, and upload live data to an IoT dashboard.

---

## 📷 Demo

**Earthquake Simulation**
>> https://youtu.be/bN8KYFFl7Zw

**Fire Simulation**
>> https://youtu.be/gObH_3M5nrk
---

## 🧠 System Architecture

```
┌─────────────────────────────────────────┐
│           Mobile App (Firebase)         │
└────────────────┬────────────────────────┘
                 │ Wi-Fi
         ┌───────▼────────┐
         │   ESP32 (IoT)  │◄──── ThingSpeak Dashboard
         └───────┬────────┘
                 │ Serial
     ┌───────────▼──────────────┐
     │  Arduino Mega (Receiver) │  ◄── Main Controller
     │  LCD | Relays | Lights   │
     └──┬──────────┬────────────┘
        │          │ Serial
        │    ┌─────▼──────────────────┐
        │    │ Arduino Mega           │
        │    │ (Earthquake Simulator) │
        │    │ Accelerometer + Motor  │
        │    └────────────────────────┘
        │ Serial
  ┌─────▼──────────────────┐      ┌──────────────────────┐
  │ Arduino Uno            │      │ ESP8266 (SMS)         │
  │ (Fire Sensor Node)     │      │ SIM900A GSM Module    │
  │ 5x IR Flame Sensors    │      │ SMS Alerts            │
  └────────────────────────┘      └──────────────────────┘
```

---

## 🗂️ Repository Structure

```
arduino-fire-earthquake-alarm/
├── mega_receiver/
│   └── mega_receiver.ino       # Main controller (Arduino Mega)
├── mega_earthquake/
│   └── mega_earthquake.ino     # Earthquake simulator (Arduino Mega)
├── uno_fire_sensor/
│   └── uno_fire_sensor.ino     # Fire sensor node (Arduino Uno)
├── esp8266_sms/
│   └── esp8266_sms.ino         # GSM SMS alerts (ESP8266 NodeMCU)
├── esp32_iot/
│   └── esp32_iot.ino           # IoT dashboard bridge (ESP32)
└── README.md
```

---

## 🔧 Hardware Components

| Component | Quantity | Purpose |
|---|---|---|
| Arduino Mega 2560 | 2 | Main controller + Earthquake simulator |
| Arduino Uno | 1 | Fire sensor node |
| ESP32 | 1 | Firebase + ThingSpeak IoT |
| ESP8266 NodeMCU | 1 | GSM SMS notifications |
| ADXL335 Accelerometer | 1 | Measures earthquake vibration |
| IR Flame Sensor | 5 | Detects fire per floor + house |
| SIM900A GSM Module | 1 | Sends SMS alerts |
| 16x2 I2C LCD Display | 1 | Real-time status display |
| DC Motor + Driver | 1 | Physical earthquake shaking |
| Relay Modules | 9 | Controls fans, lights, and speaker |
| Buzzer / Speaker | 1 | Audio alarm |

---

## ⚙️ How It Works

### Modes
The system operates in 3 modes controlled via the mobile app:

| Mode | Description |


| **Home (0)** | Idle — all outputs off |
| **Fire Simulator (1)** | Monitors 5 flame sensors; triggers relays, lights, speaker, and SMS on detection|
| **Earthquake Simulator (2)** | Reads accelerometer; maps vibration to Richter scale (3–9); drives motor and sends SMS|

### Fire Detection
- 5 IR flame sensors monitor individual zones (Floors 1–4 and House)
- A hold timer keeps a zone active briefly even if the sensor momentarily loses signal
- SMS is only sent when the active zone pattern changes (deduplication)

### Earthquake Detection
- The ADXL335 accelerometer measures X/Y/Z acceleration deltas from a calibrated baseline
- Magnitude is mapped to a Richter-scale intensity value (3.0 – 9.0)
- The DC motor runs at a speed corresponding to the commanded intensity level
- SMS is triggered once per event (requires signal to drop then rise again)

### IoT Integration
- **Firebase Realtime Database** — receives mode, phone number, and intensity commands from the mobile app
- **ThingSpeak** — receives live sensor values every 3 seconds for charting

---

## 📲 SMS Alert Examples

**Fire:**
```
Warning: Fire is detected on Second Floor and Fourth Floor.
Please stay calm and evacuate the premises immediately.
```

**Earthquake:**
```
Warning! Intensity 6.42 earthquake has been detected.
```

---

## 🚀 Setup Instructions

### 1. Install Required Libraries
In Arduino IDE, go to **Sketch → Include Library → Manage Libraries** and install:
- `LiquidCrystal I2C` by Frank de Brabander
- `FirebaseESP32` by Mobizt
- `ThingSpeak` by MathWorks

### 2. Configure Credentials (ESP32)
Open `esp32_iot/esp32_iot.ino` and replace the placeholders:
```cpp
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define FIREBASE_HOST   "https://YOUR_PROJECT_ID-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH   "YOUR_FIREBASE_SECRET_KEY"
```

### 3. Upload Each Sketch to Its Board

| File | Target Board |
|---|---|
| `mega_receiver.ino` | Arduino Mega (main) |
| `mega_earthquake.ino` | Arduino Mega (earthquake) |
| `uno_fire_sensor.ino` | Arduino Uno |
| `esp8266_sms.ino` | ESP8266 NodeMCU |
| `esp32_iot.ino` | ESP32 |

### 4. Power On Order
1. Arduino Uno (fire sensors)
2. Arduino Mega — Earthquake
3. Arduino Mega — Receiver (main)
4. ESP8266
5. ESP32

---

## 👨‍💻 Authors

**Mark Anthony A. Lim** — Rizal Technological University
Bachelor of Science in Computer Engineering, 2024

---

## 📄 License

This project was developed as an academic thesis. Feel free to use it as a reference for your own embedded systems projects.
