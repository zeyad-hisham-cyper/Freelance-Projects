/*
 * Config.h
 * ------------------------------------------------------------------------
 * Single source of truth for pin assignments and hardware-level constants.
 *
 * WHY THIS FILE EXISTS:
 * Keeping every pin number and hardware constant in one header means that
 * when Phase 3 introduces per-load-cell configuration (5kg/10kg/20kg/...),
 * there is one obvious place to add it, and no application code needs to
 * change — only this file (and eventually a small config struct here).
 * ------------------------------------------------------------------------
 */

#ifndef CONFIG_H
#define CONFIG_H

// ---- ADS1232 pin wiring ----
// DOUT: data output from ADC (input to ESP32)
// SCLK: serial clock driving the ADC (output from ESP32)
// PDWN: power-down control, active LOW = powered down, HIGH = active
// SPEED: LOW = 10 SPS (low noise), HIGH = 80 SPS (fast)
// A0: channel select on the 2-channel module (LOW = channel A, HIGH = channel B)
#define PIN_ADS1232_DOUT   4
#define PIN_ADS1232_SCLK   5
#define PIN_ADS1232_PDWN   16
#define PIN_ADS1232_SPEED  17
#define PIN_ADS1232_A0     18

// ---- UI Pins ----
#define PIN_BTN_TARE       19
#define PIN_BTN_CAL        21
#define PIN_LED_GREEN      22
#define PIN_LED_RED        23 // Red/Yellow status LED

// Which physical channel this Scale Node reads from.
// (For the 2-channel module; irrelevant if you're only using channel A.)
#define ADS1232_CHANNEL_A  LOW
#define ADS1232_CHANNEL_B  HIGH

// ---- Timing ----
#define ADS1232_TIMEOUT_MS        250   // Max time to wait for DOUT ready-low
#define SERIAL_BAUD_RATE          115200
#define READ_INTERVAL_MS          200   // How often we print a new reading

// ---- Stability ----
#define STABILITY_WINDOW_SIZE     10    // Number of samples to keep for stability check (10 samples @ 10 SPS = 1 sec)
#define STABILITY_THRESHOLD_RAW   150   // Max Peak-to-Peak raw difference considered stable

// ---- NVS Storage ----
#define NVS_NAMESPACE             "scale"
#define NVS_KEY_OFFSET            "zero_offset"
#define NVS_KEY_FACTOR            "cal_factor"

// ---- ESP-NOW ----
// How often the Scale Node retransmits its pairing request when unpaired.
#define ESPNOW_PAIRING_INTERVAL_MS    2000

// How often the Scale Node transmits a weight packet once paired.
#define ESPNOW_SEND_INTERVAL_MS       200

// Pre-shared key used by both nodes to derive the per-peer encryption key.
// This MUST be exactly 16 bytes and identical on both the Scale Node and
// the Controller Node.
// NOTE: Encryption is reserved for Phase 4. The key is defined here so the
// config is ready; the current implementation uses encrypt = false during
// the initial pairing handshake.
#define ESPNOW_PMK   "Scale_PMK_Key_12"
#define ESPNOW_LMK   "Scale_LMK_Key_12"

#endif // CONFIG_H
