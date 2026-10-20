/*
 * WeightManager.h
 * ------------------------------------------------------------------------
 * Owns the ADCs and produces ONE combined, filtered raw value.
 *
 * Responsibilities (per system.md):
 *   - read every load cell's ADC independently
 *   - filter each channel
 *   - average the channels into a single signal
 *   - report raw counts, stability, and hardware faults
 *
 * It deliberately stops at RAW COUNTS. Converting counts to grams is
 * CalibrationManager's job, and converting grams to display units is
 * UnitManager's. Keeping those separate is what allows a load cell to be
 * swapped with nothing but a recalibration.
 *
 * The update() call is non-blocking: it polls each ADC's data-ready line
 * and only clocks out bits when a conversion is actually waiting, so the
 * main loop stays responsive to button presses at 10 SPS.
 * ------------------------------------------------------------------------
 */

#ifndef WEIGHT_MANAGER_H
#define WEIGHT_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"
#include "FilterManager.h"

#ifdef USE_ADS1232
    #include "../drivers/ADS1232.h"
#endif

#ifdef USE_HX711
    #include "../drivers/HX711.h"
#endif

class WeightManager {
public:
    WeightManager();

    // Configures the ADCs from pins.h and powers them up.
    bool begin();

    // Polls every ADC; pushes any ready conversion into its filter.
    // Returns true if at least one new sample was collected this call.
    bool update();

    // Combined filtered value, in raw ADC counts.
    float rawCounts() const { return _combined; }

    // True once every channel's filter window has filled.
    bool hasValidReading() const;

    // True when every channel reports a settled reading.
    bool isStable() const;

    // Blocking helper used by the calibration procedure: averages `samples`
    // fresh conversions from every cell and returns the combined mean.
    // Returns false if any cell fails to deliver in time.
    bool captureAverage(float &outCounts, uint16_t samples, uint32_t timeoutMs);

    // Clears filter history — use after calibration or a channel change.
    void resetFilters();

    // ---- Diagnostics -----------------------------------------------
    uint32_t timeoutCount() const { return _timeoutCount; }
    uint32_t msSinceLastSample() const { return millis() - _lastSampleMs; }
    bool     isFaulty() const;

private:
    // One ADC per load cell. The two drivers expose the same
    // setPins/begin/isReady/readRaw surface, so everything below this
    // declaration is identical regardless of which one is selected.
#ifdef USE_ADS1232
    ADS1232 _adc[LOAD_CELL_COUNT];
#endif
#ifdef USE_HX711
    HX711 _adc[LOAD_CELL_COUNT];
#endif

    FilterManager _filter[LOAD_CELL_COUNT];

    float    _combined;
    uint32_t _lastSampleMs;
    uint32_t _timeoutCount;

    // Reads one conversion from cell `index`, waiting up to timeoutMs.
    bool readCell(uint8_t index, int32_t &raw, uint32_t timeoutMs);

    // Recomputes _combined as the mean of the per-cell filtered values.
    void recombine();
};

#endif // WEIGHT_MANAGER_H
