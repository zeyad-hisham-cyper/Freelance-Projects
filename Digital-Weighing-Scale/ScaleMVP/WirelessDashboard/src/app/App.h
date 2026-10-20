/*
 * App.h
 * ------------------------------------------------------------------------
 * The application layer: owns every module and wires them together.
 *
 * The dashboard's cycle is deliberately tiny — it has no sensor, no
 * calibration and no buttons:
 *
 *      receive -> validate -> classify -> display
 *
 * Everything interesting is in the classify step, and that lives in
 * DiagnosticManager rather than here, so the rule "never show an untrusted
 * number" is stated in exactly one place.
 * ------------------------------------------------------------------------
 */

#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include "../config/config.h"
#include "../modules/DisplayManager.h"
#include "../modules/WirelessManager.h"
#include "../modules/DiagnosticManager.h"

class App {
public:
    App();

    void setup();   // app/setup.cpp
    void loop();    // app/loop.cpp

private:
    DisplayManager    _display;
    WirelessManager   _wireless;
    DiagnosticManager _diagnostics;

    uint32_t _lastDisplayMs;
    uint32_t _lastLogMs;

    void refreshDisplay();
    void logSerial();
};

#endif  // APP_H
