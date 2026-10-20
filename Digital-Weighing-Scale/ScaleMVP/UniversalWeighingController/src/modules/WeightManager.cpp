/*
 * WeightManager.cpp
 * ------------------------------------------------------------------------
 * See WeightManager.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "WeightManager.h"

WeightManager::WeightManager()
    : _combined(0.0f), _lastSampleMs(0), _timeoutCount(0) {}

bool WeightManager::begin() {
#ifdef USE_ADS1232
    for (uint8_t i = 0; i < LOAD_CELL_COUNT; i++) {
        _adc[i].setPins(PIN_ADC_DOUT[i], PIN_ADC_SCLK[i], PIN_ADC_PDWN[i],
                        PIN_ADC_SPEED[i], PIN_ADC_A0[i]);

        _adc[i].begin(ADC_SPEED_80SPS ? ADS1232::Speed::SPS_80
                                      : ADS1232::Speed::SPS_10,
                      ADS1232::Channel::A);
    }
#endif

#ifdef USE_HX711
    for (uint8_t i = 0; i < LOAD_CELL_COUNT; i++) {
        _adc[i].setPins(PIN_ADC_DOUT[i], PIN_ADC_SCLK[i]);

        // Channel A at gain 128 is the load-cell input. Note that the HX711
        // sample rate is NOT software selectable — it is fixed by the RATE
        // pin on the module — so ADC_SPEED_80SPS has no effect here.
        _adc[i].begin((HX711::Gain)HX711_GAIN_SETTING);
    }
#endif

    resetFilters();
    _lastSampleMs = millis();
    _timeoutCount = 0;

    // The very first conversion after power-up is discarded: the ADS1232
    // needs a full settling period and that reading is not trustworthy.
    for (uint8_t i = 0; i < LOAD_CELL_COUNT; i++) {
        int32_t discard = 0;
        readCell(i, discard, ADC_READ_TIMEOUT_MS);
    }

    return true;
}

bool WeightManager::readCell(uint8_t index, int32_t &raw, uint32_t timeoutMs) {
    // Both drivers share this signature, so no per-ADC branch is needed.
    // config.h guarantees exactly one of them is compiled in.
    return _adc[index].readRaw(raw, timeoutMs);
}

bool WeightManager::update() {
    bool gotSample = false;

    for (uint8_t i = 0; i < LOAD_CELL_COUNT; i++) {
        // Poll rather than wait: only touch the bus when the ADC has
        // actually finished a conversion, so a slow 10 SPS ADC never
        // stalls the button handling in the main loop.
        if (!_adc[i].isReady()) {
            continue;
        }

        int32_t raw = 0;

        // Data-ready is already asserted, so this returns almost instantly;
        // the short timeout only guards against the line dropping mid-read.
        if (readCell(i, raw, 50)) {
            _filter[i].push(raw);
            gotSample = true;
        } else {
            _timeoutCount++;
        }
    }

    if (gotSample) {
        _lastSampleMs = millis();
        recombine();
    }

    return gotSample;
}

void WeightManager::recombine() {
    // Averaging (rather than summing) the channels keeps the calibration
    // factor independent of how many cells are fitted: adding cells to a
    // platform then only requires a recalibration, not a config change.
    float sum = 0.0f;
    for (uint8_t i = 0; i < LOAD_CELL_COUNT; i++) {
        sum += _filter[i].value();
    }
    _combined = sum / (float)LOAD_CELL_COUNT;
}

bool WeightManager::hasValidReading() const {
    for (uint8_t i = 0; i < LOAD_CELL_COUNT; i++) {
        if (!_filter[i].isPrimed()) return false;
    }
    return true;
}

bool WeightManager::isStable() const {
    for (uint8_t i = 0; i < LOAD_CELL_COUNT; i++) {
        if (!_filter[i].isStable()) return false;
    }
    return true;
}

bool WeightManager::isFaulty() const {
    // No conversion for several times the expected interval means the ADC
    // is unpowered, unwired, or dead.
    return msSinceLastSample() > (uint32_t)(ADC_READ_TIMEOUT_MS * 3);
}

void WeightManager::resetFilters() {
    for (uint8_t i = 0; i < LOAD_CELL_COUNT; i++) {
        _filter[i].reset();
    }
    _combined = 0.0f;
}

bool WeightManager::captureAverage(float &outCounts, uint16_t samples, uint32_t timeoutMs) {
    // Blocking on purpose: calibration is a deliberate, operator-driven
    // pause in normal operation, and a clean average matters more than
    // loop responsiveness for those two seconds.
    double cellSum[LOAD_CELL_COUNT];
    uint16_t cellCount[LOAD_CELL_COUNT];

    for (uint8_t i = 0; i < LOAD_CELL_COUNT; i++) {
        cellSum[i]   = 0.0;
        cellCount[i] = 0;
    }

    uint32_t start = millis();

    bool done = false;
    while (!done) {
        if (millis() - start > timeoutMs) {
            return false;
        }

        done = true;
        for (uint8_t i = 0; i < LOAD_CELL_COUNT; i++) {
            if (cellCount[i] >= samples) {
                continue;
            }
            done = false;

            int32_t raw = 0;
            if (readCell(i, raw, ADC_READ_TIMEOUT_MS)) {
                cellSum[i] += (double)raw;
                cellCount[i]++;
            } else {
                _timeoutCount++;
                return false;
            }
        }
    }

    double combined = 0.0;
    for (uint8_t i = 0; i < LOAD_CELL_COUNT; i++) {
        combined += cellSum[i] / (double)cellCount[i];
    }

    outCounts     = (float)(combined / (double)LOAD_CELL_COUNT);
    _lastSampleMs = millis();
    return true;
}
