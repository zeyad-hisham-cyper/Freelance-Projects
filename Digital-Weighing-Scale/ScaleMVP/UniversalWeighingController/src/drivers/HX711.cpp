/*
 * HX711.cpp
 * ------------------------------------------------------------------------
 * See HX711.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "HX711.h"

// Datasheet Table "Timing": PD_SCK high and low times are each 0.2 us min
// and 50 us MAX. The upper bound is the important one — exceeding 60 us
// with PD_SCK high triggers a power-down. 1 us sits comfortably between.
static const uint16_t CLOCK_PULSE_US = 1;

// Holding PD_SCK high for more than 60 us puts the chip to sleep.
static const uint16_t POWER_DOWN_US = 70;

HX711::HX711()
    : _doutPin(255), _sckPin(255), _gain(Gain::A_128) {}

HX711::HX711(uint8_t doutPin, uint8_t sckPin)
    : _doutPin(doutPin), _sckPin(sckPin), _gain(Gain::A_128) {}

void HX711::setPins(uint8_t doutPin, uint8_t sckPin) {
    _doutPin = doutPin;
    _sckPin  = sckPin;
}

void HX711::begin(Gain gain) {
    pinMode(_doutPin, INPUT);
    pinMode(_sckPin, OUTPUT);

    _gain = gain;

    powerUp();

    // The gain setting is only transmitted as the trailing pulses of a
    // conversion, so it is not actually applied until one read has been
    // performed. Do that here and throw the result away, otherwise the
    // first real measurement would come back at the previous gain.
    int32_t discard = 0;
    readRaw(discard, 500);
}

bool HX711::isReady() const {
    // DOUT idles HIGH and drops LOW when a conversion result is ready.
    return digitalRead(_doutPin) == LOW;
}

bool HX711::readRaw(int32_t &outValue, uint32_t timeoutMs) {
    uint32_t start = millis();

    // Wait for data-ready, with a timeout so a disconnected or unpowered
    // module cannot hang the whole application.
    while (!isReady()) {
        if (millis() - start > timeoutMs) {
            return false;
        }
    }

    int32_t value = 0;

    // Critical section is MANDATORY here, not just for data integrity: a
    // FreeRTOS context switch while PD_SCK is HIGH would hold the line past
    // the 60 us power-down threshold, sleeping the chip mid-conversion and
    // resetting it to channel A / gain 128.
    //
    // Worst case this blocks for ~27 pulses x 2 us = ~54 us.
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);

    // 24 bits, MSB first.
    for (int i = 0; i < 24; i++) {
        value = (value << 1) | readBit();
    }

    // The extra pulses select the channel and gain for the NEXT conversion.
    // They are not optional: without them DOUT stays low and the chip never
    // starts another conversion.
    for (uint8_t i = 0; i < (uint8_t)_gain; i++) {
        digitalWrite(_sckPin, HIGH);
        delayMicroseconds(CLOCK_PULSE_US);
        digitalWrite(_sckPin, LOW);
        delayMicroseconds(CLOCK_PULSE_US);
    }

    portEXIT_CRITICAL(&mux);

    // Result is 24-bit two's complement; sign-extend to 32-bit so readings
    // below zero (e.g. under tare) are handled correctly.
    if (value & 0x00800000) {
        value |= 0xFF000000;
    }

    outValue = value;
    return true;
}

int HX711::readBit() {
    digitalWrite(_sckPin, HIGH);
    delayMicroseconds(CLOCK_PULSE_US);

    // Datasheet Figure "Data output timing": the rising edge shifts the bit
    // onto DOUT, so it is sampled while PD_SCK is still HIGH. (This differs
    // from the ADS1232, which is read on the falling edge.)
    int bit = digitalRead(_doutPin);

    digitalWrite(_sckPin, LOW);
    delayMicroseconds(CLOCK_PULSE_US);
    return bit;
}

void HX711::setGain(Gain gain) {
    _gain = gain;

    // Push the new setting out immediately so the caller does not have to
    // know about the one-conversion delay.
    int32_t discard = 0;
    readRaw(discard, 500);
}

void HX711::powerDown() {
    digitalWrite(_sckPin, LOW);
    delayMicroseconds(CLOCK_PULSE_US);
    digitalWrite(_sckPin, HIGH);
    delayMicroseconds(POWER_DOWN_US);
}

void HX711::powerUp() {
    // A low PD_SCK releases the power-down state; the chip then resets to
    // channel A / gain 128 and begins converting.
    digitalWrite(_sckPin, LOW);

    // Analog settling after power-up. At the default 10 SPS the datasheet
    // allows up to 400 ms before the output is valid.
    delay(400);
}
