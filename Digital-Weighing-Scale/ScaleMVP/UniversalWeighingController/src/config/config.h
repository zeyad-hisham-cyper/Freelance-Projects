/*
 * config.h
 * ------------------------------------------------------------------------
 * The single header every module includes. It pulls in the whole
 * configuration set in dependency order:
 *
 *      features.h   -> what is enabled
 *      hardware.h   -> what is fitted
 *      pins.h       -> where it is wired  (needs hardware.h)
 *      calibration.h-> how it is scaled   (needs hardware.h)
 *
 * Application and module code should never include the individual files.
 * ------------------------------------------------------------------------
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#include "features.h"
#include "hardware.h"
#include "pins.h"
#include "calibration.h"

// ----------------------------------------------------------------- Sanity
//
// NOTE: these checks are the only thing standing between a typo here and a
// scale that boots, lights up, and silently never reads. Do not "fix" a
// build error by adding a do-nothing stub in WeightManager — every selected
// ADC must have a real driver behind it.
#if defined(USE_ADS1232) && defined(USE_HX711)
    #error "Select only one ADC in hardware.h"
#endif
#if !defined(USE_ADS1232) && !defined(USE_HX711)
    #error "No ADC selected in hardware.h"
#endif
#if defined(DISPLAY_OLED) && defined(DISPLAY_7SEG)
    #error "Select only one display in hardware.h"
#endif
#if !defined(DISPLAY_OLED) && !defined(DISPLAY_7SEG)
    #error "No display selected in hardware.h"
#endif
#if LOAD_CELL_COUNT < 1
    #error "LOAD_CELL_COUNT must be at least 1"
#endif

// The 4x4 keypad occupies GPIO 32/33/26/27 and 12/13/14/15. Three of those
// are the ADS1232's PDWN (26), SPEED (27) and A0 (14), so the two cannot
// coexist on this pin map — and a silent conflict here looks like a keypad
// that types by itself while the ADC randomly powers down.
//
// The keypad freed only GPIO4 (the old CAL button), which is one pin short
// of what the ADS1232 needs. Either fit the HX711, which needs two pins and
// fits comfortably, or re-assign the ADS1232 pins in pins.h to whatever your
// board actually has spare.
#ifdef USE_ADS1232
    #error "Pin conflict: 4x4 keypad uses GPIO26/27/14, which are the ADS1232 PDWN/SPEED/A0 - see the note in config.h"
#endif

#ifdef DISPLAY_7SEG
    #if MAX7219_DIGITS < 1 || MAX7219_DIGITS > 8
        #error "MAX7219_DIGITS must be 1..8 (one MAX7219 scans at most 8 digits)"
    #endif
    #if MAX7219_BRIGHTNESS > 15
        #error "MAX7219_BRIGHTNESS must be 0..15"
    #endif
#endif

#ifdef ENABLE_NRF
    #if NRF_CHANNEL > 125
        #error "NRF_CHANNEL must be 0..125"
    #endif
    #if NRF_ADDRESS_WIDTH < 3 || NRF_ADDRESS_WIDTH > 5
        #error "NRF_ADDRESS_WIDTH must be 3..5"
    #endif
    #if NRF_PAYLOAD_SIZE < 1 || NRF_PAYLOAD_SIZE > 32
        #error "NRF_PAYLOAD_SIZE must be 1..32"
    #endif
#endif

#define FIRMWARE_NAME    "Universal Scale"
#define FIRMWARE_VERSION "1.0-p1"

#endif // CONFIG_H
