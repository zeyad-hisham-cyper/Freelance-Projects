/*
 * ADS1232.h
 * ------------------------------------------------------------------------
 * Low-level driver for the TI ADS1232 24-bit ADC (bit-banged protocol).
 *
 * DESIGN NOTE:
 * This class knows ONLY about the ADS1232's electrical protocol:
 *   - power up/down (PDWN)
 *   - data-ready detection and clocking out 24 bits (DOUT/SCLK)
 *   - speed select (SPEED pin)
 *   - channel select (A0 pin — this is a HARDWARE pin on the 2-channel
 *     module, unlike the HX711's clock-pulse-count gain trick)
 *
 * It deliberately does NOT know about calibration, tare, units, or
 * "weight." That logic belongs one layer up (ScaleReader in Phase 1,
 * and a future Calibration class in Phase 2). This separation is what
 * lets us swap ADCs, add filtering, or add calibration later without
 * touching this file.
 * ------------------------------------------------------------------------
 */

#ifndef ADS1232_H
#define ADS1232_H

#include <Arduino.h>

class ADS1232 {
public:
    enum class Speed : uint8_t {
        SPS_10 = 0,   // low noise, slower
        SPS_80 = 1    // faster, slightly noisier
    };

    enum class Channel : uint8_t {
        A = 0,
        B = 1
    };

    ADS1232(uint8_t doutPin, uint8_t sclkPin, uint8_t pdwnPin, uint8_t speedPin, uint8_t a0Pin);

    // Configure pin modes, power up the ADC, set default speed/channel.
    void begin(Speed speed = Speed::SPS_10, Channel channel = Channel::A);

    // True once DOUT has gone low, meaning a new conversion is ready.
    bool isReady() const;

    // Blocking read of one 24-bit conversion (sign-extended to int32_t).
    // Returns true on success, false on timeout waiting for data-ready.
    bool readRaw(int32_t &outValue, uint32_t timeoutMs);

    // Change ADC speed at runtime (requires a brief settling delay after).
    void setSpeed(Speed speed);

    // Select the physical channel on the 2-channel module.
    void setChannel(Channel channel);

    // Power down / power up the ADC (PDWN pin).
    void powerDown();
    void powerUp();

private:
    uint8_t _doutPin;
    uint8_t _sclkPin;
    uint8_t _pdwnPin;
    uint8_t _speedPin;
    uint8_t _a0Pin;

    // Clocks a single bit out of the ADC. Timing follows the ADS1232
    // datasheet minimums; a small delayMicroseconds() is used since the
    // ESP32 can toggle pins faster than the ADC's spec allows.
    int readBit();
};

#endif // ADS1232_H
