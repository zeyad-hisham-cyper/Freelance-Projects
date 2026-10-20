/*
 * setup.cpp
 * ------------------------------------------------------------------------
 * Power-on sequence:
 *
 *      Load Configuration -> Initialize Hardware -> start listening
 *
 * The display comes up first so that a radio failure has somewhere to be
 * reported. Both devices share one SPI bus and each brings it up through
 * SpiBus::begin(), so the order below is about reporting, not about the bus.
 * ------------------------------------------------------------------------
 */

#include "App.h"

App::App() : _lastDisplayMs(0), _lastLogMs(0) {}

void App::setup() {
#ifdef DEBUG_SERIAL
    Serial.begin(115200);
    delay(300);
#endif

    LOGLN();
    LOGLN("=====================================");
    LOG(FIRMWARE_NAME);
    LOG("  v");
    LOGLN(FIRMWARE_VERSION);
    LOGLN("=====================================");

    // --- Display first -------------------------------------------------
    if (!_display.begin()) {
        LOGLN("[Display] init FAILED (bad MAX7219_DIGITS?)");
    }

    // The MAX7219 is write-only, so a dead panel looks exactly like a
    // healthy one on the bus. The self-test is the only way to tell, and a
    // dashboard that spends months showing the same digits is precisely
    // where a failed segment goes unnoticed.
#if DISPLAY_SELFTEST_ON_BOOT
    _display.runSelfTest();
#endif

    _display.showBoot();

    // --- Radio ----------------------------------------------------------
    if (!_wireless.begin()) {
        LOGLN("[Wireless] radio unavailable - nothing can be received");
    }
#ifdef DEBUG_SERIAL
    _wireless.logStatus(Serial);
#endif

    _diagnostics.begin(&_wireless);

    // Let the boot screen stay readable for a moment. This is the only
    // blocking delay outside the self-test.
    delay(1200);

    _lastDisplayMs = millis();
    _lastLogMs     = millis();

    LOGLN("[App] running");
}
