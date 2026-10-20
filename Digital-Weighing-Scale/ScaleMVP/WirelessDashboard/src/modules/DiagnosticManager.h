/*
 * DiagnosticManager.h
 * ------------------------------------------------------------------------
 * Decides, once per loop, whether the dashboard is allowed to show a number
 * at all — and if not, which of the four reasons applies.
 *
 * A remote display has exactly one way to be dangerous: showing a stale or
 * wrong reading as though it were current. So the rule this module enforces
 * is that the reading is the LAST thing checked, after every reason not to
 * trust it has been ruled out.
 *
 * Ordered by what the operator should act on first:
 *
 *   RadioNotResponding  the nRF24 on THIS board is dead — wiring or supply.
 *                       Nothing else can be diagnosed until it is fixed.
 *   NoSignal            the radio works; nothing has arrived recently. The
 *                       scale is off, out of range, or misconfigured.
 *   ScaleFault          packets are arriving and the scale is reporting its
 *                       own hardware fault. Go and look at the scale.
 *   None                a fresh, trustworthy reading. Show it.
 * ------------------------------------------------------------------------
 */

#ifndef DIAGNOSTIC_MANAGER_H
#define DIAGNOSTIC_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"
#include "WirelessManager.h"

enum class SystemFault : uint8_t {
    None = 0,
    RadioNotResponding,
    NoSignal,
    ScaleFault
};

class DiagnosticManager {
public:
    DiagnosticManager();

    void begin(WirelessManager *wireless);

    // Re-evaluates the fault state. Call every loop.
    void update();

    SystemFault fault() const { return _fault; }
    bool        hasFault() const { return _fault != SystemFault::None; }

    // Short text for the panel: the driver renders it as "Err" plus a
    // two-letter tag, so the first two letters have to be distinctive.
    const char *faultTitle() const;

    // Longer text for the serial log.
    const char *faultDetail() const;

private:
    WirelessManager *_wireless;
    SystemFault      _fault;
};

#endif  // DIAGNOSTIC_MANAGER_H
