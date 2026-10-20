/*
 * config.h
 * ------------------------------------------------------------------------
 * The single header every module includes. It pulls in the whole
 * configuration set in dependency order:
 *
 *      features.h   -> what is enabled
 *      hardware.h   -> what is fitted
 *      pins.h       -> where it is wired  (needs hardware.h)
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

// ----------------------------------------------------------------- Sanity
//
// These checks are the only thing standing between a typo here and a
// dashboard that lights up, looks healthy, and never receives anything.
#if !defined(DISPLAY_7SEG)
    #error "DISPLAY_7SEG is not optional on a dashboard - see features.h"
#endif
#if !defined(ENABLE_NRF)
    #error "ENABLE_NRF is not optional on a dashboard - see features.h"
#endif
#if MAX7219_DIGITS < 1 || MAX7219_DIGITS > 8
    #error "MAX7219_DIGITS must be 1..8 (one MAX7219 scans at most 8 digits)"
#endif
#if MAX7219_BRIGHTNESS > 15
    #error "MAX7219_BRIGHTNESS must be 0..15"
#endif
#if NRF_CHANNEL > 125
    #error "NRF_CHANNEL must be 0..125"
#endif
#if NRF_ADDRESS_WIDTH < 3 || NRF_ADDRESS_WIDTH > 5
    #error "NRF_ADDRESS_WIDTH must be 3..5"
#endif
#if NRF_PAYLOAD_SIZE < 1 || NRF_PAYLOAD_SIZE > 32
    #error "NRF_PAYLOAD_SIZE must be 1..32"
#endif

// A dashboard whose data can go stale faster than it is refreshed would
// flicker between the reading and the stale screen.
#if LINK_TIMEOUT_MS <= DISPLAY_REFRESH_MS
    #error "LINK_TIMEOUT_MS must be well above DISPLAY_REFRESH_MS"
#endif

#define FIRMWARE_NAME    "Wireless Dashboard"
#define FIRMWARE_VERSION "1.0"

#endif // CONFIG_H
