/*
 * FilterManager.h
 * ------------------------------------------------------------------------
 * Digital filtering and stability detection for one signal channel.
 *
 * A raw 24-bit load-cell reading jitters by hundreds of counts even with
 * nothing on the platform. This class turns that into a usable number:
 *
 *   1. a moving average over FILTER_WINDOW_SIZE samples (noise reduction)
 *   2. a peak-to-peak check over the last STABILITY_WINDOW averaged values
 *      (stability detection — "has the reading settled?")
 *
 * One instance per load cell, plus one for the combined signal.
 *
 * DESIGN NOTE:
 * Stability is measured in raw ADC COUNTS, not grams, on purpose: it must
 * work before the scale has ever been calibrated, because the calibration
 * procedure itself waits for a stable reading. A filter that depended on
 * the calibration factor could not do that.
 * ------------------------------------------------------------------------
 */

#ifndef FILTER_MANAGER_H
#define FILTER_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"

class FilterManager {
public:
    FilterManager();

    // Discards all history. Call after a large deliberate step change
    // (calibration, channel switch) so old samples do not drag the average.
    void reset();

    // Feeds one raw sample; returns the new filtered value.
    float push(int32_t rawSample);

    float value() const { return _average; }

    // True once the window is full — before that the average is computed
    // over fewer samples and is not yet trustworthy.
    bool isPrimed() const { return _count >= FILTER_WINDOW_SIZE; }

    // True when the last STABILITY_WINDOW filtered values span less than
    // STABILITY_THRESHOLD_COUNTS.
    bool isStable() const;

private:
#ifdef ENABLE_FILTER
    int32_t _samples[FILTER_WINDOW_SIZE];
    uint8_t _index;
    int64_t _sum;              // 64-bit: 24-bit samples x window never overflows
#endif
    uint16_t _count;
    float    _average;

    float   _stabilityBuf[STABILITY_WINDOW];
    uint8_t _stabilityIndex;
    uint8_t _stabilityCount;
};

#endif // FILTER_MANAGER_H
