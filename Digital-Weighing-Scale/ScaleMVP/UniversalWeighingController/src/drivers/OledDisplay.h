/*
 * OledDisplay.h
 * ------------------------------------------------------------------------
 * Driver for the SH1106 128x64 I2C OLED panel.
 *
 * DESIGN NOTE — why the screen composition lives in the DRIVER:
 * A 6-digit seven-segment module physically cannot render "a title, a
 * subtitle and two lines of instructions." So the *layout* of each screen
 * is inherently display-specific and belongs here, next to the panel that
 * has to draw it. What is shared across displays is the SEMANTIC call —
 * "show a weight", "show a calibration step" — and that unified interface
 * is DisplayManager's job.
 *
 * Nothing above this file ever touches Adafruit_GFX or SH110X.
 * ------------------------------------------------------------------------
 */

#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Arduino.h>
#include "../config/config.h"

#ifdef DISPLAY_OLED

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

class OledDisplay {
public:
    OledDisplay();

    // Brings up I2C and the panel. Returns false if the panel does not ACK,
    // which normally means a wiring or address problem.
    bool begin();

    void showBoot(const char *version);

    // Main weighing screen.
    //   value      - already converted to the active unit
    //   unitLabel  - "g" / "kg" / "lb" / "oz"
    //   decimals   - how many digits after the point to print
    //   stable     - draws the stability marker
    //   tared      - draws the NET marker (gross otherwise)
    void showWeight(float value, const char *unitLabel, uint8_t decimals,
                    bool stable, bool tared);

    // Generic notice screen, e.g. "Tare set" or "Saved".
    void showMessage(const char *title, const char *line1, const char *line2);

    // Calibration wizard screen. `value` is optional (nullptr hides it).
    void showCalibrationStep(uint8_t step, uint8_t totalSteps,
                             const char *title,
                             const char *line1, const char *line2,
                             const char *value);

    void showError(const char *title, const char *detail);

private:
    Adafruit_SH1106G _panel;
    bool _ready;

    // Draws the value right-aligned at the largest text size that still
    // fits the panel width, so "9.99" and "-12345.67" both stay on screen.
    void drawBigValue(float value, uint8_t decimals, const char *unitLabel);
};

#endif // DISPLAY_OLED
#endif // OLED_DISPLAY_H
