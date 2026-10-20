/*
 * calibration.h
 * ------------------------------------------------------------------------
 * Calibration defaults and the on-flash storage layout.
 *
 * These are FALLBACKS only. Once the user runs the calibration procedure
 * the real values live in EEPROM/NVS and are loaded at boot; the constants
 * here just keep the scale usable (and the display non-garbage) on a fresh
 * board that has never been calibrated.
 * ------------------------------------------------------------------------
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include "hardware.h"   // SCALE_CAPACITY_G

// ===================================================================
//  DEFAULTS FOR AN UNCALIBRATED BOARD
// ===================================================================

// Raw ADC counts per gram. A placeholder of 1.0 means the display shows
// essentially raw counts until a real calibration is performed — which is
// deliberate: an obviously wrong reading is safer than a plausibly wrong one.
#define DEFAULT_COUNTS_PER_GRAM 1.0f

// Raw counts at zero load.
#define DEFAULT_ZERO_OFFSET_COUNTS 0.0f

// Weight of the reference mass suggested at the start of the procedure.
#define DEFAULT_KNOWN_WEIGHT_G 1000.0f

// Bounds accepted when the user dials in the reference mass.
#define CAL_WEIGHT_MIN_G 1.0f
#define CAL_WEIGHT_MAX_G SCALE_CAPACITY_G

// ===================================================================
//  CALIBRATION PROCEDURE TUNING
// ===================================================================

// Samples averaged when capturing the zero point and the loaded point.
// More samples = better calibration, at the cost of a longer wait.
#define CAL_AVERAGE_SAMPLES 16

// Total time allowed to collect those samples before giving up.
#define CAL_CAPTURE_TIMEOUT_MS 5000

// A calibration whose span is smaller than this is rejected: it means the
// reference mass was never actually placed on the platform.
#define CAL_MIN_SPAN_COUNTS 1000L

// How long the "Saved" / "Failed" confirmation stays on screen.
#define CAL_RESULT_DISPLAY_MS 2000

// ===================================================================
//  PERSISTENT STORAGE LAYOUT
// ===================================================================

// Bump CONFIG_VERSION whenever ConfigRecord changes shape; a mismatch makes
// StorageManager discard the old record and fall back to defaults instead
// of reinterpreting stale bytes as floats.
#define CONFIG_MAGIC   0x53434C31UL   // "SCL1"
#define CONFIG_VERSION 1

#define EEPROM_SIZE       64
#define EEPROM_BASE_ADDR  0

#endif // CALIBRATION_H
