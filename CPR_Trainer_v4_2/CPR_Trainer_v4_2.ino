#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- PIN DEFINITIONS ---
const int PIN_TRIG = 4;
const int PIN_ECHO = 5;
const int PIN_BUZZER = 8;
const int PIN_LED_GOOD = 9;
const int PIN_LED_BAD = 10;

// Change 0x27 to 0x3F if your LCD doesn't show text
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- DISTANCE SETTINGS (Centimeters) ---
const int DISTANCE_RESTING = 15;  // Distance when NOBODY is touching it
const int DISTANCE_MIN_PUSH = 13; // Distance to trigger a compression
const int DISTANCE_GOOD = 10;     // The perfect 5cm compression depth
const int DISTANCE_TOO_DEEP = 8;  // Pushed too far down

// --- METRONOME SETTINGS ---
const int TARGET_BPM = 110;
const long METRONOME_INTERVAL = 60000 / TARGET_BPM;
unsigned long lastBeatTime = 0;

// --- VARIABLES ---
int compressionCount = 0;
int currentDistance = 0;
int maxDepthInStroke = 999; // Lower number means deeper push
String currentStatus = "WAITING";
bool isCompressing = false;
unsigned long lastReleaseTime = 0;

// --- BREATH PAUSE VARIABLES ---
bool isBreathingPause = false;
unsigned long breathPauseStartTime = 0;
const long BREATH_DURATION = 5000; // 5 seconds for 2 rescue breaths

void setup() {
  Serial.begin(9600);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  pinMode(PIN_LED_GOOD, OUTPUT);
  pinMode(PIN_LED_BAD, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.print("CPR Trainer v4.2");
  lcd.setCursor(0, 1);
  lcd.print("Sonic Mode Ready");
  delay(1500);
  lcd.clear();
}

int getDistance() {
  // Fire the ultrasonic pulse
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // Measure the echo time
  long duration = pulseIn(PIN_ECHO, HIGH, 30000); // 30ms timeout
  if (duration == 0) return DISTANCE_RESTING;
  return duration * 0.034 / 2; // Convert microseconds to cm
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. GET READING IN CM
  currentDistance = getDistance();

  // 2. CREATE THE FAKE FSR WAVE FOR THE PYTHON SERVER
  // Turns a 15cm rest into 0, and a 10cm push into 500
  int fakeFsrValue = map(currentDistance, DISTANCE_RESTING, DISTANCE_GOOD, 0, 500);
  if (fakeFsrValue < 0) fakeFsrValue = 0;

  // 3. BREATHING PAUSE LOGIC
  if (isBreathingPause) {
    long timeElapsed = currentMillis - breathPauseStartTime;
    if (timeElapsed >= BREATH_DURATION) {
      isBreathingPause = false;
      compressionCount = 0;
      currentStatus = "WAITING";
      lcd.clear();
      lastBeatTime = currentMillis;
    } else {
      int timeLeft = (BREATH_DURATION - timeElapsed) / 1000;
      lcd.setCursor(0, 0);
      lcd.print("GIVE 2 BREATHS! ");
      lcd.setCursor(0, 1);
      lcd.print("Resume in: ");
      lcd.print(timeLeft);
      lcd.print("s   ");

      // Send flatline data to server during breathing
      Serial.print("{\"fsr\":");
      Serial.print(fakeFsrValue);
      Serial.println(",\"status\":\"BREATHING\",\"count\":30}");
      delay(30);
      return;
    }
  }

  // 4. METRONOME
  if (currentMillis - lastBeatTime >= METRONOME_INTERVAL) {
    lastBeatTime = currentMillis;
    tone(PIN_BUZZER, 2000, 50); // Short, sharp beep
  }

  // 5. COMPRESSION LOGIC (Smaller distance = harder push!)
  if (currentDistance < DISTANCE_MIN_PUSH) {
    if (!isCompressing) {
      isCompressing = true;
      maxDepthInStroke = currentDistance;
    } else {
      if (currentDistance < maxDepthInStroke) maxDepthInStroke = currentDistance;
    }
  } else if (isCompressing && currentDistance > (DISTANCE_MIN_PUSH - 1)) {
    isCompressing = false;
    compressionCount++;
    lastReleaseTime = currentMillis;

    // Evaluate depth
    if (maxDepthInStroke <= DISTANCE_GOOD && maxDepthInStroke > DISTANCE_TOO_DEEP) {
      currentStatus = "GOOD";
      digitalWrite(PIN_LED_GOOD, HIGH);
      digitalWrite(PIN_LED_BAD, LOW);
    } else if (maxDepthInStroke > DISTANCE_GOOD) {
      currentStatus = "TOO_WEAK";
      digitalWrite(PIN_LED_GOOD, LOW);
      digitalWrite(PIN_LED_BAD, HIGH);
    } else {
      currentStatus = "TOO_HARD";
      digitalWrite(PIN_LED_GOOD, LOW);
      digitalWrite(PIN_LED_BAD, HIGH);
    }

    // Trigger Breath Pause if 30 compressions are reached
    if (compressionCount >= 30) {
      isBreathingPause = true;
      breathPauseStartTime = millis();
      currentStatus = "BREATHING";
      digitalWrite(PIN_LED_GOOD, LOW);
      digitalWrite(PIN_LED_BAD, LOW);

      // Give a distinct double-beep for breath transition
      tone(PIN_BUZZER, 1000, 200);
      delay(250);
      tone(PIN_BUZZER, 1000, 200);
    }

    maxDepthInStroke = 999; // Reset for next stroke
  } else if (!isCompressing) {
      digitalWrite(PIN_LED_GOOD, LOW);
      digitalWrite(PIN_LED_BAD, LOW);
      if (currentMillis - lastReleaseTime > 1500) {
          currentStatus = "WAITING";
      }
  }

  // 6. LCD UPDATE
  lcd.setCursor(0, 0);
  if (currentStatus == "WAITING") lcd.print("START CPR...    ");
  else if (currentStatus == "GOOD") lcd.print("Depth: GOOD!    ");
  else if (currentStatus == "TOO_WEAK") lcd.print("PUSH HARDER!    ");
  else if (currentStatus == "TOO_HARD") lcd.print("TOO HARD!       ");

  lcd.setCursor(0, 1);
  lcd.print("C:");
  lcd.print(compressionCount);
  lcd.print(" Dist:");
  lcd.print(currentDistance);
  lcd.print("cm  ");

  // 7. SEND TO PYTHON SERVER OVER SERIAL
  Serial.print("{\"fsr\":");
  Serial.print(fakeFsrValue); // Sends the FAKE FSR wave instead of raw cm
  Serial.print(",\"status\":\"");
  Serial.print(currentStatus);
  Serial.print("\",\"count\":");
  Serial.print(compressionCount);
  Serial.println("}");

  delay(30); // Small delay to keep the serial stream stable
}
