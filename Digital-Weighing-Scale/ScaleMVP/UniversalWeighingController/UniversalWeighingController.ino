/*
 * UniversalWeighingController.ino
 * ------------------------------------------------------------------------
 * Universal Weighing Controller — ESP32
 *
 *      "One firmware, many weighing systems."
 *
 * This sketch is intentionally almost empty. Arduino requires a .ino entry
 * point, but all behaviour lives in the App class (src/app/) which owns the
 * modules. Keeping the entry point trivial means the application can be
 * reasoned about, and later reused, without the Arduino scaffolding.
 *
 * To configure the controller, edit ONLY these files:
 *
 *      src/config/hardware.h   — how many cells, which ADC, which display
 *      src/config/pins.h       — GPIO assignments
 *      src/config/features.h   — which features are compiled in
 *      src/config/calibration.h— calibration defaults and storage layout
 *
 * WHAT THIS FIRMWARE CAN BE BUILT AS:
 *
 *   Bench scale (default)   load cell -> OLED -> 4x4 keypad
 *   Industrial indicator    DISPLAY_7SEG for a MAX7219 seven-segment panel
 *
 * The nRF24 link is ON by default (ENABLE_NRF): the controller and the
 * dashboard are a pair. A board with no radio fitted still weighs.
 *
 * THE REMOTE PANEL IS A SEPARATE FIRMWARE: ../WirelessDashboard. This
 * project only ever transmits. The two are independent sketches that share
 * four hand-copied files — see src/protocol/WeightPacket.h, which lists
 * them and states the rules for changing them.
 *
 * Required libraries (Library Manager):
 *      Adafruit GFX Library    ) only for DISPLAY_OLED — the MAX7219 and
 *      Adafruit SH110X         ) nRF24 drivers have no dependencies
 * ------------------------------------------------------------------------
 */

#include "src/app/App.h"

static App app;

void setup() {
    app.setup();
}

void loop() {
    app.loop();
}
