/*
 * CalibrationManager.cpp
 * ------------------------------------------------------------------------
 * See CalibrationManager.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "CalibrationManager.h"

CalibrationManager::CalibrationManager()
    : _weight(nullptr), _storage(nullptr), _tareOffsetG(0.0f),
      _state(CalState::Idle), _result(CalResult::None), _resultShownAt(0),
      _knownWeightG(DEFAULT_KNOWN_WEIGHT_G), _zeroCapture(0.0f),
      _zeroCaptured(false), _entryLen(0) {
    _entry[0] = '\0';
    StorageManager::applyDefaults(_record);
}

void CalibrationManager::begin(WeightManager *weight, StorageManager *storage) {
    _weight  = weight;
    _storage = storage;

    if (_storage && _storage->load(_record)) {
        LOG("[Cal] loaded: zero=");
        LOG(_record.zeroOffsetCounts);
        LOG(" cpg=");
        LOGLN(_record.countsPerGram);
    } else {
        StorageManager::applyDefaults(_record);
        LOGLN("[Cal] using defaults (not calibrated)");
    }

    _knownWeightG = _record.knownWeightG;
    _tareOffsetG  = 0.0f;   // tare is deliberately NOT persisted
}

// ---------------------------------------------------------------- chain

float CalibrationManager::grossGrams(float rawCounts) const {
    // Guard against a zero scale factor: without this the display would
    // read inf/nan and every downstream comparison would misbehave.
    float cpg = _record.countsPerGram;
    if (cpg == 0.0f) {
        cpg = DEFAULT_COUNTS_PER_GRAM;
    }
    return (rawCounts - _record.zeroOffsetCounts) / cpg;
}

float CalibrationManager::netGrams(float rawCounts) const {
    return grossGrams(rawCounts) - _tareOffsetG;
}

// ----------------------------------------------------------------- tare

void CalibrationManager::setTare(float grossValueGrams) {
    _tareOffsetG = grossValueGrams;
    LOG("[Cal] tare offset = ");
    LOGLN(_tareOffsetG);
}

void CalibrationManager::clearTare() {
    _tareOffsetG = 0.0f;
    LOGLN("[Cal] tare cleared");
}

// -------------------------------------------------------------- settings

void CalibrationManager::saveUnit(uint8_t unit) {
    if (_record.unit == unit) {
        return;   // nothing changed, do not touch flash
    }
    _record.unit = unit;
    if (_storage) {
        _storage->save(_record);
    }
}

// ------------------------------------------------------------- procedure

void CalibrationManager::start() {
    _state        = CalState::CaptureZero;
    _result       = CalResult::None;
    _zeroCaptured = false;

    // The field starts EMPTY rather than pre-filled with the last reference
    // mass. Pre-filling looks helpful and is not: the operator types their
    // digits onto the end of a number they did not notice was there, and
    // gets a calibration off by a factor of ten. The previous value is
    // offered as a prompt instead — see lastKnownWeight().
    clearEntry();

    LOGLN("[Cal] procedure started");
}

void CalibrationManager::abort() {
    if (_state == CalState::Idle) {
        return;
    }
    _state  = CalState::Idle;
    _result = CalResult::None;
    LOGLN("[Cal] aborted");
}

uint8_t CalibrationManager::stepNumber() const {
    switch (_state) {
        case CalState::CaptureZero: return 1;
        case CalState::PlaceWeight: return 2;
        case CalState::EnterWeight: return 3;
        case CalState::Result:      return 3;
        default:                    return 0;
    }
}

// ------------------------------------------------------- reference mass

void CalibrationManager::clearEntry() {
    _entry[0] = '\0';
    _entryLen = 0;
    syncEntryValue();
}

void CalibrationManager::syncEntryValue() {
    // An empty field, or one holding just ".", is zero — which entryValid()
    // then rejects. That is the intent: nothing typed is not a valid mass.
    _knownWeightG = (_entryLen > 0) ? (float)atof(_entry) : 0.0f;
}

void CalibrationManager::appendDigit(uint8_t digit) {
    if (_state != CalState::EnterWeight || digit > 9) {
        return;
    }
    if (_entryLen >= ENTRY_MAX) {
        return;
    }

    // A leading zero is replaced rather than appended, so tapping 0 then 5
    // gives "5" and not "05". A zero before a decimal point is real, though,
    // and has to survive: "0.5".
    if (_entryLen == 1 && _entry[0] == '0' && digit != 0) {
        _entry[0] = (char)('0' + digit);
        syncEntryValue();
        return;
    }
    if (_entryLen == 1 && _entry[0] == '0' && digit == 0) {
        return;   // "00" is not a thing
    }

    _entry[_entryLen++] = (char)('0' + digit);
    _entry[_entryLen]   = '\0';
    syncEntryValue();
}

void CalibrationManager::appendPoint() {
    if (_state != CalState::EnterWeight) {
        return;
    }
    if (_entryLen >= ENTRY_MAX) {
        return;
    }
    if (strchr(_entry, '.') != nullptr) {
        return;   // one point per number
    }

    // A point typed first means "0." — the leading zero the operator did not
    // bother with.
    if (_entryLen == 0) {
        _entry[_entryLen++] = '0';
    }

    _entry[_entryLen++] = '.';
    _entry[_entryLen]   = '\0';
    syncEntryValue();
}

void CalibrationManager::backspace() {
    if (_state != CalState::EnterWeight || _entryLen == 0) {
        return;
    }
    _entry[--_entryLen] = '\0';
    syncEntryValue();
}

const char *CalibrationManager::entryText() const {
    return (_entryLen > 0) ? _entry : "0";
}

bool CalibrationManager::entryValid() const {
    if (_entryLen == 0) {
        return false;
    }
    return _knownWeightG >= CAL_WEIGHT_MIN_G && _knownWeightG <= CAL_WEIGHT_MAX_G;
}

void CalibrationManager::next() {
    switch (_state) {
        case CalState::Idle:
            start();
            break;

        case CalState::CaptureZero: {
            // Step 1: platform empty -> this reading defines "zero".
            float counts = 0.0f;
            if (!_weight ||
                !_weight->captureAverage(counts, CAL_AVERAGE_SAMPLES, CAL_CAPTURE_TIMEOUT_MS)) {
                _result        = CalResult::FailedAdc;
                _state         = CalState::Result;
                _resultShownAt = millis();
                LOGLN("[Cal] zero capture failed");
                return;
            }

            _zeroCapture  = counts;
            _zeroCaptured = true;
            _state        = CalState::PlaceWeight;

            LOG("[Cal] zero counts = ");
            LOGLN(_zeroCapture);
            break;
        }

        case CalState::PlaceWeight:
            // Nothing to capture yet — the operator is still loading the
            // reference mass. Capturing happens once its value is confirmed.
            _state = CalState::EnterWeight;
            break;

        case CalState::EnterWeight:
            // Checked before the capture, not after: an empty or absurd
            // reference mass would divide the span by nonsense and store a
            // scale factor that looks plausible on the display and is wrong
            // by orders of magnitude.
            if (!entryValid()) {
                _result        = CalResult::FailedValue;
                _state         = CalState::Result;
                _resultShownAt = millis();
                LOG("[Cal] reference mass rejected: ");
                LOGLN(entryText());
                return;
            }
            finish();
            break;

        case CalState::Result:
            _state  = CalState::Idle;
            _result = CalResult::None;
            break;
    }
}

void CalibrationManager::finish() {
    _resultShownAt = millis();
    _state         = CalState::Result;

    float loadedCounts = 0.0f;
    if (!_weight ||
        !_weight->captureAverage(loadedCounts, CAL_AVERAGE_SAMPLES, CAL_CAPTURE_TIMEOUT_MS)) {
        _result = CalResult::FailedAdc;
        LOGLN("[Cal] loaded capture failed");
        return;
    }

    float span = loadedCounts - _zeroCapture;

    // If the span is tiny the reference mass was never actually placed (or
    // the cell is not responding). Accepting it would produce an enormous
    // scale factor and a wildly wrong scale, so reject instead.
    if (fabsf(span) < (float)CAL_MIN_SPAN_COUNTS) {
        _result = CalResult::FailedSpan;
        LOG("[Cal] span too small: ");
        LOGLN(span);
        return;
    }

    _record.zeroOffsetCounts = _zeroCapture;
    _record.countsPerGram    = span / _knownWeightG;
    _record.knownWeightG     = _knownWeightG;
    _record.calibrated       = 1;

    // A fresh calibration invalidates any container weight taken against
    // the old scale factor.
    _tareOffsetG = 0.0f;

    if (_weight) {
        _weight->resetFilters();
    }

    if (_storage) {
        _storage->save(_record);
    }

    _result = CalResult::Saved;

    LOG("[Cal] done. counts/g = ");
    LOGLN(_record.countsPerGram);
}

void CalibrationManager::update() {
    // The result screen dismisses itself so the operator is never stranded
    // in calibration mode after walking away.
    if (_state == CalState::Result &&
        (millis() - _resultShownAt) > CAL_RESULT_DISPLAY_MS) {
        _state  = CalState::Idle;
        _result = CalResult::None;
    }
}
