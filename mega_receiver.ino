/*
 * Earthquake and Fire Simulation System
 * File: mega_receiver.ino
 * Board: Arduino Mega (Main Controller / Receiver)
 *
 * Responsibilities:
 *  - Receives mode/intensity commands from ESP32 (Serial3)
 *  - Receives fire sensor states from Arduino Uno (Serial1)
 *  - Receives accelerometer data from Arduino Mega Earthquake (Serial2)
 *  - Controls relay outputs for fire zones, speakers, and lights
 *  - Displays system status on 16x2 I2C LCD
 *  - Forwards data to ESP8266 SMS module (SoftwareSerial) and ESP32 IoT (Serial3)
 */

#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ── GSM module (ESP8266 SMS bridge) ──────────────────────────────────────────
SoftwareSerial mySerial(2, 3);  // RX, TX

// ── LCD ──────────────────────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ── Relay pin assignments ─────────────────────────────────────────────────────
const int RELAY_FIRE1     = 48;
const int RELAY_FIRE2     = 46;
const int RELAY_FIRE3     = 44;
const int RELAY_FIRE4     = 42;
const int RELAY_FIRE5     = 40;
const int RELAY_SPEAKER1  = 38;
const int RELAY_SPEAKER2  = 36;
const int RELAY_LIGHT1    = 33;
const int RELAY_LIGHT2    = 35;

// ── Serial protocol state ─────────────────────────────────────────────────────
char c;
String dataIn = "";

// ── Screen / mode control ─────────────────────────────────────────────────────
// 0 = Home, 1 = Fire Simulator, 2 = Earthquake Simulator
int screen      = 0;
int clearscreen = -1;  // tracks last cleared mode to avoid redundant lcd.clear()

// ── Phone number ──────────────────────────────────────────────────────────────
String PhoneNumber        = "";
String CurrentPhoneNumber = "639672932231";  // default recipient

// ── IoT parsed strings ────────────────────────────────────────────────────────
String Screenval = "";
String Inten     = "";

// ── Earthquake state ──────────────────────────────────────────────────────────
String MotorV    = "";
String s2        = "";
float  MotorVal  = 0.0;
int    Intensity = 0;
int    lightblink = 0;

// ── Fire state ────────────────────────────────────────────────────────────────
String f1, f2, f3, f4, f5;
int fire1 = 0, fire2 = 0, fire3 = 0, fire4 = 0, fire5 = 0;

// ── Speaker cycling counter ───────────────────────────────────────────────────
int speak = 0;

// ── Helper: set all relay outputs HIGH (off) ──────────────────────────────────
void allFireRelaysOff() {
  digitalWrite(RELAY_FIRE1, HIGH);
  digitalWrite(RELAY_FIRE2, HIGH);
  digitalWrite(RELAY_FIRE3, HIGH);
  digitalWrite(RELAY_FIRE4, HIGH);
  digitalWrite(RELAY_FIRE5, HIGH);
}

void setup() {
  // Relay pins
  int relays[] = {
    RELAY_FIRE1, RELAY_FIRE2, RELAY_FIRE3, RELAY_FIRE4, RELAY_FIRE5,
    RELAY_SPEAKER1, RELAY_SPEAKER2, RELAY_LIGHT1, RELAY_LIGHT2
  };
  for (int i = 0; i < 9; i++) {
    pinMode(relays[i], OUTPUT);
    digitalWrite(relays[i], HIGH);  // relays active-LOW; HIGH = off
  }

  // Lights on during startup splash
  digitalWrite(RELAY_LIGHT1, LOW);
  digitalWrite(RELAY_LIGHT2, LOW);

  // LCD startup
  lcd.init();
  lcd.backlight();
  lcd.setCursor(1, 0); lcd.print("Earthquake And");
  lcd.setCursor(1, 1); lcd.print("Fire Simulator");

  // Serial ports
  Serial.begin(9600);   // USB monitor
  Serial1.begin(9600);  // Arduino Uno (fire sensors)
  Serial2.begin(9600);  // Arduino Mega (earthquake)
  Serial3.begin(9600);  // ESP32 (IoT / app commands)
  mySerial.begin(9600); // ESP8266 (GSM SMS)

  delay(3000);
  lcd.clear();
}

void loop() {
  // ── Read commands from ESP32 (app) via Serial3 ──────────────────────────────
  while (Serial3.available() > 0) {
    c = Serial3.read();
    if (c == '\n') break;
    else dataIn += c;
  }
  if (c == '\n') {
    parseIoTData();
    c = 0;
    dataIn = "";
  }

  // ── Mode: Fire Simulator ───────────────────────────────────────────────────
  if (screen == 1) {
    if (clearscreen != 1) {
      clearscreen = 1;
      lcd.clear();
    }
    lcd.setCursor(1, 0);
    lcd.print("Fire Simulator");

    // Read fire sensor data from Arduino Uno
    while (Serial1.available() > 0) {
      c = Serial1.read();
      if (c == '\n') break;
      else dataIn += c;
    }

    if (c == '\n') {
      parseFireData();

      // Update LCD zone indicators and relays
      updateFireZone(fire1, RELAY_FIRE1, 1,  "1");
      updateFireZone(fire2, RELAY_FIRE2, 4,  "2");
      updateFireZone(fire3, RELAY_FIRE3, 7,  "3");
      updateFireZone(fire4, RELAY_FIRE4, 10, "4");
      updateFireZone(fire5, RELAY_FIRE5, 13, "H");

      // Light control based on active zones
      bool roomFire = (fire1 || fire2 || fire3 || fire4);
      bool houseFire = fire5;

      if (!roomFire && !houseFire) {
        digitalWrite(RELAY_LIGHT1, LOW);
        digitalWrite(RELAY_LIGHT2, LOW);
        digitalWrite(RELAY_SPEAKER1, HIGH);
        digitalWrite(RELAY_SPEAKER2, HIGH);
        speak = 0;
      } else if (roomFire && houseFire) {
        digitalWrite(RELAY_LIGHT1, HIGH);
        digitalWrite(RELAY_LIGHT2, HIGH);
      } else if (roomFire) {
        digitalWrite(RELAY_LIGHT1, HIGH);
        digitalWrite(RELAY_LIGHT2, LOW);
      } else {
        digitalWrite(RELAY_LIGHT1, LOW);
        digitalWrite(RELAY_LIGHT2, HIGH);
      }

      // Speaker cycling when fire is active
      if (roomFire || houseFire) {
        if (speak <= 2) {
          digitalWrite(RELAY_SPEAKER1, LOW);
          digitalWrite(RELAY_SPEAKER2, LOW);
        } else if (speak <= 4) {
          digitalWrite(RELAY_SPEAKER1, HIGH);
          digitalWrite(RELAY_SPEAKER2, LOW);
        } else {
          speak = 0;
        }
        speak++;
      }

      c = 0;
      dataIn = "";
    }

    // Forward fire state to ESP8266 SMS module
    mySerial.print(PhoneNumber);
    mySerial.print("R"); mySerial.print(screen);
    mySerial.print("S"); mySerial.print(fire1);
    mySerial.print("U"); mySerial.print(fire2);
    mySerial.print("V"); mySerial.print(fire3);
    mySerial.print("W"); mySerial.print(fire4);
    mySerial.print("X"); mySerial.print(fire5);
    mySerial.print("Y"); mySerial.print("\n");

    // Forward fire state to ESP32 IoT
    Serial3.print(fire1); Serial3.print("a");
    Serial3.print(fire2); Serial3.print("b");
    Serial3.print(fire3); Serial3.print("c");
    Serial3.print(fire4); Serial3.print("d");
    Serial3.print(fire5); Serial3.print("e");
    Serial3.print("\n");

    // Tell earthquake Mega to stay idle
    Serial2.print(0); Serial2.print("I");
    Serial2.print(44); Serial2.print("J");
    Serial2.print("\n");

    delay(250);
    c = 0;
    dataIn = "";

  // ── Mode: Earthquake Simulator ─────────────────────────────────────────────
  } else if (screen == 2) {
    if (clearscreen != 2) {
      clearscreen = 2;
      lcd.clear();
    }
    lcd.setCursor(1, 0);
    lcd.print("Earthquake SMT");

    // Read accelerometer data from earthquake Mega
    while (Serial2.available() > 0) {
      c = Serial2.read();
      if (c == '\n') break;
      else dataIn += c;
    }

    if (c == '\n') {
      parseEarthquakeData();

      lcd.setCursor(0, 1); lcd.print("Intensity = ");
      lcd.setCursor(12, 1); lcd.print("    ");
      lcd.setCursor(12, 1); lcd.print(MotorVal);

      if (MotorVal >= 3.50) {
        digitalWrite(RELAY_SPEAKER1, LOW);
        digitalWrite(RELAY_SPEAKER2, LOW);
        // Alternate lights to simulate alarm
        if (lightblink == 0) {
          lightblink = 1;
          digitalWrite(RELAY_LIGHT1, HIGH);
          digitalWrite(RELAY_LIGHT2, LOW);
        } else {
          lightblink = 0;
          digitalWrite(RELAY_LIGHT1, LOW);
          digitalWrite(RELAY_LIGHT2, HIGH);
        }
      } else {
        digitalWrite(RELAY_SPEAKER1, HIGH);
        digitalWrite(RELAY_SPEAKER2, HIGH);
        digitalWrite(RELAY_LIGHT1, LOW);
        digitalWrite(RELAY_LIGHT2, LOW);
      }

      c = 0;
      dataIn = "";
    }

    allFireRelaysOff();

    // Send intensity command to earthquake Mega
    Serial2.print(Intensity); Serial2.print("I");
    Serial2.print(44); Serial2.print("J");
    Serial2.print("\n");

    // Forward earthquake state to ESP8266 SMS module
    mySerial.print(PhoneNumber);
    mySerial.print("R"); mySerial.print(screen);
    mySerial.print("S"); mySerial.print(MotorVal);
    mySerial.print("u"); mySerial.print(22);
    mySerial.print("v"); mySerial.print("\n");

    // Forward earthquake value to ESP32 IoT
    Serial3.print(MotorVal); Serial3.print("m");
    Serial3.print(22); Serial3.print("n");
    Serial3.print("\n");

    delay(250);
    c = 0;
    dataIn = "";

  // ── Mode: Home (idle) ──────────────────────────────────────────────────────
  } else if (screen == 0) {
    if (clearscreen != 0) {
      clearscreen = 0;
      lcd.clear();
    }
    lcd.setCursor(1, 0); lcd.print("Earthquake And");
    lcd.setCursor(1, 1); lcd.print("Fire Simulator");

    allFireRelaysOff();
    digitalWrite(RELAY_SPEAKER1, HIGH);
    digitalWrite(RELAY_SPEAKER2, HIGH);

    // Keep earthquake Mega idle
    Serial2.print(0); Serial2.print("I");
    Serial2.print(44); Serial2.print("J");
    Serial2.print("\n");

    // Notify SMS module of idle state
    mySerial.print(PhoneNumber);
    mySerial.print("R"); mySerial.print(screen);
    mySerial.print("S"); mySerial.print(0);
    mySerial.print("u"); mySerial.print(22);
    mySerial.print("v"); mySerial.print("\n");

    delay(250);
    c = 0;
    dataIn = "";
  }
}

// ── Helper: update one fire zone relay and LCD label ──────────────────────────
void updateFireZone(int state, int relayPin, int lcdCol, const char* label) {
  if (state == 1) {
    digitalWrite(relayPin, LOW);
    lcd.setCursor(lcdCol, 1);
    lcd.print(String(label) + "X");
  } else {
    digitalWrite(relayPin, HIGH);
    lcd.setCursor(lcdCol, 1);
    lcd.print(String(label) + " ");
  }
}

// ── Parse: commands from ESP32 IoT app ───────────────────────────────────────
void parseIoTData() {
  int8_t ir = dataIn.indexOf("r");
  int8_t is = dataIn.indexOf("s");
  int8_t it = dataIn.indexOf("t");

  PhoneNumber = dataIn.substring(0, ir);
  Screenval   = dataIn.substring(ir + 1, is);
  Inten       = dataIn.substring(is + 1, it);

  if (PhoneNumber.length() == 12) {
    CurrentPhoneNumber = PhoneNumber;
  }
  if (Screenval == "0" || Screenval == "1" || Screenval == "2") {
    screen = Screenval.toInt();
  }
  if (Inten == "101") {
    Intensity = 0;
  } else if (Inten == "0" || Inten == "4" || Inten == "5" ||
             Inten == "6" || Inten == "7" || Inten == "8") {
    Intensity = Inten.toInt();
  }
}

// ── Parse: fire zone states from Arduino Uno ─────────────────────────────────
void parseFireData() {
  int8_t iA = dataIn.indexOf("A");
  int8_t iB = dataIn.indexOf("B");
  int8_t iC = dataIn.indexOf("C");
  int8_t iD = dataIn.indexOf("D");
  int8_t iE = dataIn.indexOf("E");

  f1 = dataIn.substring(0, iA);
  f2 = dataIn.substring(iA + 1, iB);
  f3 = dataIn.substring(iB + 1, iC);
  f4 = dataIn.substring(iC + 1, iD);
  f5 = dataIn.substring(iD + 1, iE);

  if (f1 == "0" || f1 == "1") fire1 = f1.toInt();
  if (f2 == "0" || f2 == "1") fire2 = f2.toInt();
  if (f3 == "0" || f3 == "1") fire3 = f3.toInt();
  if (f4 == "0" || f4 == "1") fire4 = f4.toInt();
  if (f5 == "0" || f5 == "1") fire5 = f5.toInt();
}

// ── Parse: accelerometer intensity from earthquake Mega ───────────────────────
void parseEarthquakeData() {
  int8_t iM = dataIn.indexOf("M");
  int8_t iN = dataIn.indexOf("N");

  MotorV = dataIn.substring(0, iM);
  s2     = dataIn.substring(iM + 1, iN);

  if (s2 == "22") {
    float val = MotorV.toFloat();
    if (val <= 10.00) {
      MotorVal = val;
    }
  }
}
