/*
 * ScaleReader.cpp
 * ------------------------------------------------------------------------
 * See ScaleReader.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "ScaleReader.h"
#include "Config.h"

ScaleReader::ScaleReader(ADS1232 &adc)
    : _adc(adc), _lastRawReading(0), _hasReading(false) {}

void ScaleReader::begin() {
    _adc.begin(ADS1232::Speed::SPS_10, ADS1232::Channel::A);
}

bool ScaleReader::update() {
    if (!_adc.isReady()) {
        return false;
    }

    int32_t raw = 0;
    if (_adc.readRaw(raw, ADS1232_TIMEOUT_MS)) {
        _lastRawReading = raw;
        _hasReading = true;
        return true;
    }

    return false;
}

int32_t ScaleReader::getRawReading() const {
    return _lastRawReading;
}
