/*
 * Config.h — Controller Node
 * ------------------------------------------------------------------------
 * Single source of truth for pin assignments and shared constants.
 *
 * The PMK/LMK strings MUST be identical to the ones in ScaleNode/Config.h.
 * ------------------------------------------------------------------------
 */

#pragma once

// ---- MAX7219 SPI Pins ----
#define DATA_PIN    23
#define CLK_PIN     18
#define CS_PIN       5

// ---- Display Settings ----
#define DISPLAY_INTENSITY   8     // 0–15

// ---- Serial ----
#define SERIAL_BAUD_RATE    115200

// ---- ESP-NOW Encryption Keys ----
// Must be exactly 16 bytes and identical to the Scale Node.
// Encryption (LMK) is reserved for Phase 4.
#define ESPNOW_PMK   "Scale_PMK_Key_12"
#define ESPNOW_LMK   "Scale_LMK_Key_12"
