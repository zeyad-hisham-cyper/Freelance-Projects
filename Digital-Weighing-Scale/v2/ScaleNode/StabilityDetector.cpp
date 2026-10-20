#include "StabilityDetector.h"

StabilityDetector::StabilityDetector() {
    reset();
}

void StabilityDetector::reset() {
    _head = 0;
    _count = 0;
    _isStable = false;
    for (int i = 0; i < STABILITY_WINDOW_SIZE; i++) {
        _buffer[i] = 0;
    }
}

void StabilityDetector::addSample(int32_t rawValue) {
    _buffer[_head] = rawValue;
    _head = (_head + 1) % STABILITY_WINDOW_SIZE;
    if (_count < STABILITY_WINDOW_SIZE) {
        _count++;
    }

    if (_count < STABILITY_WINDOW_SIZE) {
        // Not enough samples yet to determine stability
        _isStable = false;
        return;
    }

    int32_t minVal = _buffer[0];
    int32_t maxVal = _buffer[0];

    for (int i = 1; i < STABILITY_WINDOW_SIZE; i++) {
        if (_buffer[i] < minVal) minVal = _buffer[i];
        if (_buffer[i] > maxVal) maxVal = _buffer[i];
    }

    _isStable = (maxVal - minVal) <= STABILITY_THRESHOLD_RAW;
}

bool StabilityDetector::isStable() const {
    return _isStable;
}
