/*
 * OledDisplay.cpp
 * ------------------------------------------------------------------------
 * See OledDisplay.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "OledDisplay.h"

#ifdef DISPLAY_OLED

// Adafruit_GFX built-in font metrics: one character occupies 6x8 pixels at
// text size 1 and scales linearly from there.
static const uint8_t CHAR_W = 6;
static const uint8_t CHAR_H = 8;

OledDisplay::OledDisplay()
    : _panel(OLED_WIDTH, OLED_HEIGHT, &Wire), _ready(false) {}

bool OledDisplay::begin() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

    // Second argument is `reset` — the I2C panels have no reset line, but
    // the library tolerates the request and it matches the bring-up sketch.
    if (!_panel.begin(OLED_I2C_ADDRESS, true)) {
        _ready = false;
        return false;
    }

    _ready = true;
    _panel.setTextColor(SH110X_WHITE);
    _panel.clearDisplay();
    _panel.display();
    return true;
}

void OledDisplay::showBoot(const char *version) {
    if (!_ready) return;

    _panel.clearDisplay();
    _panel.setTextColor(SH110X_WHITE);

    _panel.setTextSize(1);
    _panel.setCursor(0, 4);
    _panel.println(FIRMWARE_NAME);

    _panel.drawLine(0, 16, OLED_WIDTH - 1, 16, SH110X_WHITE);

    _panel.setTextSize(2);
    _panel.setCursor(0, 26);
    _panel.println("Starting");

    _panel.setTextSize(1);
    _panel.setCursor(0, 52);
    _panel.print("v");
    _panel.print(version);
    _panel.print("  cells:");
    _panel.print(LOAD_CELL_COUNT);

    _panel.display();
}

void OledDisplay::showWeight(float value, const char *unitLabel, uint8_t decimals,
                             bool stable, bool tared) {
    if (!_ready) return;

    _panel.clearDisplay();
    _panel.setTextColor(SH110X_WHITE);

    // --- Status bar -------------------------------------------------
    _panel.setTextSize(1);
    _panel.setCursor(0, 0);
    _panel.print(tared ? "NET" : "GROSS");

    // Stability marker, right-aligned. A hollow box means "still moving",
    // a filled box means the reading has settled and can be trusted.
    if (stable) {
        _panel.fillRect(OLED_WIDTH - 9, 0, 8, 8, SH110X_WHITE);
    } else {
        _panel.drawRect(OLED_WIDTH - 9, 0, 8, 8, SH110X_WHITE);
    }

    _panel.drawLine(0, 10, OLED_WIDTH - 1, 10, SH110X_WHITE);

    // --- Value ------------------------------------------------------
    drawBigValue(value, decimals, unitLabel);

    _panel.display();
}

void OledDisplay::drawBigValue(float value, uint8_t decimals, const char *unitLabel) {
    char buf[16];
    dtostrf(value, 0, decimals, buf);

    uint8_t len = strlen(buf);

    // The unit label sits at text size 2 in the bottom-right corner, so the
    // number gets the full width above it. Shrink the number only when it
    // genuinely does not fit — a 10-ton scale in grams can print 8 digits.
    uint8_t size = 3;
    if (len * CHAR_W * 3 > OLED_WIDTH) size = 2;
    if (len * CHAR_W * 2 > OLED_WIDTH) size = 1;

    int16_t x = OLED_WIDTH - (int16_t)(len * CHAR_W * size);
    if (x < 0) x = 0;

    // Vertically centre the digits in the area between the status bar and
    // the unit label.
    int16_t y = 22 + (3 - size) * 4;

    _panel.setTextSize(size);
    _panel.setCursor(x, y);
    _panel.print(buf);

    _panel.setTextSize(2);
    _panel.setCursor(OLED_WIDTH - (int16_t)(strlen(unitLabel) * CHAR_W * 2), 48);
    _panel.print(unitLabel);
}

void OledDisplay::showMessage(const char *title, const char *line1, const char *line2) {
    if (!_ready) return;

    _panel.clearDisplay();
    _panel.setTextColor(SH110X_WHITE);

    _panel.setTextSize(2);
    _panel.setCursor(0, 6);
    _panel.println(title);

    _panel.setTextSize(1);
    if (line1) {
        _panel.setCursor(0, 34);
        _panel.println(line1);
    }
    if (line2) {
        _panel.setCursor(0, 34 + CHAR_H + 4);
        _panel.println(line2);
    }

    _panel.display();
}

void OledDisplay::showCalibrationStep(uint8_t step, uint8_t totalSteps,
                                      const char *title,
                                      const char *line1, const char *line2,
                                      const char *value) {
    if (!_ready) return;

    _panel.clearDisplay();
    _panel.setTextColor(SH110X_WHITE);

    // --- Header: "CAL 2/3" on the left, step title on the right ------
    _panel.setTextSize(1);
    _panel.setCursor(0, 0);
    _panel.print("CAL ");
    _panel.print(step);
    _panel.print('/');
    _panel.print(totalSteps);

    if (title) {
        _panel.setCursor(OLED_WIDTH - (int16_t)(strlen(title) * CHAR_W), 0);
        _panel.print(title);
    }

    _panel.drawLine(0, 10, OLED_WIDTH - 1, 10, SH110X_WHITE);

    // --- Body -------------------------------------------------------
    if (value) {
        // A value is being edited: give it the big font, instructions small.
        uint8_t len = strlen(value);
        uint8_t size = (len * CHAR_W * 2 <= OLED_WIDTH) ? 2 : 1;

        _panel.setTextSize(size);
        _panel.setCursor((OLED_WIDTH - (int16_t)(len * CHAR_W * size)) / 2, 18);
        _panel.print(value);

        _panel.setTextSize(1);
        if (line1) {
            _panel.setCursor(0, 42);
            _panel.println(line1);
        }
        if (line2) {
            _panel.setCursor(0, 42 + CHAR_H + 3);
            _panel.println(line2);
        }
    } else {
        _panel.setTextSize(1);
        if (line1) {
            _panel.setCursor(0, 20);
            _panel.println(line1);
        }
        if (line2) {
            _panel.setCursor(0, 20 + CHAR_H + 6);
            _panel.println(line2);
        }
    }

    _panel.display();
}

void OledDisplay::showError(const char *title, const char *detail) {
    if (!_ready) return;

    _panel.clearDisplay();
    _panel.setTextColor(SH110X_WHITE);

    _panel.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SH110X_WHITE);

    _panel.setTextSize(2);
    _panel.setCursor(6, 12);
    _panel.println(title);

    _panel.setTextSize(1);
    if (detail) {
        _panel.setCursor(6, 40);
        _panel.println(detail);
    }

    _panel.display();
}

#endif // DISPLAY_OLED
