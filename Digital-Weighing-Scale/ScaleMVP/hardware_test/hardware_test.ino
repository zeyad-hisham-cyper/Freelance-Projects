#include "ADS1232.h"

#define DOUT_PIN   34
#define SCLK_PIN   25
#define PDWN_PIN   26
#define SPEED_PIN  27
#define A0_PIN     14

ADS1232 adc(DOUT_PIN, SCLK_PIN, PDWN_PIN, SPEED_PIN, A0_PIN);

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println("ADS1232 Hardware Test");

    adc.begin(ADS1232::Speed::SPS_10, ADS1232::Channel::A);

    Serial.println("Init Done");
}

void loop()
{
    Serial.print("DOUT = ");
    Serial.print(digitalRead(DOUT_PIN));

    int32_t raw = 0;

    if (adc.readRaw(raw, 1000))
    {
        Serial.print("   RAW = ");
        Serial.println(raw);
    }
    else
    {
        Serial.println("   TIMEOUT");
    }

    delay(500);
}