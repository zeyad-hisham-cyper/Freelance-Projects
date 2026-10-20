/*
 * FilterManager.cpp
 * ------------------------------------------------------------------------
 * See FilterManager.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "FilterManager.h"

FilterManager::FilterManager() {
    reset();
}

void FilterManager::reset() {
#ifdef ENABLE_FILTER
    for (uint8_t i = 0; i < FILTER_WINDOW_SIZE; i++) {
        _samples[i] = 0;
    }
    _index = 0;
    _sum   = 0;
#endif
    _count   = 0;
    _average = 0.0f;

    for (uint8_t i = 0; i < STABILITY_WINDOW; i++) {
        _stabilityBuf[i] = 0.0f;
    }
    _stabilityIndex = 0;
    _stabilityCount = 0;
}

float FilterManager::push(int32_t rawSample) {
#ifdef ENABLE_FILTER
    // Circular buffer: subtract the sample being evicted, add the new one.
    // This keeps the cost constant regardless of window size.
    _sum -= _samples[_index];
    _samples[_index] = rawSample;
    _sum += rawSample;

    _index = (uint8_t)((_index + 1) % FILTER_WINDOW_SIZE);

    if (_count < FILTER_WINDOW_SIZE) {
        _count++;
    }

    // While filling up, divide by the number of samples actually present so
    // the very first readings are not artificially pulled toward zero.
    _average = (float)_sum / (float)_count;
#else
    _average = (float)rawSample;
    if (_count < FILTER_WINDOW_SIZE) _count++;
#endif

    _stabilityBuf[_stabilityIndex] = _average;
    _stabilityIndex = (uint8_t)((_stabilityIndex + 1) % STABILITY_WINDOW);
    if (_stabilityCount < STABILITY_WINDOW) {
        _stabilityCount++;
    }

    return _average;
}

bool FilterManager::isStable() const {
    // Not enough history yet — report "moving" rather than guessing stable,
    // so nothing downstream captures a value it should not trust.
    if (_stabilityCount < STABILITY_WINDOW) {
        return false;
    }

    float minVal = _stabilityBuf[0];
    float maxVal = _stabilityBuf[0];

    for (uint8_t i = 1; i < STABILITY_WINDOW; i++) {
        if (_stabilityBuf[i] < minVal) minVal = _stabilityBuf[i];
        if (_stabilityBuf[i] > maxVal) maxVal = _stabilityBuf[i];
    }

    return (maxVal - minVal) < (float)STABILITY_THRESHOLD_COUNTS;
}
