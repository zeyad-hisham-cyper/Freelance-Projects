/*
 * hardware.h
 * ------------------------------------------------------------------------
 * Describes WHICH hardware is fitted to this particular controller:
 * how many load cells, which ADC, which display, and the tuning constants
 * that depend on the mechanical build (filter length, stability window).
 *
 * Changing the scale hardware should only ever require edits to this file
 * plus pins.h — never to modules/ or drivers/.
 * ------------------------------------------------------------------------
 */

#ifndef HARDWARE_H
#define HARDWARE_H

// ===================================================================
//  ADC SELECTION  (exactly one)
// ===================================================================
//#define USE_ADS1232
#define USE_HX711

// ===================================================================
//  DISPLAY SELECTION  (exactly one)
// ===================================================================
#define DISPLAY_OLED
//#define DISPLAY_7SEG

// ===================================================================
//  LOAD CELL CONFIGURATION
// ===================================================================

// Number of load cells, each with its own dedicated ADC.
// Supported values: 1, 2, 4 (pins.h must list this many pin sets).
#define LOAD_CELL_COUNT 1

// Nominal full-scale capacity of the *complete* platform, in grams.
// Used only for plausibility checks and overload detection, never for
// the measurement itself.
#define SCALE_CAPACITY_G 20000.0f

// Fraction of capacity above which the display shows an overload warning.
#define OVERLOAD_MARGIN 1.05f

// ===================================================================
//  ADC SETTINGS
// ===================================================================

// SPS_10 = low noise / slow, SPS_80 = fast / noisier.
// 10 SPS is the right default for a weighing indicator.
//
// ADS1232 only. The HX711's rate is fixed by the RATE pin on the module
// (tied low = 10 SPS on most breakout boards) and cannot be set in software,
// so this setting is ignored when USE_HX711 is selected.
#define ADC_SPEED_80SPS 0

// HX711 channel/gain, as the number of trailing clock pulses:
//   1 = channel A, gain 128  <- the load-cell input, correct for weighing
//   2 = channel B, gain 32   <- auxiliary input
//   3 = channel A, gain 64
#define HX711_GAIN_SETTING 1

// How long to wait for a conversion before declaring the ADC faulty.
// At 10 SPS a conversion arrives every ~100 ms, so this is generous.
#define ADC_READ_TIMEOUT_MS 400

// ===================================================================
//  FILTERING  (see FilterManager)
// ===================================================================

// Moving-average window, in samples, applied per load cell.
// At 10 SPS a window of 8 costs ~0.8 s of settling time.
// Larger = steadier reading, slower response.
#define FILTER_WINDOW_SIZE 24

// ===================================================================
//  STABILITY DETECTION
// ===================================================================
// The reading is called "stable" when the last STABILITY_WINDOW filtered
// samples span less than STABILITY_THRESHOLD_COUNTS raw ADC counts.
//
// Expressed in COUNTS rather than grams on purpose: stability must work
// before the scale has ever been calibrated (it is needed *during*
// calibration), so it cannot depend on the calibration factor.
//
// Tune it by watching the raw counts on the serial monitor at rest.
#define STABILITY_WINDOW 6
#define STABILITY_THRESHOLD_COUNTS 400L

// ===================================================================
//  SEVEN-SEGMENT DISPLAY  (MAX7219, DISPLAY_7SEG only)
// ===================================================================

// Digits physically fitted to the module, 1..8. The driver sets the chip's
// scan limit from this: scanning digits that are not there wastes duty
// cycle and makes the fitted ones dimmer.
#define MAX7219_DIGITS 6

// 0 = dimmest (1/32 duty), 15 = brightest (31/32). 8 is a sensible indoor
// default; a panel behind smoked acrylic usually wants 12-15.
#define MAX7219_BRIGHTNESS 8

// Which end of the panel the chip's DIG0 output drives.
//
// 0 = DIG0 is the LEFTMOST digit
// 1 = DIG0 is the RIGHTMOST digit  (what most breakouts do)
//
// Set to 0 to match the module measured on the Wireless Dashboard, where a
// value of 1 printed "123456" as "654321". If the panel fitted to THIS
// board is a different module, check it and flip this define — nothing else
// in the firmware needs to change.
#define MAX7219_DIGIT0_IS_RIGHTMOST 0

// SPI clock for the display.
//
// The chip is rated to 10 MHz, but the real limit is the 3.3 V -> 5 V level
// mismatch (see MAX7219.h). 8 MHz over jumper leads was enough to corrupt
// the occasional word on the dashboard hardware, which lights every segment
// when the bad word lands on the display-test register. 1 MHz redraws six
// digits in ~100 us and is far more tolerant. Do not raise it without a
// level shifter fitted.
#define MAX7219_SPI_HZ 1000000

// How often the driver re-sends every control register.
//
// The MAX7219 cannot be read back, so a control register corrupted by one
// mis-clocked word stays corrupted until power is cycled. Re-asserting them
// once a second turns a dead panel into a flicker. 0 disables it.
#define MAX7219_CONFIG_REFRESH_MS 1000

// ===================================================================
//  WIRELESS  (nRF24L01+, ENABLE_NRF only)
// ===================================================================
//
// EVERY VALUE IN THIS SECTION MUST MATCH THE WIRELESS DASHBOARD'S
// hardware.h. The dashboard is a separate firmware in ../WirelessDashboard,
// so nothing checks this for you. Two boards that disagree about the
// channel, the addresses, the data rate or the payload size will each report
// a perfectly healthy radio and never exchange a single packet.

// Datasheet maximum is 10 MHz; matched to the display so the shared bus
// runs at one speed.
#define NRF_SPI_HZ 8000000

// RF channel 0..125, giving 2400 + n MHz.
//
// This was 108 (2508 MHz), chosen to sit above the WiFi band. That was a
// mistake: the 2.4 GHz ISM band ENDS at 2483.5 MHz, so 108 is out of band —
// not legal to radiate in, and outside what the module's antenna and
// matching network are tuned for. The chip will happily transmit there and
// simply perform worse.
//
// 76 (2476 MHz) is inside the band, above the centre of WiFi channel 11,
// and is the long-standing default in the field. Legal range here is 0..83.
#define NRF_CHANNEL 76

// 0 = 1 Mbps, 1 = 2 Mbps, 2 = 250 kbps.
//
// 250 kbps buys roughly 3x the range and would be the right choice on
// genuine Nordic silicon. It is NOT the safe default, because a large share
// of the modules sold as "nRF24L01+" are Si24R1 clones whose 250 kbps mode
// is unreliable or absent — and a mixed pair (one genuine, one clone) fails
// exactly like a marginal link: most packets lost, a few getting through.
//
// 1 Mbps works on every module ever sold. Move to 250 kbps only once the
// link is solid at 1 Mbps and you need the range, and change BOTH boards.
#define NRF_DATA_RATE 0

// Transmit power: 0 = -18 dBm, 1 = -12 dBm, 2 = -6 dBm, 3 = 0 dBm.
//
// Full power is also the highest current draw — see the capacitor warning
// in NRF24.h before blaming the firmware for dropped packets.
//
// COUNTER-INTUITIVE BENCH NOTE: two modules sitting less than a metre apart
// at 0 dBm can lose packets because the receiver is being overloaded, not
// starved. If the link is poor with both boards on the same desk, drop this
// to 0 or 1 and see whether it IMPROVES before assuming it is a range
// problem.
#define NRF_PA_LEVEL 3

// Address width in bytes (3..5). Wider addresses reject stray traffic from
// other nRF24 installations better.
#define NRF_ADDRESS_WIDTH 5

// Both ends must agree on these. The last byte differs so a future second
// dashboard can be given its own pipe without touching the rest.
#define NRF_ADDRESS_SCALE     { 'S', 'C', 'A', 'L', '1' }   // scale  -> dashboard
#define NRF_ADDRESS_DASHBOARD { 'S', 'C', 'A', 'L', '2' }   // dashboard -> scale

// Fixed payload size. Must be >= sizeof(WeightPacket); WirelessManager
// static-asserts this so a grown packet cannot be silently truncated.
#define NRF_PAYLOAD_SIZE 20

// Auto-retransmit. Delay is in units of 250 us (0..15), count is 0..15.
// At 250 kbps the far end needs a slow turnaround, so the delay must be
// generous: 5 -> 1500 us, retried 5 times, is ~9 ms worst case.
#define NRF_RETRY_DELAY 5
#define NRF_RETRY_COUNT 5

// How often the scale broadcasts. Faster than the display refresh is
// pointless, and every transmission is current drawn from the 3.3 V rail.
#define WIRELESS_TX_INTERVAL_MS 200

// A link is "up" while traffic has succeeded within this window. Must be
// several transmit intervals so one lost packet does not report a fault.
#define WIRELESS_LINK_TIMEOUT_MS 2000

// ===================================================================
//  DISPLAY / UI TIMING
// ===================================================================
#define DISPLAY_REFRESH_MS 150   // OLED redraw interval

// ---- 4x4 keypad ----------------------------------------------------
// A membrane keypad bounces more than a good tactile switch, so the
// debounce is longer than the 30 ms the push buttons used. Raise it if a
// single press ever registers twice; lower it if fast typing drops digits.
#define KEYPAD_DEBOUNCE_MS 25

// Auto-repeat while a key is held. Mainly so backspace can be held down to
// clear a mis-typed reference mass — the delay is long enough that an
// ordinary press never repeats by accident.
#define KEYPAD_REPEAT_DELAY_MS 600
#define KEYPAD_REPEAT_RATE_MS 130

#endif // HARDWARE_H
