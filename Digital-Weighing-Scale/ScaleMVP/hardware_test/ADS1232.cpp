/*
 * ADS1232.cpp
 * ------------------------------------------------------------------------
 * See ADS1232.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "ADS1232.h"

// Minimum clock high/low time per ADS1232 datasheet is small (~1us range),
// but the ESP32 GPIO toggling plus a tiny margin keeps this safely within
// spec without needing precise cycle counting.
static const uint16_t CLOCK_PULSE_US = 1;

ADS1232::ADS1232(uint8_t doutPin, uint8_t sclkPin, uint8_t pdwnPin, uint8_t speedPin, uint8_t a0Pin)
    : _doutPin(doutPin), _sclkPin(sclkPin), _pdwnPin(pdwnPin), _speedPin(speedPin), _a0Pin(a0Pin) {}

void ADS1232::begin(Speed speed, Channel channel) {
    pinMode(_doutPin, INPUT);
    pinMode(_sclkPin, OUTPUT);
    pinMode(_pdwnPin, OUTPUT);
    pinMode(_speedPin, OUTPUT);
    pinMode(_a0Pin, OUTPUT);

    digitalWrite(_sclkPin, LOW);

    powerUp();
    setSpeed(speed);
    setChannel(channel);

    // Allow the ADC to settle after power-up / configuration before the
    // first read is attempted. The datasheet recommends waiting for a
    // full conversion cycle; this covers the slower 10 SPS case.
    delay(100);
}

bool ADS1232::isReady() const {
    // DOUT idles HIGH and drops LOW when a conversion result is ready.
    return digitalRead(_doutPin) == LOW;
}

bool ADS1232::readRaw(int32_t &outValue, uint32_t timeoutMs) {
    uint32_t start = millis();

    // Wait for DOUT to go low (conversion ready), with a timeout so a
    // disconnected/misbehaving ADC can't hang the whole application.
    while (!isReady()) {
        if (millis() - start > timeoutMs) {
            return false;
        }
    }

    int32_t value = 0;

    // Enter critical section to prevent ESP32 FreeRTOS context switches.
    // If interrupted during the readout, the next conversion could finish
    // mid-read, causing DOUT/DRDY to toggle and corrupt the data.
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);

    // ADS1232 shifts out 24 bits, MSB first, on the falling edge of SCLK.
    for (int i = 0; i < 24; i++) {
        value = (value << 1) | readBit();
    }

    // 25th clock pulse: Section 7.3.12 of the ADS1232 datasheet specifies
    // that applying a 25th SCLK pulse forces DOUT/DRDY HIGH until the next
    // conversion is complete. Without this, DOUT remains LOW and isReady()
    // will falsely trigger immediately on the next loop iteration.
    digitalWrite(_sclkPin, HIGH);
    delayMicroseconds(CLOCK_PULSE_US);
    digitalWrite(_sclkPin, LOW);
    delayMicroseconds(CLOCK_PULSE_US);

    portEXIT_CRITICAL(&mux);

    // Result is 24-bit two's complement; sign-extend to 32-bit so
    // negative readings (e.g. below zero/tare) are handled correctly.
    if (value & 0x00800000) {
        value |= 0xFF000000;
    }

    outValue = value;
    return true;
}

int ADS1232::readBit() {
    digitalWrite(_sclkPin, HIGH);
    delayMicroseconds(CLOCK_PULSE_US);
    digitalWrite(_sclkPin, LOW);
    // Datasheet Section 7.3.12: Read data on the falling edge of SCLK.
    int bit = digitalRead(_doutPin);
    delayMicroseconds(CLOCK_PULSE_US);
    return bit;
}

void ADS1232::setSpeed(Speed speed) {
    digitalWrite(_speedPin, speed == Speed::SPS_80 ? HIGH : LOW);
}

void ADS1232::setChannel(Channel channel) {
    digitalWrite(_a0Pin, channel == Channel::B ? HIGH : LOW);
    // Switching channels requires letting the analog front-end settle
    // before the next reading is trustworthy.
    delay(10);
}

void ADS1232::powerDown() {
    digitalWrite(_pdwnPin, LOW);
}

void ADS1232::powerUp() {
    digitalWrite(_pdwnPin, LOW);
    delay(10);

    digitalWrite(_pdwnPin, HIGH);
    delay(200);
}
