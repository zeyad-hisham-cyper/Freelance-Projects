/*
 * ScaleReader.h
 * ------------------------------------------------------------------------
 * Application-level wrapper around the ADS1232 driver.
 *
 * DESIGN NOTE (why this layer exists even though it's thin right now):
 * In Phase 1 this class just fetches and stores raw ADC counts. It looks
 * almost redundant with ADS1232 itself — but its public interface
 * (getRawReading / hasNewReading / update) is deliberately the shape that
 * Phase 2's calibration will need:
 *
 *   getRawReading()   -> stays as-is
 *   getWeight()        -> added in Phase 2, computed from raw + calibration
 *   tare()             -> added in Phase 2
 *   setCalibrationFactor() -> added in Phase 2
 *
 * Because application code (ScaleNode.ino) talks to ScaleReader and never
 * to ADS1232 directly, none of the .ino code will need to change when
 * Phase 2 lands — only this class grows.
 * ------------------------------------------------------------------------
 */

#ifndef SCALE_READER_H
#define SCALE_READER_H

#include "ADS1232.h"

class ScaleReader {
public:
    explicit ScaleReader(ADS1232 &adc);

    void begin();

    // Polls the ADC; if a new conversion is ready, reads it and updates
    // internal state. Call this frequently from loop().
    // Returns true if a new reading was captured this call.
    bool update();

    // Most recent raw ADC count (valid only after update() returns true
    // at least once).
    int32_t getRawReading() const;

private:
    ADS1232 &_adc;
    int32_t _lastRawReading;
    bool _hasReading;
};

#endif // SCALE_READER_H
