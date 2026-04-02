/*
 * Earthquake and Fire Simulation System
 * File: esp8266_sms.ino
 * Board: ESP8266 (NodeMCU)
 *
 * Responsibilities:
 *  - Receives system state from Main Mega via SoftwareSerial
 *  - Sends SMS alerts via SIM900A GSM module when fire or earthquake is detected
 *  - Deduplicates SMS: only sends when the active zone pattern changes
 *  - Earthquake SMS only triggers once per event (requires MotorVal to drop
 *    below threshold and rise again for the next alert)
 */

#include <SoftwareSerial.h>

SoftwareSerial gsm(D5, D6);       // SIM900A: RX=D5, TX=D6
SoftwareSerial megaSerial(D7, D8); // Main Mega: RX=D7, TX=D8

// ── Phone number ──────────────────────────────────────────────────────────────
String currentPhone = "639672932231";  // default; overridden by app

// ── Serial protocol state ─────────────────────────────────────────────────────
char   c;
String dataIn = "";

// ── Mode ──────────────────────────────────────────────────────────────────────
// 0 = Home, 1 = Fire, 2 = Earthquake
int screen = 0;

// ── Fire state ────────────────────────────────────────────────────────────────
int fire1 = 0, fire2 = 0, fire3 = 0, fire4 = 0, fire5 = 0;

// SMS deduplication: only send when the active zone combination changes
int lastFirePattern = 0;
int currentFirePattern = 0;
int idleCount = 0;  // consecutive idle cycles before resetting dedup

// ── Earthquake state ──────────────────────────────────────────────────────────
float MotorVal = 0.0;
// SMS sent once per rising event (waits for value to drop then rise again)
int eqTriggered = 0;
int eqIdleCount = 0;

void setup() {
  Serial.begin(9600);
  megaSerial.begin(9600);
  gsm.begin(9600);
}

void loop() {
  // ── Read data from Main Mega ─────────────────────────────────────────────────
  while (megaSerial.available() > 0) {
    c = megaSerial.read();
    if (c == '\n') break;
    else dataIn += c;
  }

  if (c == '\n') {
    parseData();

    if (screen == 1) {
      handleFireAlerts();
    } else if (screen == 2) {
      handleEarthquakeAlert();
    } else {
      // Home / idle: reset dedup counters after a few cycles
      fire1 = fire2 = fire3 = fire4 = fire5 = 0;
      MotorVal = 0;
    }

    c = 0;
    dataIn = "";
  }
}

// ── Fire alert logic ──────────────────────────────────────────────────────────
void handleFireAlerts() {
  bool anyFire = (fire1 || fire2 || fire3 || fire4 || fire5);

  if (anyFire) {
    // Build a unique integer fingerprint of the current zone pattern
    currentFirePattern = fire1 + fire2*10 + fire3*100 + fire4*1000 + fire5*10000;

    if (currentFirePattern != lastFirePattern) {
      lastFirePattern = currentFirePattern;
      sendFireSMS();
    }
    idleCount = 0;
  } else {
    // Reset dedup after 5 consecutive idle cycles
    if (idleCount >= 5) {
      lastFirePattern = 0;
      currentFirePattern = 0;
      idleCount = 0;
    } else {
      idleCount++;
    }
  }
}

// ── Earthquake alert logic ────────────────────────────────────────────────────
void handleEarthquakeAlert() {
  if (MotorVal < 3.5) {
    // Below threshold: count idle cycles
    if (eqIdleCount >= 5) {
      eqTriggered = 1;  // ready to alert on next rise
    } else {
      eqIdleCount++;
    }
  } else {
    // Above threshold
    eqIdleCount = 0;
    if (eqTriggered == 1) {
      sendEarthquakeSMS();
      eqTriggered = 0;
    }
  }
}

// ── Build and send fire SMS ───────────────────────────────────────────────────
void sendFireSMS() {
  // Collect names of active zones into an array
  String zones[5];
  int count = 0;
  if (fire1) zones[count++] = "First Floor";
  if (fire2) zones[count++] = "Second Floor";
  if (fire3) zones[count++] = "Third Floor";
  if (fire4) zones[count++] = "Fourth Floor";
  if (fire5) zones[count++] = "House";

  // Build zone list string
  String zoneList = "";
  for (int i = 0; i < count; i++) {
    if (i == 0)             zoneList = zones[i];
    else if (i == count-1)  zoneList += " and " + zones[i];
    else                    zoneList += ", " + zones[i];
  }

  String msg = "Warning: Fire is detected on " + zoneList +
               ". Please stay calm and evacuate the premises immediately.";

  sendSMS(msg);
}

// ── Build and send earthquake SMS ─────────────────────────────────────────────
void sendEarthquakeSMS() {
  char buf[10];
  dtostrf(MotorVal, 6, 2, buf);
  String msg = "Warning! Intensity " + String(buf) + " earthquake has been detected.";
  sendSMS(msg);
}

// ── Low-level GSM SMS sender ──────────────────────────────────────────────────
void sendSMS(String message) {
  Serial.println("Sending SMS: " + message);
  gsm.println("AT+CMGF=1");                                 // text mode
  delay(1000);
  gsm.println("AT+CMGS=\"+" + currentPhone + "\"\r");       // recipient
  delay(1000);
  gsm.println(message);
  delay(100);
  gsm.println((char)26);  // CTRL+Z to send
  delay(2000);
}

// ── Parse incoming data from Main Mega ───────────────────────────────────────
void parseData() {
  int8_t iR = dataIn.indexOf("R");
  int8_t iS = dataIn.indexOf("S");

  String phoneStr  = dataIn.substring(0, iR);
  String screenStr = dataIn.substring(iR + 1, iS);

  if (phoneStr.length() == 12) currentPhone = phoneStr;
  if (screenStr == "0" || screenStr == "1" || screenStr == "2") {
    screen = screenStr.toInt();
  }

  if (screen == 1) {
    // Parse fire zone flags
    int8_t iU = dataIn.indexOf("U");
    int8_t iV = dataIn.indexOf("V");
    int8_t iW = dataIn.indexOf("W");
    int8_t iX = dataIn.indexOf("X");
    int8_t iY = dataIn.indexOf("Y");

    String f[5] = {
      dataIn.substring(iS + 1, iU),
      dataIn.substring(iU + 1, iV),
      dataIn.substring(iV + 1, iW),
      dataIn.substring(iW + 1, iX),
      dataIn.substring(iX + 1, iY)
    };

    fire1 = (f[0] == "1") ? 1 : 0;
    fire2 = (f[1] == "1") ? 1 : 0;
    fire3 = (f[2] == "1") ? 1 : 0;
    fire4 = (f[3] == "1") ? 1 : 0;
    fire5 = (f[4] == "1") ? 1 : 0;

    MotorVal = 0;

  } else if (screen == 2) {
    // Parse earthquake intensity
    int8_t iu = dataIn.indexOf("u");
    int8_t iv = dataIn.indexOf("v");

    String motorStr = dataIn.substring(iS + 1, iu);
    String filter   = dataIn.substring(iu + 1, iv);

    if (filter == "22") {
      float val = motorStr.toFloat();
      if (val <= 10.00) MotorVal = val;
    }

    fire1 = fire2 = fire3 = fire4 = fire5 = 0;

  } else {
    fire1 = fire2 = fire3 = fire4 = fire5 = 0;
    MotorVal = 0;
  }
}
