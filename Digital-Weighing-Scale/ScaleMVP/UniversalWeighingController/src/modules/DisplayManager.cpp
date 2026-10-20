/*
 * DisplayManager.cpp
 * ------------------------------------------------------------------------
 * See DisplayManager.h for design rationale.
 *
 * Both drivers expose the same semantic calls, and config.h guarantees that
 * exactly one display is selected, so `_driver` always exists and most
 * methods are a plain forward. Only the calls that genuinely have no
 * meaning on one of the panels — brightness, lamp test — need a #ifdef.
 * ------------------------------------------------------------------------
 */

#include "DisplayManager.h"

DisplayManager::DisplayManager() : _ready(false) {}

bool DisplayManager::begin() {
    _ready = _driver.begin();
    return _ready;
}

void DisplayManager::showBoot() {
    _driver.showBoot(FIRMWARE_VERSION);
}

void DisplayManager::showWeight(float value, const char *unitLabel, uint8_t decimals,
                                bool stable, bool tared) {
    _driver.showWeight(value, unitLabel, decimals, stable, tared);
}

void DisplayManager::showMessage(const char *title, const char *line1, const char *line2) {
    _driver.showMessage(title, line1, line2);
}

void DisplayManager::showCalibrationStep(uint8_t step, const char *title,
                                         const char *line1, const char *line2,
                                         const char *value) {
    _driver.showCalibrationStep(step, 3, title, line1, line2, value);
}

void DisplayManager::showError(const char *title, const char *detail) {
    _driver.showError(title, detail);
}

void DisplayManager::setBrightness(uint8_t level) {
#ifdef DISPLAY_7SEG
    _driver.setBrightness(level);
#else
    // The SH1106 has a contrast register, but changing it on an OLED trades
    // legibility for burn-in rather than fighting ambient light, so the
    // request is ignored rather than half-honoured.
    (void)level;
#endif
}

void DisplayManager::runSelfTest() {
#ifdef DISPLAY_7SEG
    _driver.runSelfTest();
#endif
}
