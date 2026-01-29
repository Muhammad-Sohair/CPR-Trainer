/*
 * Arduino CPR Feedback Training Module
 * ------------------------------------
 * A low-cost, real-time feedback system for CPR training.
 * Features:
 * - Real-time compression depth analysis via FSR.
 * - Metronome (110 BPM).
 * - Visual feedback (LEDs + LCD).
 * - Auto-sleep mode for power saving.
 *
 * Hardware:
 * - Arduino Uno
 * - FSR 402 Sensor (Pin A0) + 10k Resistor
 * - I2C LCD 16x2 (SDA=A4, SCL=A5)
 * - Green LED (Pin 9), Red LED (Pin 10), Mode LED (Pin 11)
 * - Active Buzzer (Pin 8)
 *
 * Library Required: LiquidCrystal I2C by Frank de Brabander
 */

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// --- PIN DEFINITIONS ---
const int PIN_FSR = A0;       // Force Sensitive Resistor
const int PIN_BUZZER = 8;     // Active Buzzer
const int PIN_LED_GOOD = 9;   // Green LED (Correct Depth)
const int PIN_LED_BAD = 10;   // Red LED (Incorrect Depth)
const int PIN_LED_MODE = 11;  // Blue/Yellow LED (Breath Mode)

// --- LCD CONFIGURATION ---
// Address is usually 0x27 or 0x3F.
LiquidCrystal_I2C lcd(0x27, 16, 2);  

// --- CPR GUIDELINE CONSTANTS (AHA) ---
const int TARGET_BPM = 110;
const long METRONOME_INTERVAL = 60000 / TARGET_BPM; 
const int COMPRESSIONS_PER_CYCLE = 30;
const int BREATH_PAUSE_DURATION = 5000; // 5 Seconds for 2 breaths

// --- POWER SAVING SETTINGS ---
const unsigned long TIMEOUT_MS = 120000; // 2 Minutes
unsigned long lastActivityTime = 0;      

// --- SENSOR CALIBRATION ---
// Adjust these values based on your specific FSR and surface
const int THRESHOLD_MIN_PRESSURE = 150; // Detects "touch"
const int THRESHOLD_GOOD_MIN = 400;     // Minimum "Good" depth
const int THRESHOLD_GOOD_MAX = 980;     // Maximum "Good" depth (limits "Too Hard")
const int THRESHOLD_RECOIL = 50;        // Ensures full release

// --- GLOBAL VARIABLES ---
unsigned long lastBeatTime = 0;
unsigned long stateStartTime = 0;
int compressionCount = 0;
int fsrValue = 0;
int maxPressureInStroke = 0;

// State Machine
enum State { 
  STATE_COMPRESSION_DOWN, 
  STATE_COMPRESSION_UP, 
  STATE_BREATH_PAUSE,
  STATE_SLEEP 
};
State currentState = STATE_COMPRESSION_UP;

void setup() {
  Serial.begin(9600);
  
  // Initialize Pins
  pinMode(PIN_LED_GOOD, OUTPUT);
  pinMode(PIN_LED_BAD, OUTPUT);
  pinMode(PIN_LED_MODE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  
  // Startup Sequence
  lcd.setCursor(0, 0);
  lcd.print("CPR TRAINER v1.2");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  
  resetSystem(); // Enters the main loop state
}

void loop() {
  unsigned long currentMillis = millis();
  fsrValue = analogRead(PIN_FSR);

  // --- ACTIVITY MONITOR ---
  // Reset sleep timer if user is interacting
  if (fsrValue > THRESHOLD_MIN_PRESSURE) {
    lastActivityTime = currentMillis;
  }

  // Check for Timeout
  if (currentState != STATE_SLEEP && (currentMillis - lastActivityTime > TIMEOUT_MS)) {
    goToSleep();
  }

  // --- STATE MACHINE ---
  switch (currentState) {
    
    // STATE 0: SLEEP MODE
    case STATE_SLEEP:
      // Wake up on pressure
      if (fsrValue > THRESHOLD_MIN_PRESSURE) {
        wakeUp();
      }
      break;

    // STATE 1: WAITING FOR COMPRESSION (UP)
    case STATE_COMPRESSION_UP:
      runMetronome(currentMillis);

      if (fsrValue > THRESHOLD_MIN_PRESSURE) {
        // Clear previous LEDs
        digitalWrite(PIN_LED_GOOD, LOW); 
        digitalWrite(PIN_LED_BAD, LOW);
        
        currentState = STATE_COMPRESSION_DOWN;
        maxPressureInStroke = fsrValue; 
      }
      break;

    // STATE 2: COMPRESSION IN PROGRESS (DOWN)
    case STATE_COMPRESSION_DOWN:
      runMetronome(currentMillis);

      // Track Peak Pressure
      if (fsrValue > maxPressureInStroke) maxPressureInStroke = fsrValue;

      // Detect Recoil (Release)
      if (fsrValue < THRESHOLD_RECOIL) {
        analyzeCompression(maxPressureInStroke);
        compressionCount++;
        updateScreenCount();

        // Check for Cycle Completion (30 pushes)
        if (compressionCount >= COMPRESSIONS_PER_CYCLE) {
          startBreathPause(currentMillis);
        } else {
          currentState = STATE_COMPRESSION_UP; 
        }
      }
      break;

    // STATE 3: RESCUE BREATH PAUSE
    case STATE_BREATH_PAUSE:
      digitalWrite(PIN_LED_MODE, HIGH);
      
      // Countdown Display
      int timeLeft = (BREATH_PAUSE_DURATION - (currentMillis - stateStartTime)) / 1000;
      lcd.setCursor(10, 1);
      lcd.print(timeLeft); lcd.print("s ");

      if (currentMillis - stateStartTime >= BREATH_PAUSE_DURATION) {
        compressionCount = 0;
        digitalWrite(PIN_LED_MODE, LOW);
        
        // Resume Message
        lcd.clear();
        lcd.print("RESUME CPR!");
        delay(500); 
        updateScreenCount();
        
        // Audio Cue
        tone(PIN_BUZZER, 1000, 500); 
        
        currentState = STATE_COMPRESSION_UP;
        lastBeatTime = currentMillis; 
      }
      break;
  }
}

// --- HELPER FUNCTIONS ---

void runMetronome(unsigned long currentMillis) {
  if (currentState != STATE_BREATH_PAUSE) {
    if (currentMillis - lastBeatTime >= METRONOME_INTERVAL) {
      lastBeatTime = currentMillis;
      tone(PIN_BUZZER, 2000, 50); // High pitch beep
    }
  }
}

void goToSleep() {
  currentState = STATE_SLEEP;
  lcd.noBacklight();
  lcd.clear();
  lcd.print("Sleeping...");
  delay(1000);
  lcd.noDisplay();
  
  digitalWrite(PIN_LED_GOOD, LOW);
  digitalWrite(PIN_LED_BAD, LOW);
  digitalWrite(PIN_LED_MODE, LOW);
  noTone(PIN_BUZZER);
}

void wakeUp() {
  lcd.display();
  lcd.backlight();
  lcd.clear();
  lcd.print("Waking up...");
  delay(1000);
  resetSystem();
}

void resetSystem() {
  currentState = STATE_COMPRESSION_UP;
  lastActivityTime = millis();
  lastBeatTime = millis();
  compressionCount = 0;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("START CPR");
  updateScreenCount();
}

void analyzeCompression(int peakValue) {
  lcd.setCursor(0, 0); 
  
  if (peakValue >= THRESHOLD_GOOD_MIN && peakValue <= THRESHOLD_GOOD_MAX) {
    digitalWrite(PIN_LED_GOOD, HIGH);
    lcd.print("Depth: GOOD!    "); 
  } else if (peakValue < THRESHOLD_GOOD_MIN) {
    digitalWrite(PIN_LED_BAD, HIGH);
    lcd.print("PUSH HARDER!    ");
  } else {
    digitalWrite(PIN_LED_BAD, HIGH);
    lcd.print("TOO HARD!       ");
  }
}

void updateScreenCount() {
  lcd.setCursor(0, 1);
  lcd.print("Count: ");
  lcd.print(compressionCount);
  lcd.print("/30  ");
}

void startBreathPause(unsigned long startTime) {
  currentState = STATE_BREATH_PAUSE;
  stateStartTime = startTime;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("STOP! BREATHS");
  lcd.setCursor(0, 1);
  lcd.print("Resume in: 5s");
  
  // Audio Cue for Breaths
  tone(PIN_BUZZER, 1500, 100); delay(150); tone(PIN_BUZZER, 1500, 100);
}