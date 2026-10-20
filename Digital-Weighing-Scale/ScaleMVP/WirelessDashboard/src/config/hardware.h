/*
 * hardware.h
 * ------------------------------------------------------------------------
 * Describes the hardware fitted to THIS dashboard: how many digits, how
 * bright, and the radio settings.
 *
 * EVERY VALUE IN THE "WIRELESS" SECTION MUST MATCH THE SCALE CONTROLLER'S
 * hardware.h. Two boards that disagree about the channel, the addresses, the
 * data rate or the payload size will each report a perfectly healthy radio
 * and never exchange a single packet — the most confusing failure this
 * system can produce. When in doubt, diff the two sections.
 * ------------------------------------------------------------------------
 */

#ifndef HARDWARE_H
#define HARDWARE_H

// ===================================================================
//  SEVEN-SEGMENT DISPLAY  (MAX7219)
// ===================================================================

// Digits physically fitted to the module, 1..8. The driver sets the chip's
// scan limit from this: scanning digits that are not there wastes duty
// cycle and makes the fitted ones dimmer.
#define MAX7219_DIGITS 6

// 0 = dimmest (1/32 duty), 15 = brightest (31/32). A dashboard is usually
// read from across a room, so it wants more than a bench indicator.
#define MAX7219_BRIGHTNESS 12

// Which end of the panel the chip's DIG0 output drives.
//
// 0 = DIG0 is the LEFTMOST digit   <- measured on this hardware
// 1 = DIG0 is the RIGHTMOST digit  (what most breakouts do)
//
// This started at 1 on the assumption that the module followed the common
// wiring. It does not: "123456" came out as "654321". One define, and every
// position in the driver API is counted from the left again.
#define MAX7219_DIGIT0_IS_RIGHTMOST 0

// SPI clock for the display.
//
// The chip is rated to 10 MHz, but the real limit here is the 3.3 V -> 5 V
// level mismatch (see MAX7219.h), and 8 MHz over jumper leads was enough to
// corrupt the occasional word — which is what lights every segment when the
// bad word lands on the display-test register.
//
// 1 MHz is 16 us per register write, ~100 us to redraw six digits at a
// 150 ms refresh: nowhere near a bottleneck, and far more tolerant of
// marginal levels and untidy wiring. Do not raise it without a level
// shifter fitted.
#define MAX7219_SPI_HZ 1000000

// How often the driver re-sends every control register.
//
// The MAX7219 cannot be read back, so a control register corrupted by one
// mis-clocked word stays corrupted until power is cycled — the panel locks
// on all-segments-lit or freezes on its last value. Re-asserting them once
// a second turns that from a dead panel into a flicker. 0 disables it.
#define MAX7219_CONFIG_REFRESH_MS 1000

// Run the lamp/segment/count self-test at every power-up.
//
// OFF by default, and deliberately so: the self-test opens by lighting every
// segment of every digit, which is exactly what a corrupted display-test
// register looks like. With it off, "the whole panel is lit" has one meaning
// instead of two, and a board stuck in a brown-out reset loop is obvious
// rather than looking like a slow lamp test.
//
// Turn it on for bench work — it is still the only way to prove every
// segment of a write-only panel, and to check the digit order above.
#define DISPLAY_SELFTEST_ON_BOOT 0

// ===================================================================
//  WIRELESS  (nRF24L01+)  — MUST MATCH THE SCALE CONTROLLER
// ===================================================================

#define NRF_SPI_HZ 8000000

// RF channel 0..125, giving 2400 + n MHz.
//
// This was 108 (2508 MHz), chosen to sit above the WiFi band. That was a
// mistake: the 2.4 GHz ISM band ENDS at 2483.5 MHz, so 108 is out of band —
// not legal to radiate in, and outside what the module's antenna and
// matching network are tuned for. The chip transmits there happily and
// simply performs worse.
//
// 76 (2476 MHz) is inside the band and is the long-standing field default.
// Legal range here is 0..83.
#define NRF_CHANNEL 76

// 0 = 1 Mbps, 1 = 2 Mbps, 2 = 250 kbps.
//
// 250 kbps buys roughly 3x the range and would be right on genuine Nordic
// silicon. It is NOT the safe default: a large share of modules sold as
// "nRF24L01+" are Si24R1 clones whose 250 kbps mode is unreliable or
// absent, and a mixed pair fails exactly like a marginal link — most
// packets lost, a few getting through.
//
// 1 Mbps works on every module ever sold. Move up only once the link is
// solid, and change BOTH boards together.
#define NRF_DATA_RATE 0

// 0 = -18 dBm, 1 = -12 dBm, 2 = -6 dBm, 3 = 0 dBm. The receiver transmits
// only auto-acknowledges, but they still have to reach the scale.
//
// COUNTER-INTUITIVE BENCH NOTE: two modules less than a metre apart at
// 0 dBm can lose packets because the receiver is overloaded, not starved.
// On a desk, try dropping this to 0 or 1 and see whether it IMPROVES.
#define NRF_PA_LEVEL 3

#define NRF_ADDRESS_WIDTH 5

// The dashboard LISTENS on the scale's address and acknowledges from its
// own — the mirror image of the scale's configuration.
#define NRF_ADDRESS_SCALE     { 'S', 'C', 'A', 'L', '1' }
#define NRF_ADDRESS_DASHBOARD { 'S', 'C', 'A', 'L', '2' }

// Fixed payload size. Must be >= sizeof(WeightPacket) and identical on both
// boards; WirelessManager static-asserts the first half of that.
#define NRF_PAYLOAD_SIZE 20

// Auto-retransmit settings. A receiver never initiates a transmission, so
// these only shape the acknowledge timing, but the registers still have to
// be programmed the same on both ends.
#define NRF_RETRY_DELAY 5
#define NRF_RETRY_COUNT 5

// ===================================================================
//  LINK TIMING
// ===================================================================

// How long a received reading stays on the panel before it is declared
// stale. Must be several of the scale's transmit intervals (200 ms by
// default) so one lost packet does not blank the display.
//
// This is the single most important number in this file: it is the
// difference between "the scale reads 4.2 kg" and "the scale read 4.2 kg
// at some point in the past".
#define LINK_TIMEOUT_MS 2000

// ===================================================================
//  UI TIMING
// ===================================================================
#define DISPLAY_REFRESH_MS 150   // panel redraw interval
#define SERIAL_LOG_MS 1000       // diagnostic line interval

#endif // HARDWARE_H
