#include <Wire.h>
#include <Adafruit_SI5351.h>
#include <LiquidCrystal_I2C.h>
#include <Encoder.h>

// Constants
const uint32_t MIN_FREQUENCY = 143000000; // 143 MHz
const uint32_t MAX_FREQUENCY = 147000000; // 147 MHz
const uint32_t STEP_SIZES[] = {10000, 100000, 1000000};
const uint32_t OFFSET = 600000; // 600 kHz
const unsigned long DEBOUNCE_DELAY = 50; // Button debounce delay

// Pins
const uint8_t PTT_PIN = 5;
const uint8_t MODE_BUTTON_PIN = 6;
const uint8_t STEP_BUTTON_PIN = 4;

// Globals
Adafruit_SI5351 si5351;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Encoder encoder(3, 2);
uint32_t frequency = 145440000;
uint32_t lastFrequency = 0;
uint8_t stepIndex = 0;
uint8_t duplexMode = 0;
bool lastStepButtonState = HIGH;
bool lastModeButtonState = HIGH;
unsigned long lastDebounceTime = 0;

void setup() {
  // Initialize SI5351
  if (si5351.begin() != ERROR_NONE) {
    lcd.print(F("SI5351 error"));
    while (1);
  }
  si5351.setupPLLInt(SI5351_PLL_A, 36);
  si5351.setupMultisynthInt(0, SI5351_PLL_A, 900);
  si5351.enableOutputs(true);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.print(F("Selamat Datang !!"));
  lcd.setCursor(0, 1);
  lcd.print(F("YD1FFB-Ardiansah"));
  delay(1000);
  lcd.clear();

  // Initialize pins
  encoder.write(0);
  pinMode(STEP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PTT_PIN, INPUT_PULLUP);
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  // Read encoder position
  int32_t encoderPos = encoder.read() / 4;
  if (encoderPos != 0) {
    frequency = constrain(frequency + encoderPos * STEP_SIZES[stepIndex], MIN_FREQUENCY, MAX_FREQUENCY);
    encoder.write(0);
  }

  // Read button states
  bool stepButtonState = digitalRead(STEP_BUTTON_PIN);
  bool modeButtonState = digitalRead(MODE_BUTTON_PIN);

  // Handle step size button
  if (stepButtonState != lastStepButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (stepButtonState == LOW) {
      stepIndex = (stepIndex + 1) % 3;
      delay(200);
    }
  }
  lastStepButtonState = stepButtonState;

  // Handle duplex mode button
  if (modeButtonState != lastModeButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (modeButtonState == LOW) {
      duplexMode = (duplexMode + 1) % 3;
      delay(200);
    }
  }
  lastModeButtonState = modeButtonState;

  // Update frequency based on PTT and duplex mode
  uint32_t displayFrequency = frequency;
  if (digitalRead(PTT_PIN) == LOW) {
    if (duplexMode == 1) {
      displayFrequency = max(MIN_FREQUENCY, frequency - OFFSET);
    } else if (duplexMode == 2) {
      displayFrequency = min(MAX_FREQUENCY, frequency + OFFSET);
    }
  }

  // Update SI5351 and LCD if frequency changed or time to update
  if (displayFrequency != lastFrequency || millis() - lastDebounceTime >= 100) {
    lastDebounceTime = millis();
    lastFrequency = displayFrequency;

    si5351.setupPLLInt(SI5351_PLL_A, 36);
    si5351.setupMultisynthInt(0, SI5351_PLL_A, 90000000000UL / displayFrequency);

    lcd.setCursor(0, 0);
    lcd.print("Controller IC2N");
    lcd.setCursor(0, 1);
    lcd.print(digitalRead(PTT_PIN) == LOW ? F("TX:") : F("RX:"));

    // Display frequency
    float frequencyMHz = displayFrequency / 1000000.0;
    lcd.setCursor(3, 1);
    lcd.printf("%.2f MHz", frequencyMHz);

    // Display duplex mode
    lcd.setCursor(14, 1);
    lcd.print(duplexMode == 1 ? F("-") : duplexMode == 2 ? F("+") : F(" "));
  }
}
