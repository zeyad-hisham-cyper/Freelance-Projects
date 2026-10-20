#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define BTN_TARE 32
#define BTN_MODE 33
#define BTN_CAL  4

Adafruit_SH1106G display(128, 64, &Wire);

enum Unit
{
  GRAM = 0,
  KILOGRAM,
  OUNCE,
  POUND
};

Unit currentUnit = KILOGRAM;

float baseWeight = 12.345;
float tareOffset = 0.0;

bool calibrationMode = false;

bool lastTare = HIGH;
bool lastMode = HIGH;
bool lastCal  = HIGH;

unsigned long lastPrint = 0;

void setup()
{
  Serial.begin(115200);

  Wire.begin(21,22);

  pinMode(BTN_TARE, INPUT_PULLUP);
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_CAL, INPUT_PULLUP);

  if (!display.begin(0x3C, true))
  {
    Serial.println("OLED Init Failed");
    while (1);
  }

  Serial.println();
  Serial.println("===== SCALE TEST =====");
}

void loop()
{
  bool tare = digitalRead(BTN_TARE);
  bool mode = digitalRead(BTN_MODE);
  bool cal  = digitalRead(BTN_CAL);

  // Print buttons every 200ms
  if (millis() - lastPrint > 200)
  {
    lastPrint = millis();

    Serial.print("T=");
    Serial.print(tare);

    Serial.print(" M=");
    Serial.print(mode);

    Serial.print(" C=");
    Serial.println(cal);
  }

  //---------------- TARE ----------------

  if(lastTare==HIGH && tare==LOW)
  {
    Serial.println(">>> TARE PRESSED");

    tareOffset = baseWeight;
    calibrationMode = false;

    delay(150);
  }

  //---------------- MODE ----------------

  if(lastMode==HIGH && mode==LOW)
  {
    Serial.println(">>> MODE PRESSED");

    calibrationMode=false;

    currentUnit = (Unit)((currentUnit+1)%4);

    Serial.print("Current Unit = ");

    switch(currentUnit)
    {
      case GRAM:
        Serial.println("GRAM");
        break;

      case KILOGRAM:
        Serial.println("KG");
        break;

      case OUNCE:
        Serial.println("OZ");
        break;

      case POUND:
        Serial.println("LB");
        break;
    }

    delay(150);
  }

  //---------------- CAL ----------------

  if(lastCal==HIGH && cal==LOW)
  {
    Serial.println(">>> CAL PRESSED");

    calibrationMode=!calibrationMode;

    delay(150);
  }

  lastTare=tare;
  lastMode=mode;
  lastCal=cal;

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  if(calibrationMode)
  {
    display.setTextSize(2);
    display.setCursor(0,12);
    display.println("Calibration");

    display.setCursor(25,40);
    display.println("Mode");

    display.display();
    return;
  }

  float weightKg = baseWeight - tareOffset;

  float weight = weightKg;

  String unit;

  switch(currentUnit)
  {
    case GRAM:
      weight = weightKg * 1000.0;
      unit = "g";
      break;

    case KILOGRAM:
      weight = weightKg;
      unit = "kg";
      break;

    case OUNCE:
      weight = weightKg * 35.27396;
      unit = "oz";
      break;

    case POUND:
      weight = weightKg * 2.20462;
      unit = "lb";
      break;
  }

  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Weight");

  display.drawLine(0,10,127,10,SH110X_WHITE);

  display.setTextSize(3);
  display.setCursor(0,20);
  display.print(weight,2);

  display.setTextSize(2);
  display.setCursor(90,46);
  display.print(unit);

  display.display();
}