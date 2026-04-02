/*
 * Earthquake and Fire Simulation System
 * File: esp32_iot.ino
 * Board: ESP32
 *
 * Responsibilities:
 *  - Connects to Wi-Fi and maintains connection with auto-reconnect
 *  - Reads control commands (screen, phone number, intensity, motor trigger)
 *    from Firebase Realtime Database
 *  - Forwards commands to the Main Mega via SoftwareSerial
 *  - Receives live sensor data from the Main Mega
 *  - Uploads sensor data to Firebase and ThingSpeak every 3 seconds
 *
 * ⚠️  IMPORTANT: Replace the placeholder values below with your actual credentials
 *     before uploading. Do NOT commit real credentials to a public GitHub repo.
 *     Consider using a config.h file added to .gitignore instead.
 */

#include <WiFi.h>
#include <FirebaseESP32.h>
#include "ThingSpeak.h"
#include <SoftwareSerial.h>

// ── Credentials — replace with your own ──────────────────────────────────────
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define FIREBASE_HOST   "https://YOUR_PROJECT_ID-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH   "YOUR_FIREBASE_SECRET_KEY"

// ThingSpeak
const unsigned long THINGSPEAK_CHANNEL = 0;          // replace with your channel number
const char*         THINGSPEAK_API_KEY = "YOUR_API_KEY";

// ── Serial to Main Mega ───────────────────────────────────────────────────────
SoftwareSerial megaSerial(16, 17);  // RX=16, TX=17

// ── Firebase & ThingSpeak objects ─────────────────────────────────────────────
FirebaseData  firebaseData;
FirebaseJson  json;
WiFiClient    wifiClient;

// ── Firebase data paths ───────────────────────────────────────────────────────
String fbPaths[4] = {
  "/Data/scrn",      // screen mode (0/1/2)
  "/Data/PN",        // phone number
  "/Data/motor",     // motor trigger (0/1)
  "/Data/intesity"   // intensity (4-8)
};

// ── App-controlled values ─────────────────────────────────────────────────────
int    Screen    = 0;
String PhoneNumber = "639672932231";
int    motor     = 0;
int    Intensity = 1;

// ── Serial protocol state ─────────────────────────────────────────────────────
char   c;
String dataIn = "";

// ── Sensor values to upload ───────────────────────────────────────────────────
int   fire1 = 0, fire2 = 0, fire3 = 0, fire4 = 0, fire5 = 0;
float MotorVal = 0.0;

// ── ThingSpeak upload interval ────────────────────────────────────────────────
unsigned long prevMillis = 0;
const unsigned long UPLOAD_INTERVAL = 3000;  // 3 seconds

// ── Parsed field strings ──────────────────────────────────────────────────────
String f1, f2, f3, f4, f5;
String MotorV, s2;

void setup() {
  Serial.begin(9600);
  megaSerial.begin(9600);

  connectToWiFi();
  ThingSpeak.begin(wifiClient);
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
}

void loop() {
  // ── Auto-reconnect Wi-Fi ─────────────────────────────────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost. Reconnecting...");
    connectToWiFi();
  }

  // ── Read control values from Firebase ────────────────────────────────────────
  for (int i = 0; i < 4; i++) {
    Firebase.getString(firebaseData, fbPaths[i]);
    String raw = firebaseData.stringData();

    // Some values are wrapped in escaped JSON; extract inner value
    int slashIdx = raw.indexOf("\\", 2);
    String clean = (slashIdx > 0) ? raw.substring(2, slashIdx) : raw;

    if      (fbPaths[i] == fbPaths[0]) Screen    = raw.toInt();
    else if (fbPaths[i] == fbPaths[1]) PhoneNumber = clean;
    else if (fbPaths[i] == fbPaths[2]) motor     = raw.toInt();
    else if (fbPaths[i] == fbPaths[3]) Intensity = clean.toInt();
  }

  // If motor trigger is off, force intensity to 0
  if (motor == 0) Intensity = 0;

  // ── Forward commands to Main Mega ─────────────────────────────────────────────
  if (Screen == 0 || Screen == 1) {
    megaSerial.print(PhoneNumber);
    megaSerial.print("r"); megaSerial.print(Screen);
    megaSerial.print("s"); megaSerial.print(101);  // 101 = no intensity override
    megaSerial.print("t"); megaSerial.print("\n");
  } else if (Screen == 2) {
    megaSerial.print(PhoneNumber);
    megaSerial.print("r"); megaSerial.print(Screen);
    megaSerial.print("s"); megaSerial.print(Intensity);
    megaSerial.print("t"); megaSerial.print("\n");
  }
  delay(250);

  // ── Receive sensor data back from Main Mega ───────────────────────────────────
  if (Screen == 1) {
    readMegaSerial();
    if (c == '\n') {
      parseFireData();
      c = 0; dataIn = "";
    }
  } else if (Screen == 2) {
    readMegaSerial();
    if (c == '\n') {
      parseEarthquakeData();
      c = 0; dataIn = "";
    }
  }

  // ── Upload to Firebase and ThingSpeak every UPLOAD_INTERVAL ms ───────────────
  unsigned long now = millis();
  if (now - prevMillis >= UPLOAD_INTERVAL) {
    prevMillis = now;

    // Firebase
    json.set("/F1", fire1);
    json.set("/F2", fire2);
    json.set("/F3", fire3);
    json.set("/F4", fire4);
    json.set("/F5", fire5);
    json.set("/EQS", MotorVal);
    Firebase.updateNode(firebaseData, "/Data", json);

    // ThingSpeak
    ThingSpeak.setField(1, fire1);
    ThingSpeak.setField(2, fire2);
    ThingSpeak.setField(3, fire3);
    ThingSpeak.setField(4, fire4);
    ThingSpeak.setField(5, fire5);
    ThingSpeak.setField(6, MotorVal);
    ThingSpeak.setField(7, PhoneNumber);

    int result = ThingSpeak.writeFields(THINGSPEAK_CHANNEL, THINGSPEAK_API_KEY);
    if (result == 200) Serial.println("ThingSpeak update OK.");
    else Serial.println("ThingSpeak error: " + String(result));
  }
}

// ── Read one line from megaSerial into dataIn ─────────────────────────────────
void readMegaSerial() {
  while (megaSerial.available() > 0) {
    c = megaSerial.read();
    if (c == '\n') break;
    else dataIn += c;
  }
}

// ── Parse fire zone data from Main Mega ───────────────────────────────────────
void parseFireData() {
  int8_t ia = dataIn.indexOf("a");
  int8_t ib = dataIn.indexOf("b");
  int8_t ic = dataIn.indexOf("c");
  int8_t id = dataIn.indexOf("d");
  int8_t ie = dataIn.indexOf("e");

  f1 = dataIn.substring(0, ia);
  f2 = dataIn.substring(ia+1, ib);
  f3 = dataIn.substring(ib+1, ic);
  f4 = dataIn.substring(ic+1, id);
  f5 = dataIn.substring(id+1, ie);

  if (f1 == "0" || f1 == "1") fire1 = f1.toInt();
  if (f2 == "0" || f2 == "1") fire2 = f2.toInt();
  if (f3 == "0" || f3 == "1") fire3 = f3.toInt();
  if (f4 == "0" || f4 == "1") fire4 = f4.toInt();
  if (f5 == "0" || f5 == "1") fire5 = f5.toInt();
}

// ── Parse earthquake intensity from Main Mega ─────────────────────────────────
void parseEarthquakeData() {
  int8_t im = dataIn.indexOf("m");
  int8_t in_ = dataIn.indexOf("n");

  MotorV = dataIn.substring(0, im);
  s2     = dataIn.substring(im+1, in_);

  if (s2 == "22") {
    float val = MotorV.toFloat();
    if (val <= 10.00) MotorVal = val;
  }
}

// ── Wi-Fi connection with retry ───────────────────────────────────────────────
void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nFailed to connect. Check credentials or restart.");
  }
}
