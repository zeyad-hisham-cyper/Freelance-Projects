/*
 * DisplayManager.cpp
 * ------------------------------------------------------------------------
 * See DisplayManager.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "DisplayManager.h"
#include "UnitFormat.h"

DisplayManager::DisplayManager() : _ready(false) {}

bool DisplayManager::begin() {
    _ready = _panel.begin();
    return _ready;
}

void DisplayManager::showBoot() {
    if (!_ready) return;
    _panel.showBoot(FIRMWARE_VERSION);
}

void DisplayManager::showWaiting() {
    if (!_ready) return;

    // Six dashes, not a zero: a dashboard that shows "0" before it has heard
    // anything is indistinguishable from an empty platform.
    _panel.writeText("------");
}

void DisplayManager::showReading(const WeightPacket &packet) {
    if (!_ready) return;

    // The unit label has nowhere to go on a bare six-digit panel, but the
    // decimals it implies do — that is how the operator can tell 2.500 kg
    // from 2500.0 g at a glance.
    _panel.showWeight(packet.displayValue,
                      unitLabel(packet.unit),
                      unitDecimals(packet.unit),
                      (packet.flags & PKT_FLAG_STABLE) != 0,
                      (packet.flags & PKT_FLAG_TARED) != 0);
}

void DisplayManager::showFault(const char *title) {
    if (!_ready) return;
    _panel.showError(title, nullptr);
}

void DisplayManager::showMessage(const char *text) {
    if (!_ready) return;
    _panel.showMessage(text, nullptr, nullptr);
}

void DisplayManager::setBrightness(uint8_t level) {
    if (!_ready) return;
    _panel.setBrightness(level);
}

void DisplayManager::runSelfTest() {
    if (!_ready) return;
    _panel.runSelfTest();
}
