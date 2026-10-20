/*
 * ============================================================================
 *  ADS1232 + Load Cell test bench for ESP32  (Arduino IDE)
 * ============================================================================
 *
 *  - ADS1232 24-bit ADC driven by hand (bit-banged SCLK, no external library)
 *  - SSD1306 128x64 I2C OLED for local read-out
 *  - 3 push buttons: TARE / CAL / UNIT
 *
 *  Libraries required (Library Manager):
 *      Adafruit GFX Library
 *      Adafruit SSD1306
 *
 *  Board: "ESP32 Dev Module" (or any ESP32 variant)
 *
 *  ---------------------------------------------------------------------------
 *  WIRING
 *  ---------------------------------------------------------------------------
 *      ADS1232                 ESP32
 *      -------                 -----
 *      DOUT/DRDY   ---------->  GPIO34   (input only pin - OK, it is an input)
 *      SCLK        <----------  GPIO25
 *      PDWN        <----------  GPIO26
 *      SPEED       <----------  GPIO27
 *      A0          <----------  GPIO14
 *      VDD/AVDD    ----------   5V (or 3.3V, check your module)
 *      GND         ----------   GND
 *      *** DOUT must be 3.3 V logic. If your ADS1232 board runs at 5 V,
 *          put a divider / level shifter on DOUT before GPIO34. ***
 *
 *      SSD1306 OLED            ESP32
 *      ------------            -----
 *      SDA         <--------->  GPIO21
 *      SCL         <----------  GPIO22
 *      VCC         ----------   3.3V
 *      GND         ----------   GND
 *
 *      Buttons (other side of every button goes to GND, INPUT_PULLUP used)
 *      TARE  ->  GPIO32
 *      CAL   ->  GPIO33
 *      UNIT  ->  GPIO13
 *
 *  ---------------------------------------------------------------------------
 *  ARCHITECTURE NOTE (requirement 19: OLED -> Nextion later)
 *  ---------------------------------------------------------------------------
 *  All measurement logic writes into ONE structure (ScaleData). The display
 *  layer only ever reads that structure through three functions:
 *
 *      displayInit()  /  displayRender(const ScaleData&)  /  displayMessage()
 *
 *  To move to a Nextion panel you only rewrite those three functions
 *  (send "t0.txt=..." style commands over Serial2). Nothing else changes.
 * ============================================================================
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===========================================================================
//  1. PIN MAP
// ===========================================================================
#define DOUT_PIN   34      // ADS1232 DOUT/DRDY  (data out + data-ready flag)
#define SCLK_PIN   25      // ADS1232 SCLK       (we generate this clock)
#define PDWN_PIN   26      // ADS1232 PDWN       (HIGH = running, LOW = power down)
#define SPEED_PIN  27      // ADS1232 SPEED      (LOW = 10 SPS, HIGH = 80 SPS)
#define A0_PIN     14      // ADS1232 A0         (LOW = channel A, HIGH = channel B)

#define BTN_TARE   32
#define BTN_CAL    33
#define BTN_UNIT   13

// ===========================================================================
//  2. OLED
// ===========================================================================
#define OLED_W        128
#define OLED_H         64
#define OLED_RESET     -1     // no reset pin on most I2C modules
#define OLED_ADDR    0x3D     // some modules are 0x3D

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET);

// ===========================================================================
//  3. TUNABLE CONSTANTS
// ===========================================================================
#define ADS_SPEED_HIGH      false   // false = 10 SPS (quieter), true = 80 SPS
#define ADS_CHANNEL_B       false   // false = channel A (normal load cell input)

#define AVG_SAMPLES           20    // requirement 6: >= 20 samples averaged
#define EMA_ALPHA           0.20f   // requirement 7: exponential filter weight
                                    // smaller = smoother but slower to settle

#define DEBOUNCE_MS           50    // requirement 15
#define DISPLAY_PERIOD_MS    100    // requirement 16: ~10 Hz refresh
#define SERIAL_PERIOD_MS     500    // debug print rate (2 Hz, keeps log readable)

#define ADS_READ_TIMEOUT_MS  400    // one conversion @10 SPS = 100 ms, 4x margin
#define CAL_INPUT_TIMEOUT_MS 30000  // how long we wait for the operator to type

// Default calibration factor = ADC counts per gram.
// 1.0 means "raw counts are shown as grams" until you actually calibrate.
#define DEFAULT_CAL_FACTOR   1.0f

// ===========================================================================
//  4. MEASUREMENT STATE  (the only thing the display layer is allowed to read)
// ===========================================================================
struct ScaleData {
  int32_t rawValue;        // last raw 24-bit signed sample
  float   filteredValue;   // moving-average + exponential filtered value
  float   zeroOffset;      // tare point, in ADC counts
  float   calFactor;       // ADC counts per gram
  float   netCounts;       // filtered - zeroOffset
  float   weightGrams;     // net / calFactor
  float   weightDisplay;   // same weight expressed in the selected unit
  bool    unitIsKg;        // false = g, true = kg
  bool    sensorOK;        // false if the ADS1232 stopped answering
};

ScaleData gScale = {
  0, 0.0f, 0.0f, DEFAULT_CAL_FACTOR, 0.0f, 0.0f, 0.0f, false, false
};

// Set true only if the SSD1306 actually ACKed on the I2C bus. Every display
// call is gated on this: without the gate a missing panel produces an endless
// "I2C hardware NACK detected" flood that buries the measurement output.
bool gDisplayPresent = false;

// --- moving average ring buffer -------------------------------------------
int32_t  maBuffer[AVG_SAMPLES];
uint8_t  maIndex   = 0;
uint8_t  maCount   = 0;      // how many valid entries are in the buffer
int64_t  maSum     = 0;      // 64-bit: 20 * 2^23 would overflow nothing, but
                             // stays safe if AVG_SAMPLES is increased a lot

bool  emaPrimed = false;     // first sample loads the EMA directly

// --- button bookkeeping ----------------------------------------------------
struct Button {
  uint8_t  pin;
  bool     lastStable;      // last debounced level (HIGH = released)
  bool     lastRead;        // last raw level
  uint32_t lastChangeMs;    // when the raw level last changed
};

Button btnTare = { BTN_TARE, HIGH, HIGH, 0 };
Button btnCal  = { BTN_CAL,  HIGH, HIGH, 0 };
Button btnUnit = { BTN_UNIT, HIGH, HIGH, 0 };

// --- scheduling ------------------------------------------------------------
uint32_t lastDisplayMs = 0;
uint32_t lastSerialMs  = 0;

// ESP32 critical section handle: the 24 clock pulses must not be interrupted
// by the WiFi/RTOS tick, otherwise SCLK can stay high long enough to shift a
// wrong bit out. We keep the critical section as short as possible.
portMUX_TYPE adsMux = portMUX_INITIALIZER_UNLOCKED;

// ===========================================================================
//  Forward declarations (requirement 14: modular functions)
// ===========================================================================
void    initADS1232();
bool    readADS1232(int32_t &value);                       // one raw sample
bool    readAverage(uint8_t samples, float &average);      // blocking average
void    tare();
void    calibrate();
float   getWeight();
void    updateDisplay();
void    handleButtons();

void    i2cScan();
void    displayInit();
void    displayRender(const ScaleData &d);
void    displayMessage(const char *l1, const char *l2, const char *l3);

void    pushSample(int32_t raw);
void    resetFilter();
bool    buttonPressed(Button &b);
void    printDebug();
float   readSerialFloat(uint32_t timeoutMs);

// ===========================================================================
//  SETUP
// ===========================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("================================================"));
  Serial.println(F(" ESP32 + ADS1232 load cell test"));
  Serial.println(F(" TARE=GPIO32   CAL=GPIO33   UNIT=GPIO13"));
  Serial.println(F("================================================"));

  // ---- buttons ------------------------------------------------------------
  pinMode(BTN_TARE, INPUT_PULLUP);
  pinMode(BTN_CAL,  INPUT_PULLUP);
  pinMode(BTN_UNIT, INPUT_PULLUP);

  // ---- OLED ---------------------------------------------------------------
  displayInit();
  displayMessage("ADS1232 Scale", "Starting...", "");

  // ---- ADC ----------------------------------------------------------------
  initADS1232();

  displayMessage("ADS1232 Scale", "Ready", "TARE to zero");
  delay(800);
}

// ===========================================================================
//  MAIN LOOP  - fully non-blocking so the buttons always feel responsive
// ===========================================================================
void loop() {
  // 1) Grab a sample if (and only if) the ADS1232 says one is ready.
  //    DOUT/DRDY low = new conversion available.
  if (digitalRead(DOUT_PIN) == LOW) {
    int32_t raw;
    if (readADS1232(raw)) {
      pushSample(raw);            // updates raw / moving average / EMA
      gScale.sensorOK = true;
    }
  }

  // 2) Recompute the derived values from the filtered reading.
  gScale.netCounts    = gScale.filteredValue - gScale.zeroOffset;
  gScale.weightGrams  = gScale.netCounts / gScale.calFactor;
  gScale.weightDisplay= getWeight();

  // 3) User input.
  handleButtons();

  // 4) Output.
  if (millis() - lastDisplayMs >= DISPLAY_PERIOD_MS) {
    lastDisplayMs = millis();
    updateDisplay();
  }
  if (millis() - lastSerialMs >= SERIAL_PERIOD_MS) {
    lastSerialMs = millis();
    printDebug();
  }
}

// ===========================================================================
//  ADS1232 LOW LEVEL
// ===========================================================================
/*
 *  initADS1232()
 *  -------------
 *  PDWN LOW -> HIGH performs the internal reset/power-up sequence.
 *  SPEED and A0 must be set BEFORE leaving power-down; changing them later
 *  makes the next two conversions invalid, so we always throw samples away
 *  after touching them.
 */
void initADS1232() {
  pinMode(DOUT_PIN,  INPUT);      // GPIO34 has no internal pull-up, none needed
  pinMode(SCLK_PIN,  OUTPUT);
  pinMode(PDWN_PIN,  OUTPUT);
  pinMode(SPEED_PIN, OUTPUT);
  pinMode(A0_PIN,    OUTPUT);

  digitalWrite(SCLK_PIN,  LOW);   // clock idles low
  digitalWrite(SPEED_PIN, ADS_SPEED_HIGH ? HIGH : LOW);   // requirement 4
  digitalWrite(A0_PIN,    ADS_CHANNEL_B  ? HIGH : LOW);   // LOW = channel A

  // Power-down pulse: hold PDWN low >= 26 us, then release.
  digitalWrite(PDWN_PIN, LOW);
  delay(10);
  digitalWrite(PDWN_PIN, HIGH);   // normal operation
  delay(50);                      // let the modulator settle

  Serial.print(F("[ADS1232] init: SPEED="));
  Serial.print(ADS_SPEED_HIGH ? F("80 SPS") : F("10 SPS"));
  Serial.print(F("  channel="));
  Serial.println(ADS_CHANNEL_B ? F("B") : F("A"));

  // Discard the first conversions - they are not settled yet after a
  // reset or after a SPEED / A0 change (datasheet: 2 invalid conversions).
  int32_t dummy;
  for (uint8_t i = 0; i < 3; i++) {
    uint32_t t0 = millis();
    while (digitalRead(DOUT_PIN) == HIGH && (millis() - t0) < ADS_READ_TIMEOUT_MS) {
      delay(1);
    }
    readADS1232(dummy);
  }

  resetFilter();
}

/*
 *  readADS1232()
 *  -------------
 *  Bit-banged serial read, straight from the ADS1232 datasheet:
 *
 *    - DOUT/DRDY is HIGH while a conversion is running, and goes LOW when a
 *      new result is ready.
 *    - Each SCLK rising edge shifts the next bit out; the bit is sampled by
 *      the master while SCLK is HIGH (we read just before the falling edge).
 *    - 24 bits are sent MSB first, in two's complement format.
 *    - A 25th clock pulse forces DOUT/DRDY back HIGH so the "data ready"
 *      flag is clean for the next conversion.
 *      (A 26th pulse would trigger an offset self-calibration - we do NOT
 *       send it here, tare() does that job in software instead.)
 *
 *  Returns false if no data was ready inside ADS_READ_TIMEOUT_MS.
 */
bool readADS1232(int32_t &value) {
  // Wait for data ready (returns immediately if loop() already saw DOUT low).
  uint32_t t0 = millis();
  while (digitalRead(DOUT_PIN) == HIGH) {
    if (millis() - t0 >= ADS_READ_TIMEOUT_MS) {
      gScale.sensorOK = false;
      return false;                     // ADC not answering: bad wiring/power
    }
    delayMicroseconds(200);
  }

  uint32_t raw = 0;

  // The bit-bang itself must not be preempted -> short critical section.
  portENTER_CRITICAL(&adsMux);
  for (uint8_t i = 0; i < 24; i++) {
    digitalWrite(SCLK_PIN, HIGH);
    delayMicroseconds(1);               // t(SCLK high) >= 0.1 us, 1 us is safe
    raw = (raw << 1) | (uint32_t)digitalRead(DOUT_PIN);
    digitalWrite(SCLK_PIN, LOW);
    delayMicroseconds(1);
  }
  // 25th pulse: release DRDY (no offset calibration requested).
  digitalWrite(SCLK_PIN, HIGH);
  delayMicroseconds(1);
  digitalWrite(SCLK_PIN, LOW);
  delayMicroseconds(1);
  portEXIT_CRITICAL(&adsMux);

  // Sign-extend the 24-bit two's complement result into a 32-bit signed int:
  // if bit 23 is set the number is negative, so fill the top byte with 1s.
  if (raw & 0x800000UL) {
    raw |= 0xFF000000UL;
  }
  value = (int32_t)raw;
  return true;
}

/*
 *  readAverage()
 *  -------------
 *  Blocking helper used by tare() and calibrate(): takes `samples` fresh
 *  conversions and returns their arithmetic mean. Returns false if too many
 *  reads timed out (broken sensor) so callers never use a garbage value.
 */
bool readAverage(uint8_t samples, float &average) {
  int64_t sum   = 0;
  uint8_t good  = 0;

  for (uint8_t i = 0; i < samples; i++) {
    int32_t v;
    if (readADS1232(v)) {              // readADS1232 waits for DRDY itself
      sum += v;
      good++;
    }
  }

  if (good < samples / 2) {            // more than half failed -> give up
    return false;
  }
  average = (float)sum / (float)good;
  return true;
}

// ===========================================================================
//  SOFTWARE FILTER  (requirements 6 + 7)
//  Two stages:
//    stage 1: moving average over AVG_SAMPLES raw conversions (kills random
//             noise, gives a stable baseline)
//    stage 2: exponential moving average on top of it (smooths the steps and
//             costs almost nothing in RAM/CPU)
// ===========================================================================
void pushSample(int32_t raw) {
  gScale.rawValue = raw;

  // --- stage 1: ring-buffer moving average ---------------------------------
  if (maCount == AVG_SAMPLES) {
    maSum -= maBuffer[maIndex];        // drop the oldest sample
  } else {
    maCount++;
  }
  maBuffer[maIndex] = raw;
  maSum += raw;
  maIndex = (maIndex + 1) % AVG_SAMPLES;

  float movingAvg = (float)maSum / (float)maCount;

  // --- stage 2: exponential filter -----------------------------------------
  if (!emaPrimed) {
    gScale.filteredValue = movingAvg;  // first value: jump straight there
    emaPrimed = true;
  } else {
    gScale.filteredValue =
        (EMA_ALPHA * movingAvg) + ((1.0f - EMA_ALPHA) * gScale.filteredValue);
  }
}

void resetFilter() {
  maSum     = 0;
  maIndex   = 0;
  maCount   = 0;
  emaPrimed = false;
}

// ===========================================================================
//  SCALE OPERATIONS
// ===========================================================================
/*
 *  tare()
 *  ------
 *  Stores the current (unloaded) reading as the zero point. Everything after
 *  this is measured relative to it:  net = filtered - zeroOffset.
 */
void tare() {
  Serial.println(F("[TARE] averaging, keep the platform empty and still..."));
  displayMessage("TARE", "Averaging...", "keep platform empty");

  float avg;
  if (!readAverage(AVG_SAMPLES, avg)) {
    Serial.println(F("[TARE] FAILED - no data from ADS1232"));
    displayMessage("TARE", "FAILED", "check ADC wiring");
    delay(1500);
    return;
  }

  gScale.zeroOffset = avg;
  resetFilter();                       // restart the filter at the new zero
  gScale.filteredValue = avg;
  emaPrimed = true;

  Serial.print(F("[TARE] zero offset = "));
  Serial.println(gScale.zeroOffset, 2);
  displayMessage("TARE", "Done", "zero stored");
  delay(700);
}

/*
 *  calibrate()
 *  -----------
 *  1. Re-zeroes with an empty platform.
 *  2. Asks the operator (Serial Monitor) to put a known weight on.
 *  3. Reads the loaded average.
 *  4. calFactor = (loadedCounts - zeroCounts) / knownGrams   [counts per gram]
 *
 *  Serial Monitor must be set to 115200 baud with "Newline" line ending.
 */
void calibrate() {
  Serial.println();
  Serial.println(F("=========== CALIBRATION ==========="));
  Serial.println(F("Step 1: remove everything from the platform."));
  displayMessage("CALIBRATION", "Step 1", "empty platform");
  delay(2500);

  float zeroAvg;
  if (!readAverage(AVG_SAMPLES, zeroAvg)) {
    Serial.println(F("[CAL] FAILED - no data from ADS1232"));
    displayMessage("CALIBRATION", "FAILED", "no ADC data");
    delay(1500);
    return;
  }
  Serial.print(F("[CAL] empty reading = "));
  Serial.println(zeroAvg, 2);

  Serial.println(F("Step 2: place the KNOWN weight on the platform."));
  Serial.println(F("Step 3: type its mass in GRAMS and press Enter."));
  Serial.println(F("        (e.g. 500  or 1000.0    - send 0 to abort)"));
  displayMessage("CALIBRATION", "Place weight", "enter grams on Serial");

  float knownGrams = readSerialFloat(CAL_INPUT_TIMEOUT_MS);

  if (isnan(knownGrams)) {
    Serial.println(F("[CAL] timeout - calibration aborted."));
    displayMessage("CALIBRATION", "Aborted", "serial timeout");
    delay(1500);
    return;
  }
  if (knownGrams <= 0.0f) {
    Serial.println(F("[CAL] invalid / zero weight - aborted."));
    displayMessage("CALIBRATION", "Aborted", "invalid weight");
    delay(1500);
    return;
  }

  Serial.print(F("[CAL] known weight = "));
  Serial.print(knownGrams, 3);
  Serial.println(F(" g - averaging, do not touch the scale..."));
  displayMessage("CALIBRATION", "Measuring...", "hold still");

  float loadedAvg;
  if (!readAverage(AVG_SAMPLES, loadedAvg)) {
    Serial.println(F("[CAL] FAILED - no data from ADS1232"));
    displayMessage("CALIBRATION", "FAILED", "no ADC data");
    delay(1500);
    return;
  }

  float deltaCounts = loadedAvg - zeroAvg;

  // Guard: if the counts barely moved, the load cell is not connected, the
  // weight is far too small, or the excitation is missing.
  if (fabsf(deltaCounts) < 100.0f) {
    Serial.print(F("[CAL] FAILED - delta of only "));
    Serial.print(deltaCounts, 2);
    Serial.println(F(" counts. Check the load cell / use a heavier weight."));
    displayMessage("CALIBRATION", "FAILED", "delta too small");
    delay(2000);
    return;
  }

  gScale.zeroOffset = zeroAvg;                    // zero measured in step 1
  gScale.calFactor  = deltaCounts / knownGrams;   // <-- counts per gram

  resetFilter();

  Serial.println(F("---------- CALIBRATION DONE ----------"));
  Serial.print(F("  empty  counts : ")); Serial.println(zeroAvg, 2);
  Serial.print(F("  loaded counts : ")); Serial.println(loadedAvg, 2);
  Serial.print(F("  delta  counts : ")); Serial.println(deltaCounts, 2);
  Serial.print(F("  known weight  : ")); Serial.print(knownGrams, 3); Serial.println(F(" g"));
  Serial.print(F("  CAL FACTOR    : ")); Serial.print(gScale.calFactor, 6);
  Serial.println(F(" counts/gram"));
  Serial.println(F("  weight = (filtered - zeroOffset) / calFactor"));
  Serial.println(F("--------------------------------------"));

  displayMessage("CALIBRATION", "Done", "factor stored");
  delay(1200);
}

/*
 *  getWeight()
 *  -----------
 *  Returns the current weight expressed in the unit selected by the UNIT
 *  button. All internal maths stays in grams; only the presentation changes.
 */
float getWeight() {
  float grams = (gScale.filteredValue - gScale.zeroOffset) / gScale.calFactor;
  return gScale.unitIsKg ? (grams / 1000.0f) : grams;
}

// ===========================================================================
//  BUTTONS  (requirement 15: every button debounced, edge triggered)
// ===========================================================================
/*
 *  buttonPressed()
 *  ---------------
 *  Classic time-based debounce: a level change is only accepted after the
 *  input has been stable for DEBOUNCE_MS. Returns true once per press
 *  (on the HIGH -> LOW edge, because of INPUT_PULLUP).
 */
bool buttonPressed(Button &b) {
  bool now = digitalRead(b.pin);

  if (now != b.lastRead) {             // raw level moved: restart the timer
    b.lastRead      = now;
    b.lastChangeMs  = millis();
    return false;
  }

  if ((millis() - b.lastChangeMs) >= DEBOUNCE_MS && now != b.lastStable) {
    b.lastStable = now;                // level is stable and different -> edge
    if (now == LOW) {
      return true;                     // falling edge = button just pressed
    }
  }
  return false;
}

void handleButtons() {
  if (buttonPressed(btnTare)) {
    Serial.println(F("[BTN] TARE"));
    tare();
  }

  if (buttonPressed(btnCal)) {
    Serial.println(F("[BTN] CAL"));
    calibrate();
  }

  if (buttonPressed(btnUnit)) {
    gScale.unitIsKg = !gScale.unitIsKg;
    Serial.print(F("[BTN] UNIT -> "));
    Serial.println(gScale.unitIsKg ? F("kg") : F("g"));
  }
}

/*
 *  readSerialFloat()
 *  -----------------
 *  Blocking-with-timeout line reader. Collects characters until CR/LF and
 *  converts them to a float. Returns NAN on timeout so the caller can abort.
 */
float readSerialFloat(uint32_t timeoutMs) {
  char     buf[24];
  uint8_t  len = 0;
  uint32_t t0  = millis();

  while (Serial.available()) Serial.read();      // flush anything stale

  while ((millis() - t0) < timeoutMs) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\r' || c == '\n') {
        if (len == 0) continue;                  // ignore leading line endings
        buf[len] = '\0';
        return atof(buf);
      }
      if (len < sizeof(buf) - 1) {
        buf[len++] = c;
      }
    }
    delay(2);                                    // feed the RTOS watchdog
  }
  return NAN;                                    // timed out
}

// ===========================================================================
//  DEBUG OUTPUT  (requirement 12)
// ===========================================================================
void printDebug() {
  Serial.print(F("RAW="));      Serial.print(gScale.rawValue);
  Serial.print(F("  FILT="));   Serial.print(gScale.filteredValue, 1);
  Serial.print(F("  ZERO="));   Serial.print(gScale.zeroOffset, 1);
  Serial.print(F("  NET="));    Serial.print(gScale.netCounts, 1);
  Serial.print(F("  CALF="));   Serial.print(gScale.calFactor, 4);
  Serial.print(F("  W="));      Serial.print(gScale.weightDisplay, gScale.unitIsKg ? 3 : 1);
  Serial.print(gScale.unitIsKg ? F(" kg") : F(" g"));
  if (!gScale.sensorOK) Serial.print(F("   [!! NO ADC DATA !!]"));
  Serial.println();
}

// ===========================================================================
//  DISPLAY LAYER  -  replace ONLY this block to switch to a Nextion panel
// ===========================================================================
void updateDisplay() {
  displayRender(gScale);       // measurement logic never talks to the OLED
}

/*
 *  i2cScan()
 *  ---------
 *  Walks the whole 7-bit address space and prints whatever answers. This is
 *  the fastest way to tell "panel is dead / not wired" apart from
 *  "panel is alive but on the other address".
 */
void i2cScan() {
  uint8_t found = 0;
  Serial.println(F("[I2C] scanning bus (SDA=21, SCL=22)..."));
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("[I2C]   device at 0x"));
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) {
    Serial.println(F("[I2C]   NOTHING FOUND - check SDA/SCL/VCC/GND wiring"));
  }
}

void displayInit() {
  Wire.begin(21, 22);                       // SDA=21, SCL=22
  i2cScan();                                // tell the operator what is there

  // Try the two addresses every SSD1306 module in the wild uses.
  const uint8_t candidates[2] = { 0x3C, 0x3D };
  for (uint8_t i = 0; i < 2; i++) {
    if (display.begin(SSD1306_SWITCHCAPVCC, candidates[i])) {
      gDisplayPresent = true;
      Serial.print(F("[OLED] SSD1306 found at 0x"));
      Serial.println(candidates[i], HEX);
      break;
    }
  }

  if (!gDisplayPresent) {
    // We deliberately do NOT halt: the scale must keep working over Serial
    // even without a display. All display calls become no-ops from here.
    Serial.println(F("[OLED] SSD1306 NOT FOUND at 0x3C or 0x3D."));
    Serial.println(F("[OLED] Running headless - readings go to Serial only."));
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();
}

/*
 *  displayRender()
 *  ---------------
 *  Layout (128x64):
 *      RAW    <raw>
 *      ZERO   <offset>
 *      CALF   <counts/gram>
 *      ------------------------
 *      <big weight>        g/kg
 */
void displayRender(const ScaleData &d) {
  if (!gDisplayPresent) return;        // no panel -> no I2C traffic, no NACKs
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("RAW  "));
  display.println(d.rawValue);

  display.setCursor(0, 10);
  display.print(F("ZERO "));
  display.println(d.zeroOffset, 0);

  display.setCursor(0, 20);
  display.print(F("CALF "));
  display.println(d.calFactor, 3);

  display.drawFastHLine(0, 31, OLED_W, SSD1306_WHITE);

  // Big weight line
  display.setTextSize(2);
  display.setCursor(0, 38);
  if (!d.sensorOK) {
    display.print(F("-- ERR"));
  } else {
    display.print(d.weightDisplay, d.unitIsKg ? 3 : 1);
  }

  // Unit in the bottom-right corner
  display.setTextSize(2);
  display.setCursor(d.unitIsKg ? 96 : 110, 38);
  display.print(d.unitIsKg ? F("kg") : F("g"));

  display.display();
}

/*
 *  displayMessage()
 *  ----------------
 *  Simple 3-line status screen used during TARE / CALIBRATION.
 */
void displayMessage(const char *l1, const char *l2, const char *l3) {
  // Mirror status messages to Serial so a headless rig is still usable.
  Serial.print(F("[UI] ")); Serial.print(l1);
  Serial.print(F(" | "));   Serial.print(l2);
  Serial.print(F(" | "));   Serial.println(l3);

  if (!gDisplayPresent) return;        // no panel -> no I2C traffic, no NACKs
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 4);   display.println(l1);
  display.drawFastHLine(0, 15, OLED_W, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 24);  display.println(l2);
  display.setCursor(0, 40);  display.println(l3);
  display.display();
}
