/*
 * Earthquake and Fire Simulation System
 * File: mega_earthquake.ino
 * Board: Arduino Mega (Earthquake Simulator)
 *
 * Responsibilities:
 *  - Reads ADXL335 accelerometer (X, Y, Z axes) to measure vibration
 *  - Calculates earthquake intensity (Richter-like scale 3–9) from acceleration magnitude
 *  - Drives a DC motor at calibrated speeds to physically simulate shaking
 *  - Sends intensity readings back to the Main Mega (Serial2)
 *  - Receives intensity commands from the Main Mega to set motor speed
 */

// ── Motor A control pins ──────────────────────────────────────────────────────
const int EN_A = 9;  // PWM speed control
const int IN_1 = 8;  // direction pin 1
const int IN_2 = 7;  // direction pin 2

// ── Accelerometer analog pins ─────────────────────────────────────────────────
#define ACCEL_X A15
#define ACCEL_Y A14
#define ACCEL_Z A13
#define VIV_PIN A12  // vibration trigger signal

// ── Calibration settings ──────────────────────────────────────────────────────
#define SAMPLES   50   // samples averaged during calibration
#define MAX_DELTA  20  // max change threshold to detect motion
#define MIN_DELTA -20  // min change threshold

// ── Accelerometer baseline (set during setup calibration) ─────────────────────
int xSample = 0;
int ySample = 0;
int zSample = 0;

// ── Earthquake state ──────────────────────────────────────────────────────────
float  MotorVal   = 0.0;  // calculated Richter intensity
int    Intensity  = 0;    // commanded intensity from Main Mega
int    reIntensity = 0;   // previous intensity (detects changes)
int    rerept     = 0;    // flag: trigger startup burst on new intensity

// ── Serial protocol state ─────────────────────────────────────────────────────
char   c;
String dataIn    = "";
String IntensityStr = "";
String s4        = "";

void setup() {
  // Motor pins
  pinMode(EN_A, OUTPUT);
  pinMode(IN_1, OUTPUT);
  pinMode(IN_2, OUTPUT);

  // Motor off at startup
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, LOW);

  Serial.begin(9600);   // USB monitor
  Serial2.begin(9600);  // Main Mega

  // ── Accelerometer calibration: average SAMPLES readings as baseline ──────────
  for (int i = 0; i < SAMPLES; i++) {
    xSample += analogRead(ACCEL_X);
    ySample += analogRead(ACCEL_Y);
    zSample += analogRead(ACCEL_Z);
  }
  xSample /= SAMPLES;
  ySample /= SAMPLES;
  zSample /= SAMPLES;

  delay(3000);
}

void loop() {
  int viv = analogRead(VIV_PIN);

  // ── Read accelerometer and calculate intensity ──────────────────────────────
  int xDelta = xSample - analogRead(ACCEL_X);
  int yDelta = ySample - analogRead(ACCEL_Y);
  int zDelta = zSample - analogRead(ACCEL_Z);

  delay(100);

  // Only process if motion exceeds threshold
  bool motionDetected = (
    xDelta < MIN_DELTA || xDelta > MAX_DELTA ||
    yDelta < MIN_DELTA || yDelta > MAX_DELTA ||
    zDelta < MIN_DELTA || zDelta > MAX_DELTA
  );

  if (motionDetected) {
    float distance = sqrt((float)(xDelta * xDelta) + (float)(yDelta * yDelta));

    // Map acceleration magnitude to Richter scale range 3–9
    // Each band covers one unit of intensity
    if      (distance >= 28  && distance <= 59)  MotorVal = 3.0 + ((distance - 28)  / 16.0);
    else if (distance >= 60  && distance <= 113) MotorVal = 4.0 + ((distance - 60)  / 27.0);
    else if (distance >= 114 && distance <= 182) MotorVal = 5.0 + ((distance - 114) / 34.0);
    else if (distance >= 183 && distance <= 264) MotorVal = 6.0 + ((distance - 183) / 41.0);
    else if (distance >= 265 && distance <= 364) MotorVal = 7.0 + ((distance - 265) / 50.0);
    else if (distance >= 365 && distance <= 479) MotorVal = 8.0 + ((distance - 365) / 57.0);
    else if (distance >= 480 && distance <= 615) MotorVal = 9.0 + ((distance - 480) / 68.0);
  }

  // Send intensity reading to Main Mega
  Serial2.print(MotorVal); Serial2.print("M");
  Serial2.print(22);       Serial2.print("N");
  Serial2.print("\n");

  delay(250);
  c = 0;
  dataIn = "";

  // ── Only control motor when vibration signal is active ─────────────────────
  if (viv >= 100) {
    // Read intensity command from Main Mega
    while (Serial2.available() > 0) {
      c = Serial2.read();
      if (c == '\n') break;
      else dataIn += c;
    }
    if (c == '\n') {
      parseCommand();
      c = 0;
      dataIn = "";
    }

    // On new intensity value: give a 3-second startup burst
    if (Intensity != reIntensity && Intensity != 0) {
      reIntensity = Intensity;
      rerept = 1;
    }
    if (Intensity > 0 && rerept == 1) {
      rerept = 0;
      analogWrite(EN_A, 140);
      digitalWrite(IN_1, HIGH);
      digitalWrite(IN_2, LOW);
      delay(3000);
    }

    // Set continuous motor speed based on commanded intensity
    switch (Intensity) {
      case 4: analogWrite(EN_A, 36);  break;
      case 5: analogWrite(EN_A, 47);  break;
      case 6: analogWrite(EN_A, 92);  break;
      case 7: analogWrite(EN_A, 142); break;
      case 8: analogWrite(EN_A, 225); break;
      case 0:
        digitalWrite(IN_1, LOW);
        digitalWrite(IN_2, LOW);
        reIntensity = 0;
        return;
    }

    // Keep motor spinning forward
    if (Intensity > 0) {
      digitalWrite(IN_1, HIGH);
      digitalWrite(IN_2, LOW);
    }
  }
}

// ── Parse: intensity command from Main Mega ───────────────────────────────────
void parseCommand() {
  int8_t iI = dataIn.indexOf("I");
  int8_t iJ = dataIn.indexOf("J");

  IntensityStr = dataIn.substring(0, iI);
  s4           = dataIn.substring(iI + 1, iJ);

  if (IntensityStr == "0" || IntensityStr == "4" || IntensityStr == "5" ||
      IntensityStr == "6" || IntensityStr == "7" || IntensityStr == "8") {
    Intensity = IntensityStr.toInt();
  }
}
