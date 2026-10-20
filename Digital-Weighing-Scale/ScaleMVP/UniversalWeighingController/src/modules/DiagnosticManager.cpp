/*
 * DiagnosticManager.cpp
 * ------------------------------------------------------------------------
 * See DiagnosticManager.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "DiagnosticManager.h"

DiagnosticManager::DiagnosticManager()
    : _weight(nullptr), _calibration(nullptr), _wireless(nullptr),
      _fault(SystemFault::None) {}

void DiagnosticManager::begin(WeightManager *weight, CalibrationManager *calibration,
                              WirelessManager *wireless) {
    _weight      = weight;
    _calibration = calibration;
    _wireless    = wireless;
    _fault       = SystemFault::None;
}

void DiagnosticManager::update() {
#ifdef ENABLE_DIAGNOSTICS
    // Faults are ordered by severity: a dead ADC makes every other check
    // meaningless, so it is tested first.
    if (_weight && _weight->isFaulty()) {
        _fault = SystemFault::AdcNotResponding;
        return;
    }

    // Overload is only meaningful once the scale actually knows what a gram
    // is; on an uncalibrated board the raw counts would trip it constantly.
    if (_weight && _calibration && _calibration->isCalibrated()) {
        float grossG = _calibration->grossGrams(_weight->rawCounts());
        if (fabsf(grossG) > SCALE_CAPACITY_G * OVERLOAD_MARGIN) {
            _fault = SystemFault::Overload;
            return;
        }
    }

#ifdef ENABLE_NRF
    // A radio that does not answer on SPI is a wiring or supply fault on
    // this board. A radio that works but reaches no dashboard is NOT a
    // fault — see the note in the header.
    if (_wireless && !_wireless->hasRadio()) {
        _fault = SystemFault::RadioNotResponding;
        return;
    }
#endif

    _fault = SystemFault::None;
#else
    _fault = SystemFault::None;
#endif
}

const char *DiagnosticManager::faultTitle() const {
    switch (_fault) {
        case SystemFault::AdcNotResponding:   return "ADC FAIL";
        case SystemFault::Overload:           return "OVERLOAD";
        case SystemFault::RadioNotResponding: return "RADIO";
        default:                              return "";
    }
}

const char *DiagnosticManager::faultDetail() const {
    switch (_fault) {
        case SystemFault::AdcNotResponding:   return "Check load cell wiring";
        case SystemFault::Overload:           return "Remove excess weight";
        case SystemFault::RadioNotResponding: return "nRF24 not responding";
        default:                              return "";
    }
}
