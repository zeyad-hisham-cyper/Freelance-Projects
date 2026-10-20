/*
 * HX711.h
 * ------------------------------------------------------------------------
 * Low-level driver for the Avia Semiconductor HX711 24-bit ADC
 * (bit-banged protocol).
 *
 * Deliberately mirrors the ADS1232 driver's interface — setPins / begin /
 * isReady / readRaw / powerUp / powerDown — so WeightManager can switch
 * between them with nothing but a #ifdef on the member declaration.
 *
 * DIFFERENCES FROM THE ADS1232 THAT MATTER:
 *
 *  1. Only TWO wires. There is no PDWN, SPEED or A0 pin. Everything is
 *     done over DOUT + PD_SCK:
 *       - power down  = hold PD_SCK HIGH for >60 us
 *       - sample rate = a hardware RATE pin on the module (10/80 SPS),
 *                       NOT software selectable
 *       - channel/gain = the NUMBER of extra clock pulses after the 24
 *                        data bits (25/26/27), which is why setGain() only
 *                        takes effect on the *following* conversion
 *
 *  2. The 60 us power-down rule makes the critical section mandatory, not
 *     merely a data-integrity nicety: if FreeRTOS preempts this task while
 *     PD_SCK is HIGH, the chip powers itself down mid-read and comes back
 *     reset to channel A / gain 128.
 *
 * As with the ADS1232, this class knows nothing about calibration, tare,
 * units or "weight" — that lives one layer up.
 * ------------------------------------------------------------------------
 */

#ifndef HX711_H
#define HX711_H

#include <Arduino.h>

class HX711 {
public:
    // The gain/channel selection, expressed as the number of extra PD_SCK
    // pulses the datasheet assigns to it.
    enum class Gain : uint8_t {
        A_128 = 1,   // channel A, gain 128 — the load-cell input
        B_32  = 2,   // channel B, gain 32  — auxiliary input
        A_64  = 3    // channel A, gain 64
    };

    HX711();
    HX711(uint8_t doutPin, uint8_t sckPin);

    void setPins(uint8_t doutPin, uint8_t sckPin);

    // Configures pin modes, powers the chip up and primes the gain setting.
    void begin(Gain gain = Gain::A_128);

    // True once DOUT has gone low, meaning a new conversion is ready.
    bool isReady() const;

    // Blocking read of one 24-bit conversion (sign-extended to int32_t).
    // Returns true on success, false on timeout waiting for data-ready.
    bool readRaw(int32_t &outValue, uint32_t timeoutMs);

    // Selects channel/gain. Takes effect on the NEXT conversion, because
    // the setting is transmitted as trailing pulses of the current one.
    void setGain(Gain gain);

    void powerDown();
    void powerUp();

private:
    uint8_t _doutPin;
    uint8_t _sckPin;
    Gain    _gain;

    // One clock pulse: rising edge shifts the next bit onto DOUT, which is
    // sampled while PD_SCK is still HIGH.
    int readBit();
};

#endif // HX711_H
