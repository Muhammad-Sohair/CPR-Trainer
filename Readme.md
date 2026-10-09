# 🫀 Arduino CPR Feedback Training Module

A low-cost, real-time feedback system for CPR practice. It gives feedback on compression depth, cues the compression rate with a metronome, and runs the 30:2 compression-to-breath cycle. The AHA and ERC 2025 targets (5–6 cm depth, 100–120 compressions/min) are used as reference values. It is a student-built training aid: it has not been calibrated against a reference instrument or certified, and it makes no clinical claims.

## 📄 Paper
**M. S. Khan, "Arduino-Based CPR Trainer with Live Telemetry," *Preprints*, Oct. 2026.**
DOI: [10.20944/preprints202610.0760.v1](https://doi.org/10.20944/preprints202610.0760.v1) · [Read on Preprints.org](https://www.preprints.org/manuscript/202610.0760/v1)

The preprint describes v4.2 (the ultrasonic version in [`CPR_Trainer_v4_2/`](CPR_Trainer_v4_2/CPR_Trainer_v4_2.ino)) and analyzes its firmware. It is not peer-reviewed.

```bibtex
@misc{khan2026cprtrainer,
  author = {Khan, Muhammad Sohair},
  title  = {Arduino-Based {CPR} Trainer with Live Telemetry},
  year   = {2026},
  month  = oct,
  publisher = {Preprints},
  doi    = {10.20944/preprints202610.0760.v1},
  url    = {https://www.preprints.org/manuscript/202610.0760/v1}
}
```

![Project Status](https://img.shields.io/badge/Status-Completed-success)
![Tech](https://img.shields.io/badge/Tech-Arduino%20%7C%20C%2B%2B%20%7C%20Sensors-blue)

## 📦 Versions in this repository
| Version | Folder / file | Sensor | Status |
|---|---|---|---|
| **v4.2 "Sonic Mode"** | [`CPR_Trainer_v4_2/`](CPR_Trainer_v4_2/CPR_Trainer_v4_2.ino) | HC-SR04 ultrasonic | Presented at ICETBEST 2026 (Salim Habib University, Karachi, 19–20 May 2026) |
| v1 (first prototype, Jan 2026) | [`code.ino`](code.ino) | Interlink FSR 402 | Superseded; described in the sections below |

### v4.2 at a glance
- **Rig.** An HC-SR04 sits at the bottom of a hollow shaft in a 10-inch Jumbolon (closed-cell polyethylene) foam block. It measures the distance to a rigid plastic reflector plate inside the block. The rest distance is 15 cm.
- **Classification of the deepest point of each compression.**
  | Reported distance | Approx. depth | LCD message |
  |---|---|---|
  | 9–10 cm | 5–6 cm | GOOD |
  | 11–12 cm | 3–4 cm | PUSH HARDER |
  | ≤ 8 cm | more than 6 cm | TOO HARD |
  
  Distances are whole centimetres. A stroke starts below 13 cm. The depth column is nominal: because readings are truncated to whole centimetres and the rest distance is fixed at 15 cm, the real "GOOD" band can sit up to about 1 cm shallower or deeper than 5–6 cm. The accompanying preprint gives the full analysis.
- **Cycle.** 110 BPM metronome. After 30 compressions, a 5 s breath pause.
- **Pins.** TRIG D4, ECHO D5, buzzer D8, green LED D9, red LED D10. The LCD is on I²C at 0x27.
- **Telemetry.** One JSON line per loop over USB at 9600 baud: `{"fsr":F,"status":S,"count":N}`. Despite its name, `fsr` is the distance re-encoded for the original FSR dashboard: `F = 100 × (15 − distance_cm)`.
- **Web dashboard.** A Flask + Flask-SocketIO server with a Chart.js live graph, made public through ngrok. Not yet in this repository.

## 🎥 Project Demo (v1, FSR prototype)
https://github.com/user-attachments/assets/2b567ae3-7cb6-4be4-9a8d-5c7575663425

## 💡 The Problem
High-fidelity CPR manikins that provide feedback on compression depth and rate cost thousands of dollars, making quality training inaccessible in many regions.

## 🛠️ The Solution
I developed a portable, non-invasive module using an **Arduino Uno** and **Force Sensitive Resistors (FSR)** to measure compression quality. The system provides:
- **Visual Feedback:** 16x2 LCD & LEDs (Green = Good Depth, Red = Poor Depth).
- **Auditory Guidance:** Active buzzer metronome at **110 BPM**.
- **Cycle Management:** Automates the 30:2 compression-to-breath cycle.
- **Power Saving:** Auto-sleep mode after 2 minutes of inactivity.

## 🔌 Hardware Tech Stack
- **Microcontroller:** Arduino Uno (ATmega328P)
- **Sensors:** Interlink FSR 402. It measures force, not depth; its thresholds were set by hand and were not calibrated against measured depth.
- **Display:** 16x2 LCD with I2C Interface (PCF8574)
- **Feedback:** 5mm LEDs (Green, Red) & Active Buzzer. The code also drives a "breath mode" LED on pin 11, but it was not fitted.

## 💻 Code Highlights
The firmware is written in **C++** and utilizes a **Non-Blocking State Machine** architecture.
- Uses `millis()` timers for the metronome, so the sensor keeps being read while it beeps. A few short `delay()` calls remain: start-up, the sleep, wake and resume messages, and the breath cue.
- Uses separate start (150) and release (50) thresholds, a form of hysteresis, so one compression is not counted twice. There is no other noise filtering.


## 🚀 How to Run
1. Install the `LiquidCrystal_I2C` library in Arduino IDE.
2. Connect the FSR sensor to Pin A0 (with 10kΩ pull-down resistor).
3. Connect LCD via I2C (SDA -> A4, SCL -> A5).
4. Upload `code.ino` (FSR prototype). For the v4.2 ultrasonic version, open `CPR_Trainer_v4_2/CPR_Trainer_v4_2.ino` instead.

---
Made by: Muhammad Sohair Khan
Led by: Engr. M. Yasir Zaheen

**University:** Sir Syed University of Engineering & Technology (SSUET).

