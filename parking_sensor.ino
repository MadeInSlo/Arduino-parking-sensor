#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= DISPLAY =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================= PINS =================
const int buttonPin = 4;
const int speakerPin = 5;
int trigPins[3] = {12, 10, 8};
int echoPins[3] = {13, 11, 9};
const int frontTrig = 6;
const int frontEcho = 7;

// ================= SYSTEM CONTROL =================
bool systemEnabled = true;
bool lastButtonState = HIGH;

// ================= SPEAKER PARAMETERS =================
const int beepFreq = 320;
const int minBeepInterval = 50;
const int maxBeepInterval = 1200;
const int beepDuration = 30;
unsigned long lastBeepTime = 0;

// ================= SENSOR STRUCT =================
struct Sensor {
  int trigPin;
  int echoPin;
  long readings[5];
  int readIndex;
  long total;
  long smoothed;
  bool failed;
};
Sensor sensors[3]; // rear sensors
Sensor frontSensor; // front sensor

// ================= DISTANCE READ =================
float getDistanceFloat(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return -1.0f;
  return duration * 0.034f / 2.0f;
}

// ================= SETUP =================
void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  pinMode(speakerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // Initialize rear sensors
  for (int i = 0; i < 3; i++) {
    sensors[i].trigPin = trigPins[i];
    sensors[i].echoPin = echoPins[i];
    sensors[i].readIndex = 0;
    sensors[i].total = 0;
    sensors[i].smoothed = 99;
    sensors[i].failed = false;
    for (int j = 0; j < 5; j++) {
      sensors[i].readings[j] = 99;
      sensors[i].total += 99;
    }
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
  }

  // Initialize front sensor
  frontSensor.trigPin = frontTrig;
  frontSensor.echoPin = frontEcho;
  frontSensor.readIndex = 0;
  frontSensor.total = 0;
  frontSensor.smoothed = 99;
  frontSensor.failed = false;
  for (int j = 0; j < 5; j++) {
    frontSensor.readings[j] = 99;
    frontSensor.total += 99;
  }
  pinMode(frontTrig, OUTPUT);
  pinMode(frontEcho, INPUT);
}

// ================= LOOP =================
void loop() {
  // ---- BUTTON TOGGLE SYSTEM ----
  bool buttonState = digitalRead(buttonPin);
  if (buttonState == LOW && lastButtonState == HIGH) {
    systemEnabled = !systemEnabled;
  }
  lastButtonState = buttonState;

  if (!systemEnabled) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("OFF");
    display.display();
    noTone(speakerPin);
    delay(50);
    return;
  }

  float closestRawFloat = 1000.0f;
  bool anyStop = false;

  // ---- READ & SMOOTH REAR SENSORS ----
  for (int i = 0; i < 3; i++) {
    float rawF = getDistanceFloat(sensors[i].trigPin, sensors[i].echoPin);
    long raw;
    if (rawF < 0 || rawF > 99.0f) {
      sensors[i].failed = true;
      raw = 99;
    } else {
      sensors[i].failed = false;
      raw = (long)rawF;
      if (rawF < closestRawFloat) closestRawFloat = rawF;
    }
    if (raw <= 20) anyStop = true;

    sensors[i].total -= sensors[i].readings[sensors[i].readIndex];
    sensors[i].readings[sensors[i].readIndex] = raw;
    sensors[i].total += raw;
    sensors[i].readIndex = (sensors[i].readIndex + 1) % 5;
    sensors[i].smoothed = sensors[i].total / 5;
  }

  // ---- READ & SMOOTH FRONT SENSOR ----
  float rawF = getDistanceFloat(frontSensor.trigPin, frontSensor.echoPin);
  long raw;
  if (rawF < 0 || rawF > 99.0f) {
    frontSensor.failed = true;
    raw = 99;
  } else {
    frontSensor.failed = false;
    raw = (long)rawF;
    if (rawF < closestRawFloat) closestRawFloat = rawF;
  }
  if (raw <= 20) anyStop = true;

  frontSensor.total -= frontSensor.readings[frontSensor.readIndex];
  frontSensor.readings[frontSensor.readIndex] = raw;
  frontSensor.total += raw;
  frontSensor.readIndex = (frontSensor.readIndex + 1) % 5;
  frontSensor.smoothed = frontSensor.total / 5;

  // ---- DECIDE WHICH TO DISPLAY ----
  bool showFront = (frontSensor.smoothed < sensors[0].smoothed &&
                    frontSensor.smoothed < sensors[1].smoothed &&
                    frontSensor.smoothed < sensors[2].smoothed);

  display.clearDisplay();

  if (anyStop) {
    display.setTextSize(5);
    display.setCursor(0, 10);
    display.print("STOP");
    display.display();
  } else {
    if (showFront) {
      // FRONT SENSOR DISPLAY
      display.setTextSize(1);
      display.setCursor(44, 5); display.print("SPREDAJ");
      display.setTextSize(3);
      display.setCursor(48, 25);
      if (frontSensor.smoothed >= 99) display.print("OK");
      else display.print(frontSensor.smoothed);
      if (frontSensor.smoothed < 99) {
        display.setTextSize(1);
        display.setCursor(57, 55);
        display.print("cm");
      }
    } else {
      // REAR SENSOR DISPLAY
      int numberY = 25;
      int cmY = 55;
      int labelY = 5;
      int leftX = 0;
      int centerX = SCREEN_WIDTH / 2 - 18;
      int rightX = SCREEN_WIDTH - 36;

      display.setTextSize(1);
      display.setCursor(leftX, labelY);   display.print("LEVA");
      display.setCursor(centerX, labelY);  display.print("SREDINA");
      display.setCursor(rightX, labelY);   display.print("DESNA");

      for (int i = 0; i < 3; i++) {
        int x = (i == 0) ? leftX : (i == 1 ? centerX : rightX);
        display.setTextSize(3);
        display.setCursor(x, numberY);
        if (sensors[i].smoothed >= 99) display.print("OK");
        else display.print(sensors[i].smoothed);

        if (sensors[i].smoothed < 99) {
          display.setTextSize(1);
          display.setCursor(x + 5, cmY);
          display.print("cm");
        }
      }
    }
    display.display();
  }

  // ---- SPEAKER (STOP HAS ABSOLUTE PRIORITY) ----
  if (anyStop) {
    tone(speakerPin, beepFreq);
  } else if (closestRawFloat <= 99.0f) {
    float t = (closestRawFloat - 20.0f) / 79.0f;
    int interval = minBeepInterval + t * (maxBeepInterval - minBeepInterval);
    if (millis() - lastBeepTime >= interval) {
      lastBeepTime = millis();
      tone(speakerPin, beepFreq);
      delay(beepDuration);
      noTone(speakerPin);
    }
  } else {
    noTone(speakerPin);
  }

  delay(50);
}