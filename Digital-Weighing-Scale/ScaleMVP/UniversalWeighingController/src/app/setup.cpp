/*
 * setup.cpp
 * ------------------------------------------------------------------------
 * Power-on sequence, in the order system.md specifies:
 *
 *      Load Configuration -> Initialize Hardware
 *      -> Load Calibration Data -> start measuring
 *
 * Ordering matters: storage comes up before CalibrationManager (which reads
 * from it), and the display comes up early so any later failure has
 * somewhere to be reported.
 *
 * The display and the radio share one SPI bus, and both call SpiBus::begin()
 * through their drivers. Whichever runs first brings the bus up; the order
 * below is therefore about reporting, not about the bus.
 * ------------------------------------------------------------------------
 */

#include "App.h"

App::App()
    : _lastDisplayMs(0), _lastLogMs(0),
      _messageTitle(nullptr), _messageLine(nullptr), _messageUntilMs(0) {
    _valueBuf[0] = '\0';
    _hintBuf[0]  = '\0';
}

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
    LOG("Load cells: ");
    LOGLN(LOAD_CELL_COUNT);
    LOGLN("=====================================");

    // --- Display first -----------------------------------------------
    // Bringing the panel up before anything else means a failure in the
    // ADC or storage can actually be shown to the operator.
    if (!_display.begin()) {
#ifdef DISPLAY_7SEG
        LOGLN("[Display] init FAILED (bad MAX7219_DIGITS?)");
#else
        LOGLN("[Display] init FAILED (check I2C wiring/address)");
#endif
    } else {
        _display.showBoot();
    }

    // --- Inputs -------------------------------------------------------
    _input.begin();

    // --- Persistent storage -------------------------------------------
    if (!_storage.begin()) {
        LOGLN("[Storage] unavailable - calibration will not persist");
    }

    // --- Wireless ------------------------------------------------------
    // A missing radio is not fatal: the scale still weighs, it just cannot
    // report to the remote panel.
#ifdef ENABLE_NRF
    if (!_wireless.begin()) {
        LOGLN("[Wireless] radio unavailable - running without the link");
    }
  #ifdef DEBUG_SERIAL
    _wireless.logStatus(Serial);
  #endif
#endif

    // --- ADCs ---------------------------------------------------------
    _weight.begin();

    // --- Calibration data (needs storage + weight) --------------------
    _calibration.begin(&_weight, &_storage);

    // Restore the unit the operator was last using.
    _unit.setUnit((Unit)_calibration.storedUnit());

    _diagnostics.begin(&_weight, &_calibration, &_wireless);

    if (!_calibration.isCalibrated()) {
        LOGLN("[App] scale is NOT calibrated - press CAL to calibrate");
    }

    // Let the boot screen stay readable for a moment. This is the only
    // blocking delay in the firmware and it happens before measuring starts.
    delay(1200);

    _lastDisplayMs = millis();
    _lastLogMs     = millis();

    // Discard anything registered while the operator was powering up — a
    // finger resting on the keypad during a reset should not type.
    _input.flush();

    LOGLN("[App] running");
}
