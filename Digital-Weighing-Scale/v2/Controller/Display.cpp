#include "Display.h"
#include "Config.h"
#include <SPI.h>

// 7-Segment digit patterns — bit order matches MD_MAX72XX FC16 hardware
static const uint8_t num[] =
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

// Initialize using Hardware SPI to prevent Wi-Fi interrupt timing conflicts
Display::Display() : mx(HARDWARE_TYPE, CS_PIN, MAX_DEVICES)
{
}

void Display::begin()
{
    mx.begin();
    mx.control(MD_MAX72XX::INTENSITY, DISPLAY_INTENSITY);
    mx.clear();
}

void Display::clear()
{
    mx.clear();
}

void Display::showWeight(float weight)
{
    if (weight < 0.0f)
        weight = 0.0f;

    // Cap at a value that fits in 8 digits (999.99999f loses precision and can round to 1000.0f)
    uint32_t scaled = (uint32_t)(weight * 100000.0f + 0.5f);
    if (scaled > 99999999) 
        scaled = 99999999;

    // Increased buffer size to prevent any possibility of stack corruption
    char str[16];
    sprintf(str, "%08lu", (unsigned long)scaled);

    mx.clear();

    for (int i = 0; i < 8; i++)
    {
        uint8_t pattern = num[str[i] - '0'];

        // Decimal point after the third digit (DDD.DDDDD → index 2)
        if (i == 2)
            pattern |= 0x80;

        // Use the global setRow (row, value) to apply to our single device
        mx.setRow(i, pattern);
    }

    mx.update();
}
