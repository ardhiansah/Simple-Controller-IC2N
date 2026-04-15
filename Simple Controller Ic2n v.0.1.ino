#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Encoder.h>
#include <Adafruit_SI5351.h>

// ================= SI5351 RF ENGINE =================
Adafruit_SI5351 si5351;

// ================= LCD =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= ENCODER =================
Encoder encoder(3, 2);

// ================= CONSTANTS =================
const uint32_t MIN_FREQUENCY = 143000000;
const uint32_t MAX_FREQUENCY = 147000000;
const uint32_t STEP_SIZES[] = {10000, 100000, 1000000};
const uint32_t OFFSET = 600000;

// ================= PINS =================
const uint8_t PTT_PIN = 5;
const uint8_t MODE_BUTTON_PIN = 6;
const uint8_t STEP_BUTTON_PIN = 4;
const uint8_t RSSI_PIN = A0;

// ================= GLOBALS =================
uint32_t frequency = 145440000;
uint32_t lastFrequency = 0;

uint8_t stepIndex = 1;
uint8_t duplexMode = 0;

bool lastStepButtonState = HIGH;
bool lastModeButtonState = HIGH;

unsigned long lastDebounceTime = 0;

// ================= ICON =================
byte sigIcon[8] = {
  B00100,
  B01110,
  B11111,
  B00100,
  B00100,
  B01110,
  B00000,
  B00000
};

// ================= BAR GRAPH =================
byte bar1[8] = {B10000,B10000,B10000,B10000,B10000,B10000,B10000,B10000};
byte bar2[8] = {B11000,B11000,B11000,B11000,B11000,B11000,B11000,B11000};
byte bar3[8] = {B11100,B11100,B11100,B11100,B11100,B11100,B11100,B11100};
byte bar4[8] = {B11110,B11110,B11110,B11110,B11110,B11110,B11110,B11110};
byte bar5[8] = {B11111,B11111,B11111,B11111,B11111,B11111,B11111,B11111};

// ================= RF ENGINE =================
void setFrequencyRF(uint32_t f) {

  // CLK0 = TX / VFO utama
  si5351.set_freq((uint64_t)f * 100ULL, SI5351_CLK0);

  // CLK1 = RX / duplex offset
  if (duplexMode == 1) {
    si5351.set_freq((uint64_t)(f - OFFSET) * 100ULL, SI5351_CLK1);
  } 
  else if (duplexMode == 2) {
    si5351.set_freq((uint64_t)(f + OFFSET) * 100ULL, SI5351_CLK1);
  } 
  else {
    si5351.set_freq((uint64_t)f * 100ULL, SI5351_CLK1);
  }
}

void setup() {

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print(F("### WELCOME! ###"));
  lcd.setCursor(0, 1);
  lcd.print(F(" SI5351 VFO RF "));
  delay(1200);
  lcd.clear();

  lcd.createChar(0, sigIcon);
  lcd.createChar(1, bar1);
  lcd.createChar(2, bar2);
  lcd.createChar(3, bar3);
  lcd.createChar(4, bar4);
  lcd.createChar(5, bar5);

  encoder.write(0);

  pinMode(STEP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PTT_PIN, INPUT_PULLUP);
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RSSI_PIN, INPUT);

  // ================= SI5351 INIT =================
  if (!si5351.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("SI5351 ERROR");
    while (1);
  }

  si5351.init(SI5351_CRYSTAL_LOAD_8PF);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA);

  setFrequencyRF(frequency);
}

void loop() {

  // ================= ENCODER =================
  int32_t encoderPos = encoder.read() / 4;

  if (encoderPos != 0) {
    frequency = constrain(
      frequency + encoderPos * STEP_SIZES[stepIndex],
      MIN_FREQUENCY,
      MAX_FREQUENCY
    );

    encoder.write(0);

    setFrequencyRF(frequency);
  }

  // ================= STEP BUTTON =================
  bool stepButtonState = digitalRead(STEP_BUTTON_PIN);

  if (stepButtonState != lastStepButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 50) {
    if (stepButtonState == LOW) {
      stepIndex = (stepIndex + 1) % 3;
      delay(150);
    }
  }
  lastStepButtonState = stepButtonState;

  // ================= MODE BUTTON =================
  bool modeButtonState = digitalRead(MODE_BUTTON_PIN);

  if (modeButtonState != lastModeButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 50) {
    if (modeButtonState == LOW) {
      duplexMode = (duplexMode + 1) % 3;
      setFrequencyRF(frequency);
      delay(150);
    }
  }
  lastModeButtonState = modeButtonState;

  // ================= RSSI =================
  static int filtered = 0;
  int raw = analogRead(RSSI_PIN);
  filtered = (filtered * 3 + raw) / 4;

  int level = map(filtered, 0, 1023, 0, 5);

  // ================= PTT =================
  bool tx = (digitalRead(PTT_PIN) == LOW);

  if (tx) {
    setFrequencyRF(frequency);
  }

  uint32_t displayFrequency = frequency;

  if (tx) {
    if (duplexMode == 1) displayFrequency = frequency - OFFSET;
    else if (duplexMode == 2) displayFrequency = frequency + OFFSET;
  }

  // ================= LCD UPDATE =================
  if (displayFrequency != lastFrequency || millis() - lastDebounceTime >= 100) {

    lastFrequency = displayFrequency;

    // LINE 1
    lcd.setCursor(0, 0);
    lcd.print(tx ? "TX:" : "RX:");

    float mhz = displayFrequency / 1000000.0;
    lcd.setCursor(3, 0);
    lcd.print(mhz, 3);
    lcd.print(" MHz ");

    lcd.setCursor(14, 0);
    lcd.print(duplexMode == 1 ? "-" :
              duplexMode == 2 ? "+" : " ");

    // LINE 2
    lcd.setCursor(0, 1);
    lcd.write((uint8_t)0);

    for (int i = 0; i < 5; i++) {
      if (i < level) lcd.write((uint8_t)5);
      else lcd.print(" ");
    }
  }
}
