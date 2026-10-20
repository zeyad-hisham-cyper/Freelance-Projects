/*
 * loop.cpp
 * ------------------------------------------------------------------------
 * The main cycle.
 *
 * Nothing here blocks. The radio is polled, the panel is redrawn on an
 * interval rather than every pass, and the serial log is rate-limited — so
 * a packet arriving between two redraws is never missed, it simply appears
 * at the next one.
 * ------------------------------------------------------------------------
 */

#include "App.h"
#include "../modules/UnitFormat.h"

void App::loop() {
    // 1. Receive: drain the radio's FIFO, recover a radio that has died.
    _wireless.update();

    // 2. Classify: is anything we know still worth showing?
    _diagnostics.update();

    // 3. Present.
    if (millis() - _lastDisplayMs >= DISPLAY_REFRESH_MS) {
        _lastDisplayMs = millis();
        refreshDisplay();
    }

    logSerial();
}

// =====================================================================
//  DISPLAY
// =====================================================================

void App::refreshDisplay() {
    if (!_display.isReady()) {
        return;
    }

    // Priority order matters, and it is the whole safety argument of this
    // firmware: a fault always outranks a reading, so there is no path that
    // reaches showReading() with data that DiagnosticManager has rejected.
    if (_diagnostics.hasFault()) {
        _display.showFault(_diagnostics.faultTitle());
        return;
    }

    // hasFault() being false already implies the link is up, which implies a
    // packet exists. The check stays as a guard against a future fault rule
    // that stops implying it.
    if (!_wireless.hasPacket()) {
        _display.showWaiting();
        return;
    }

    _display.showReading(_wireless.packet());
}

// =====================================================================
//  SERIAL DIAGNOSTICS
// =====================================================================

void App::logSerial() {
#ifdef DEBUG_SERIAL
    if (millis() - _lastLogMs < SERIAL_LOG_MS) {
        return;
    }
    _lastLogMs = millis();

    if (_diagnostics.hasFault()) {
        Serial.print("FAULT: ");
        Serial.print(_diagnostics.faultTitle());
        Serial.print(" - ");
        Serial.println(_diagnostics.faultDetail());
        return;
    }

    const WeightPacket &packet = _wireless.packet();

    Serial.print("value=");
    Serial.print(packet.displayValue, unitDecimals(packet.unit));
    Serial.print(unitLabel(packet.unit));
    Serial.print("  grams=");
    Serial.print(packet.grams, 1);
    Serial.print(packet.flags & PKT_FLAG_STABLE ? "  [stable]" : "  [moving]");
    if (packet.flags & PKT_FLAG_TARED)          Serial.print("  [net]");
    if (!(packet.flags & PKT_FLAG_CALIBRATED))  Serial.print("  [uncalibrated]");

    Serial.print("  age=");
    Serial.print(_wireless.packetAgeMs());
    Serial.print("ms  rx=");
    Serial.print(_wireless.receivedCount());
    if (_wireless.missedCount()) {
        Serial.print("  missed=");
        Serial.print(_wireless.missedCount());
    }
    if (_wireless.rejectedCount()) {
        Serial.print("  rejected=");
        Serial.print(_wireless.rejectedCount());
    }
    Serial.println();
#endif
}
