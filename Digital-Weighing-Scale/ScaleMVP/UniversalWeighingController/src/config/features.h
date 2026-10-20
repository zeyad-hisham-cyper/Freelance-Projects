/*
 * features.h
 * ------------------------------------------------------------------------
 * Compile-time feature switches.
 *
 * Anything disabled here is removed by the preprocessor, so its driver and
 * module code is never compiled into the binary. This is how the same
 * firmware fits both a minimal bench scale and a full industrial indicator.
 *
 * PHASE 1 SCOPE:
 *   Load cell reading + OLED display + TARE / MODE / CALIBRATION buttons.
 *   Wireless (NRF24) and the 7-segment display are intentionally left off;
 *   their hooks exist in the architecture but are not built yet.
 * ------------------------------------------------------------------------
 */

#ifndef FEATURES_H
#define FEATURES_H

// ---------------------------------------------------------------- Filtering
// Moving-average filter over the raw ADC counts. Strongly recommended: a
// bare 24-bit reading jitters by several hundred counts even at rest.
#define ENABLE_FILTER

// ------------------------------------------------------------------ Storage
// Persist calibration data and user settings across power cycles.
// On the ESP32 the Arduino EEPROM library is a wrapper over NVS flash.
#define ENABLE_EEPROM

// --------------------------------------------------------------- Diagnostics
// Track ADC timeouts / sensor faults and surface them on the display.
#define ENABLE_DIAGNOSTICS

// ------------------------------------------------------------------ Wireless
// Broadcasts the reading to a remote panel over an nRF24L01+.
//
// The receiver is a SEPARATE FIRMWARE: ../WirelessDashboard. This switch
// adds the TRANSMITTER to the scale; it does not build a dashboard. The
// radio settings in hardware.h must match the dashboard's.
//
// ON by default: the controller and the dashboard are a pair, and the link
// is part of the product rather than an option. A missing or unwired radio
// is not fatal — the scale still weighs, logs the fault, and shows
// "RADIO" on the panel — so leaving this on costs a board without an nRF24
// nothing but the Flash the driver occupies.
#define ENABLE_NRF

// -------------------------------------------------------------------- Debug
// Serial logging. Costs Flash and slows the loop; disable for production.
#define DEBUG_SERIAL

#ifdef DEBUG_SERIAL
    #define LOG(x)        Serial.print(x)
    #define LOGLN(x)      Serial.println(x)
    #define LOGF(x, y)    Serial.print(x, y)
    #define LOGFLN(x, y)  Serial.println(x, y)
#else
    #define LOG(x)
    #define LOGLN(x)
    #define LOGF(x, y)
    #define LOGFLN(x, y)
#endif

#endif // FEATURES_H
