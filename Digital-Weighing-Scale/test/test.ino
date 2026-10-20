#include <MD_MAX72xx.h>
#include <SPI.h>
#include <Keypad.h>
#include <ADS1232.h>
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 1

// MAX7219
#define DATA_PIN 23
#define CLK_PIN 18
#define CS_PIN 5

MD_MAX72XX mx(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

//========================
// Keypad
//========================
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] =
{
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {13,12,14,27};
byte colPins[COLS] = {26,25,33,32};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//========================
// Segment patterns
//========================
byte num[] =
{
  B01111110, //0
  B00110000, //1
  B01101101, //2
  B01111001, //3
  B00110011, //4
  B01011011, //5
  B01011111, //6
  B01110000, //7
  B01111111, //8
  B01111011  //9
};

//========================
// Display Buffer
//========================
char displayBuffer[8] =
{
  ' ',' ',' ',' ',' ',' ',' ',' '
};

void refreshDisplay()
{
  for (int i = 0; i < 8; i++)
  {
    if (displayBuffer[i] >= '0' && displayBuffer[i] <= '9')
      mx.setRow(0, 7 - i, num[displayBuffer[i] - '0']);
    else
      mx.setRow(0, 7 - i, 0x00);
  }

  mx.update();
}

void pushDigit(char d)
{
  for (int i = 0; i < 7; i++)
    displayBuffer[i] = displayBuffer[i + 1];

  displayBuffer[7] = d;

  refreshDisplay();
}

void clearDisplay()
{
  for (int i = 0; i < 8; i++)
    displayBuffer[i] = ' ';

  refreshDisplay();
}

void setup()
{
  Serial.begin(115200);

  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 8);

  clearDisplay();

  Serial.println("Ready");
}

void loop()
{
  char key = keypad.getKey();

  if (key)
  {
    Serial.print("Key: ");
    Serial.println(key);

    if (key >= '0' && key <= '9')
    {
      pushDigit(key);
    }
    else if (key == '*')
    {
      clearDisplay();
    }
    else if (key == '#')
    {
      Serial.print("Entered Number: ");

      for (int i = 0; i < 8; i++)
        Serial.print(displayBuffer[i]);

      Serial.println();
    }
  }
}