#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Encoder.h>
#include <si5351.h>

// --- KONSTANTA ---
const uint32_t MIN_FREQUENCY = 143000000;
const uint32_t MAX_FREQUENCY = 147000000;
const uint32_t STEP_SIZES[] = {10000, 100000, 1000000};
const uint8_t TOTAL_STEPS = 3; 
const uint32_t OFFSET = 600000;
const unsigned long DEBOUNCE_DELAY = 50;

// --- PIN ---
const uint8_t PTT_PIN = 5;
const uint8_t MODE_BUTTON_PIN = 6;
const uint8_t ENCODER_SW_PIN = 4; 
const uint8_t RSSI_PIN = A0;

// --- GLOBALS ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
Encoder encoder(3, 2);
Si5351 si5351;

uint32_t frequency = 144000000; 
uint32_t lastFrequency = 0;
uint32_t lastSiFreq = 0; 
uint8_t stepIndex = 0; 
uint8_t duplexMode = 0; 

bool lastSwState = HIGH;
bool lastModeButtonState = HIGH;
unsigned long lastDebounceTime = 0;

unsigned long blinkStartTime = 0;
const unsigned long BLINK_DURATION = 2000; 
bool isBlinking = false;

unsigned long pttStartTime = 0;
unsigned long talkDuration = 0;
bool wasTransmitting = false;
bool txAllowed = true;

// --- CUSTOM CHARACTERS ---
byte sigIcon[8] = { B00100, B01110, B11111, B00100, B00100, B01110, B00000, B00000 };
byte bar5[8]    = { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111 };

void setup() {
  lcd.init();
  lcd.backlight();

  bool siInit = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  if (!siInit) {
    lcd.setCursor(0, 0);
    lcd.print(F("Si5351 Error!"));
    while (1); 
  }
  
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.output_enable(SI5351_CLK0, 1);

  lcd.setCursor(0, 0);
  lcd.print(F("### WELCOME! ###"));
  lcd.setCursor(0, 1);
  lcd.print(F("   - YD1ABI -   "));
  delay(1500);
  lcd.clear();

  lcd.createChar(0, sigIcon);
  lcd.createChar(5, bar5);

  encoder.write(0);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  pinMode(PTT_PIN, INPUT_PULLUP);
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RSSI_PIN, INPUT);
}

void loop() {
  // --- LOGIKA ENCODER ---
  int32_t encoderPos = encoder.read() / 4;
  if (encoderPos != 0) {
    frequency = constrain(
      frequency + (encoderPos * STEP_SIZES[stepIndex]),
      MIN_FREQUENCY,
      MAX_FREQUENCY
    );
    encoder.write(0);
  }

  // --- LOGIKA TOMBOL ENCODER ---
  bool swState = digitalRead(ENCODER_SW_PIN);
  if (swState == LOW && lastSwState == HIGH) {
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
      stepIndex = (stepIndex + 1) % TOTAL_STEPS;
      lastDebounceTime = millis();
      blinkStartTime = millis();
      isBlinking = true;
    }
  }
  lastSwState = swState;

  if (isBlinking && (millis() - blinkStartTime > BLINK_DURATION)) {
    isBlinking = false;
  }

  // --- LOGIKA TOMBOL DUPLEX ---
  bool modeButtonState = digitalRead(MODE_BUTTON_PIN);
  if (modeButtonState == LOW && lastModeButtonState == HIGH) {
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
      duplexMode = (duplexMode + 1) % 3;
      lastDebounceTime = millis();
    }
  }
  lastModeButtonState = modeButtonState;

  // --- LOGIKA PTT & PROTEKSI ---
  bool isTransmitting = (digitalRead(PTT_PIN) == LOW);
  uint32_t currentFreq = frequency;
  txAllowed = true;

  if (isTransmitting) {
    if (duplexMode == 1) currentFreq = frequency - OFFSET;
    else if (duplexMode == 2) currentFreq = frequency + OFFSET;

    if (currentFreq < MIN_FREQUENCY || currentFreq > MAX_FREQUENCY) {
      txAllowed = false;
    }

    if (txAllowed) {
      if (!wasTransmitting) {
        pttStartTime = millis();
        wasTransmitting = true;
      }
      talkDuration = (millis() - pttStartTime) / 1000;
    } else {
      wasTransmitting = false;
      talkDuration = 0;
    }
  } else {
    wasTransmitting = false;
    talkDuration = 0; 
  }

  // --- UPDATE Si5351 ---
  if (isTransmitting && !txAllowed) {
    si5351.output_enable(SI5351_CLK0, 0); 
    lastSiFreq = 0; 
  } 
  else if (currentFreq != lastSiFreq) {
    si5351.output_enable(SI5351_CLK0, 1);
    si5351.set_freq(currentFreq * 100ULL, SI5351_CLK0);
    lastSiFreq = currentFreq;
  }

  // --- UPDATE TAMPILAN LCD ---
  static unsigned long lastUpdate = 0;
  static bool lastTxState = false; // Untuk mendeteksi perubahan status TX

  // Update LCD jika frekuensi berubah, atau status TX berubah, atau interval 100ms
  if (currentFreq != lastFrequency || isTransmitting != lastTxState || millis() - lastUpdate > 100) {
    lastFrequency = currentFreq;
    lastTxState = isTransmitting;
    lastUpdate = millis();

    // JIKA TERJADI ERROR TRANSMIT
    if (isTransmitting && !txAllowed) {
      lcd.setCursor(0, 0);
      lcd.print(F(" ERROR TRANSMIT "));
      lcd.setCursor(0, 1);
      lcd.print(F(" OUT OF RANGE!  "));
    } 
    else {
      // TAMPILAN NORMAL
      lcd.setCursor(0, 0);
      lcd.print(isTransmitting ? F("TX:") : F("RX:"));

      char freqStr[12];
      dtostrf(currentFreq / 1000000.0, 7, 3, freqStr); 
      bool hideDigit = (isBlinking && ((millis() / 150) % 2 == 0));

      for (int i = 0; i < 7; i++) {
        lcd.setCursor(3 + i, 0);
        bool isBlinkTarget = false;
        uint32_t s = STEP_SIZES[stepIndex];
        if (s == 1000000 && i == 2) isBlinkTarget = true;      
        else if (s == 100000 && i == 4) isBlinkTarget = true;   
        else if (s == 10000 && i == 5) isBlinkTarget = true;    

        if (isBlinkTarget && hideDigit) lcd.print(" ");
        else lcd.print(freqStr[i]);
      }
      lcd.print(F(" MHz "));
      
      lcd.setCursor(15, 0);
      if (duplexMode == 1) lcd.print(F("-"));
      else if (duplexMode == 2) lcd.print(F("+"));
      else lcd.print(F(" "));

      // Baris 1
      lcd.setCursor(0, 1);
      uint32_t stp = STEP_SIZES[stepIndex];
      if (stp == 10000)      lcd.print(F("10K "));
      else if (stp == 100000)  lcd.print(F("100K"));
      else if (stp == 1000000) lcd.print(F("1M  "));

      lcd.setCursor(4, 1);
      lcd.print(" ");
      lcd.write((uint8_t)0); 
      int level = isTransmitting ? 6 : map(analogRead(RSSI_PIN), 0, 1023, 0, 6);
      for (int i = 0; i < 6; i++) {
        if (i < level) lcd.write((uint8_t)5);
        else lcd.print(" ");
      }

      lcd.setCursor(12, 1);
      lcd.print(" ");
      if (talkDuration < 10) lcd.print("0"); 
      lcd.print(talkDuration);
      lcd.print("s");
    }
  }
}
