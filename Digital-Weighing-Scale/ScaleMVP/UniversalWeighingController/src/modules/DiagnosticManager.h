/*
 * DiagnosticManager.h
 * ------------------------------------------------------------------------
 * Watches the hardware and classifies faults so the application can show
 * one clear message instead of a plausible-looking wrong number.
 *
 * Covered here:
 *   - ADC not delivering conversions (unwired, unpowered, dead cable)
 *   - reading beyond the configured platform capacity (overload)
 *   - no radio answering on the SPI bus
 *
 * NOTE on what is deliberately NOT a fault: a scale that cannot reach its
 * remote dashboard is still weighing correctly, so a dropped link is
 * reported in the serial log and nowhere else. The dashboard firmware makes
 * the opposite call about the same dropped link, because a stale number on
 * a remote display is worse than no number at all.
 * ------------------------------------------------------------------------
 */

#ifndef DIAGNOSTIC_MANAGER_H
#define DIAGNOSTIC_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"
#include "WeightManager.h"
#include "CalibrationManager.h"
#include "WirelessManager.h"

enum class SystemFault : uint8_t {
    None = 0,
    AdcNotResponding,
    Overload,
    RadioNotResponding    // no nRF24 on the bus — wiring or supply
};

class DiagnosticManager {
public:
    DiagnosticManager();

    // `wireless` may be null, and a manager built without ENABLE_NRF simply
    // reports no radio, so the wireless checks cost nothing on a board that
    // has none.
    void begin(WeightManager *weight, CalibrationManager *calibration,
               WirelessManager *wireless = nullptr);

    // Re-evaluates the fault state. Call every loop.
    void update();

    SystemFault fault() const { return _fault; }
    bool        hasFault() const { return _fault != SystemFault::None; }

    // Short text suitable for the display.
    const char *faultTitle() const;
    const char *faultDetail() const;

private:
    WeightManager      *_weight;
    CalibrationManager *_calibration;
    WirelessManager    *_wireless;
    SystemFault         _fault;
};

#endif // DIAGNOSTIC_MANAGER_H
