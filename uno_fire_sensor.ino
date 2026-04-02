/*
 * Earthquake and Fire Simulation System
 * File: uno_fire_sensor.ino
 * Board: Arduino Uno (Fire Sensor Node)
 *
 * Responsibilities:
 *  - Reads 5 IR flame sensors (one per floor + house)
 *  - Applies a hold timer so a detection stays active for a brief period
 *    even if the sensor briefly loses the flame signal
 *  - Transmits fire zone states to the Main Mega via SoftwareSerial
 */

#include <SoftwareSerial.h>

SoftwareSerial dataOut(6, 7);  // RX, TX → to Main Mega Serial1

// ── Flame sensor analog pins ──────────────────────────────────────────────────
// Sensors output LOW voltage when flame is detected
#define SENSOR_1 A5  // Floor 1
#define SENSOR_2 A4  // Floor 2
#define SENSOR_3 A3  // Floor 3
#define SENSOR_4 A2  // Floor 4
#define SENSOR_5 A1  // House

// Detection threshold: sensor reads below this value → fire detected
#define FIRE_THRESHOLD 100

// ── Fire zone states ──────────────────────────────────────────────────────────
int f1 = 0, f2 = 0, f3 = 0, f4 = 0, f5 = 0;

// ── Hold timers (keep zone active for N cycles after sensor clears) ───────────
// Floors 1–4 hold for 22 cycles; house (f5) holds for 14 cycles
int hold1 = 0, hold2 = 0, hold3 = 0, hold4 = 0, hold5 = 0;
const int HOLD_CYCLES_FLOOR = 22;
const int HOLD_CYCLES_HOUSE = 14;

void setup() {
  Serial.begin(9600);
  dataOut.begin(9600);
}

void loop() {
  // ── Read sensors ────────────────────────────────────────────────────────────
  int raw1 = analogRead(SENSOR_1);
  int raw2 = analogRead(SENSOR_2);
  int raw3 = analogRead(SENSOR_3);
  int raw4 = analogRead(SENSOR_4);
  int raw5 = analogRead(SENSOR_5);

  // ── Detect and apply hold logic ─────────────────────────────────────────────
  f1 = detectFire(raw1, f1, hold1, HOLD_CYCLES_FLOOR, "Floor 1");
  f2 = detectFire(raw2, f2, hold2, HOLD_CYCLES_FLOOR, "Floor 2");
  f3 = detectFire(raw3, f3, hold3, HOLD_CYCLES_FLOOR, "Floor 3");
  f4 = detectFire(raw4, f4, hold4, HOLD_CYCLES_FLOOR, "Floor 4");
  f5 = detectFire(raw5, f5, hold5, HOLD_CYCLES_HOUSE,  "House");

  // ── Transmit zone states to Main Mega ───────────────────────────────────────
  dataOut.print(f1); dataOut.print("A");
  dataOut.print(f2); dataOut.print("B");
  dataOut.print(f3); dataOut.print("C");
  dataOut.print(f4); dataOut.print("D");
  dataOut.print(f5); dataOut.print("E");
  dataOut.print("\n");

  delay(250);
}

// ── Helper: evaluate one sensor with hold timer ───────────────────────────────
// Returns the updated fire state (0 or 1) and updates the hold counter by ref.
int detectFire(int raw, int currentState, int &hold, int maxHold, const char* label) {
  if (raw < FIRE_THRESHOLD) {
    // Active detection — set state and reset hold timer
    Serial.print("Fire detected at "); Serial.println(label);
    hold = 1;
    return 1;
  } else {
    Serial.print(label); Serial.println(" safe");

    if (hold > 0) {
      hold++;
      if (hold < maxHold) {
        return 1;  // still within hold window
      } else {
        hold = 0;  // hold expired
        return 0;
      }
    }
    return 0;
  }
}
