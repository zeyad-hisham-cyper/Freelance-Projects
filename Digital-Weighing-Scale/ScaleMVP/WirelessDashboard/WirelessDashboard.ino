/*
 * WirelessDashboard.ino
 * ------------------------------------------------------------------------
 * Wireless Dashboard — ESP32
 *
 * A remote display for the Universal Weighing Controller. It has no load
 * cell, no calibration and no buttons: it listens on the nRF24 link and puts
 * whatever the scale reports on a MAX7219 six-digit panel.
 *
 * This sketch is intentionally almost empty. Arduino requires a .ino entry
 * point, but all behaviour lives in the App class (src/app/).
 *
 * TWO INDEPENDENT FIRMWARES
 * -------------------------
 * This is a standalone project. Its counterpart is UniversalWeighingController
 * (the scale). They share nothing at build time — but four files are kept
 * byte-for-byte identical by hand, and the link breaks if they drift:
 *
 *      src/protocol/WeightPacket.h     the wire contract
 *      src/drivers/SpiBus.*            shared-bus bring-up
 *      src/drivers/MAX7219.*           display driver
 *      src/drivers/NRF24.*             radio driver
 *
 * The radio settings in src/config/hardware.h must also match the scale's:
 * channel, data rate, addresses, address width and payload size. Two boards
 * that disagree will each report a perfectly healthy radio and never
 * exchange a packet.
 *
 * To configure this board, edit ONLY these files:
 *
 *      src/config/hardware.h   — digits, brightness, radio, link timeout
 *      src/config/pins.h       — GPIO assignments
 *      src/config/features.h   — diagnostics and serial logging
 *
 * Required libraries: none. Both drivers are register-level and depend on
 * nothing but the ESP32 core's SPI library.
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
