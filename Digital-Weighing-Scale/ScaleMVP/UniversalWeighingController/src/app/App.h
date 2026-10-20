/*
 * App.h
 * ------------------------------------------------------------------------
 * The application layer: owns every module and wires them together.
 *
 * It is the ONLY place that knows about the user interface as a whole —
 * which button does what, which screen is showing, and in what order the
 * measurement chain runs (system.md, "Measurement Flow"):
 *
 *      read ADC -> filter -> average cells -> calibrate -> tare
 *               -> convert unit -> update display -> (wireless, Phase 2)
 *
 * Modules below never call each other behind App's back, so a module can be
 * replaced or tested in isolation.
 *
 * The front panel is a 4x4 keypad; InputManager.h documents the key map.
 * ------------------------------------------------------------------------
 */

#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include "../config/config.h"
#include "../modules/WeightManager.h"
#include "../modules/StorageManager.h"
#include "../modules/CalibrationManager.h"
#include "../modules/UnitManager.h"
#include "../modules/DisplayManager.h"
#include "../modules/InputManager.h"
#include "../modules/DiagnosticManager.h"
#include "../modules/WirelessManager.h"

class App {
public:
    App();

    void setup();   // app/setup.cpp
    void loop();    // app/loop.cpp

private:
    WeightManager      _weight;
    StorageManager     _storage;
    CalibrationManager _calibration;
    UnitManager        _unit;
    DisplayManager     _display;
    InputManager       _input;
    DiagnosticManager  _diagnostics;
    WirelessManager    _wireless;

    uint32_t _lastDisplayMs;
    uint32_t _lastLogMs;

    // Transient confirmation banner ("Tare set", "Cancelled", ...).
    const char *_messageTitle;
    const char *_messageLine;
    uint32_t    _messageUntilMs;

    char _valueBuf[16];   // formatting scratch for the calibration value
    char _hintBuf[24];    // ...and for the line of help beneath it

    // --- loop.cpp helpers -------------------------------------------
    void handleKeys();
    void handleWeighingKey(char key);
    void handleCalibrationKey(char key);
    void refreshDisplay();
    void drawCalibrationScreen();
    void logSerial();

    // Broadcasts the current reading. Compiles away without ENABLE_NRF.
    void transmitWeight();

    void showBanner(const char *title, const char *line, uint32_t durationMs = 900);
    bool bannerActive() const { return millis() < _messageUntilMs; }
};

#endif // APP_H
