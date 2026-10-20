/*
 * DiagnosticManager.cpp
 * ------------------------------------------------------------------------
 * See DiagnosticManager.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "DiagnosticManager.h"

DiagnosticManager::DiagnosticManager()
    : _wireless(nullptr), _fault(SystemFault::None) {}

void DiagnosticManager::begin(WirelessManager *wireless) {
    _wireless = wireless;
    _fault    = SystemFault::None;
}

void DiagnosticManager::update() {
#ifdef ENABLE_DIAGNOSTICS
    if (!_wireless) {
        _fault = SystemFault::None;
        return;
    }

    // A radio that does not answer on SPI is a fault on this board, and it
    // makes every other question unanswerable.
    if (!_wireless->hasRadio()) {
        _fault = SystemFault::RadioNotResponding;
        return;
    }

    // Nothing recent to show. This deliberately covers both "never heard
    // from the scale" and "heard from it, but not lately": from the
    // operator's point of view they are the same problem, and the second is
    // the more dangerous one to paper over.
    if (!_wireless->isLinkUp()) {
        _fault = SystemFault::NoSignal;
        return;
    }

    // The link is fine and the scale is telling us its reading cannot be
    // trusted. Repeat that rather than displaying the number it sent.
    if (_wireless->packet().flags & PKT_FLAG_FAULT) {
        _fault = SystemFault::ScaleFault;
        return;
    }

    _fault = SystemFault::None;
#else
    _fault = SystemFault::None;
#endif
}

const char *DiagnosticManager::faultTitle() const {
    // First two letters must be distinctive: the panel shows "Err" plus
    // exactly those two characters.
    switch (_fault) {
        case SystemFault::RadioNotResponding: return "RADIO";        // Err rA
        case SystemFault::NoSignal:           return "NO SIGNAL";    // Err no
        case SystemFault::ScaleFault:         return "SCALE FAULT";  // Err Sc
        default:                              return "";
    }
}

const char *DiagnosticManager::faultDetail() const {
    switch (_fault) {
        case SystemFault::RadioNotResponding: return "nRF24 not responding - check 3V3 and CE/CSN";
        case SystemFault::NoSignal:           return "no packet from the scale";
        case SystemFault::ScaleFault:         return "the scale reports a hardware fault";
        default:                              return "";
    }
}
