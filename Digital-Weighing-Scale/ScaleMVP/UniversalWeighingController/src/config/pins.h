/*
 * pins.h
 * ------------------------------------------------------------------------
 * Every GPIO assignment in the project lives here.
 *
 * Moving to a different ESP32 board, or re-routing the PCB, should require
 * editing this file and nothing else.
 *
 * Pin values below are taken from the validated bring-up sketches in
 * hardware_test/ and oled_test/.
 * ------------------------------------------------------------------------
 */

#ifndef PINS_H
#define PINS_H

#include <Arduino.h>
#include "hardware.h"

// ===================================================================
//  ADC PINS  — one complete pin set per load cell
// ===================================================================
//
// NOTE on GPIO34: it is an input-only pin on the ESP32, which is exactly
// what DOUT needs. Do not reuse it for SCLK/PDWN/SPEED/A0 (all outputs).
//
// If LOAD_CELL_COUNT is increased, add one entry to EVERY array below.

#ifdef USE_ADS1232

static constexpr uint8_t PIN_ADC_DOUT[LOAD_CELL_COUNT]  = { 34 };
static constexpr uint8_t PIN_ADC_SCLK[LOAD_CELL_COUNT]  = { 25 };
static constexpr uint8_t PIN_ADC_PDWN[LOAD_CELL_COUNT]  = { 26 };
static constexpr uint8_t PIN_ADC_SPEED[LOAD_CELL_COUNT] = { 27 };
static constexpr uint8_t PIN_ADC_A0[LOAD_CELL_COUNT]    = { 14 };

#endif // USE_ADS1232

#ifdef USE_HX711

// The HX711 needs only two wires: DT (data out) and SCK (PD_SCK clock).
// There is no PDWN, SPEED or A0 — power-down and gain are both done by
// clocking SCK, and the sample rate is a hardware pin on the module.
//
// GPIO34 is input-only, which is exactly right for DT. It also has no
// internal pull-up, but that is fine: the HX711 drives DT push-pull. If DT
// reads permanently HIGH, the module is unpowered or the wire is open.
static constexpr uint8_t PIN_ADC_DOUT[LOAD_CELL_COUNT] = { 34 };   // DT
static constexpr uint8_t PIN_ADC_SCLK[LOAD_CELL_COUNT] = { 25 };   // SCK

#endif // USE_HX711

// ===================================================================
//  SPI BUS  — shared by the MAX7219 display and the nRF24L01+
// ===================================================================
//
// These are the ESP32's default VSPI pins. Both devices hang off the same
// three lines and are told apart by their chip-select pins alone; SpiBus.h
// explains why that is safe here.
//
// GPIO19 (MISO) is only ever driven by the radio — the MAX7219 has no data
// output at all — so there is no bus contention to arbitrate.
#if defined(DISPLAY_7SEG) || defined(ENABLE_NRF)
#define PIN_SPI_SCK  18
#define PIN_SPI_MOSI 23
#define PIN_SPI_MISO 19
#endif

// ===================================================================
//  DISPLAY PINS
// ===================================================================

#ifdef DISPLAY_OLED
// SH1106 128x64 over I2C.
#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22
#define OLED_I2C_ADDRESS 0x3C
#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#endif

#ifdef DISPLAY_7SEG
// MAX7219 over SPI. DIN and CLK are the shared bus lines above and are
// listed again here only so the wiring diagram can be read off one place;
// the driver uses PIN_SPI_MOSI / PIN_SPI_SCK and only PIN_MAX7219_CS is
// really its own.
#define PIN_MAX7219_DIN PIN_SPI_MOSI   // GPIO23
#define PIN_MAX7219_CLK PIN_SPI_SCK    // GPIO18
#define PIN_MAX7219_CS  5
#endif

// ===================================================================
//  KEYPAD PINS  — 4x4 matrix
// ===================================================================
//
//        C0(12) C1(13) C2(14) C3(15)
//  R0(32)   1      2      3      A
//  R1(33)   4      5      6      B
//  R2(26)   7      8      9      C
//  R3(27)   *      0      #      D
//
// SCAN DIRECTION IS NOT ARBITRARY: rows are driven LOW one at a time and
// columns are read with the internal pull-ups, so a pressed key reads LOW.
// It has to be this way round because of GPIO12 — see below.
//
// GPIO12 (MTDI) IS A STRAPPING PIN. If it is HIGH at reset the ESP32
// selects a 1.8 V flash supply and the board does not boot. Two rules
// follow, and breaking either gives a board that boots only sometimes:
//
//   1. NEVER fit external pull-up resistors to the column lines. The
//      internal pull-ups this driver enables are applied long after reset
//      and are harmless; an external one is present *during* reset.
//   2. Columns must be the inputs and rows the outputs. With rows idle
//      (high-impedance until the first scan) a key held down at power-up
//      cannot drag GPIO12 high — MTDI's internal pull-DOWN wins.
//
// GPIO15 (MTDO) is also a strapping pin, but only cosmetically: held low at
// reset it silences the boot ROM log on U0TXD.
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4

static const uint8_t PIN_KEYPAD_ROW[KEYPAD_ROWS] = { 32, 33, 26, 27 };
static const uint8_t PIN_KEYPAD_COL[KEYPAD_COLS] = { 12, 13, 14, 15 };

// ===================================================================
//  WIRELESS PINS
// ===================================================================
// The nRF24L01+ sits on the shared SPI bus above and needs two GPIOs of its
// own: CE (transmit/listen strobe) and CSN (chip select).
//
// These are NOT the dashboard's radio pins (it uses GPIO4/GPIO15). The two
// boards are separate products with separate layouts; only the SPI bus lines
// and the radio *settings* in hardware.h have to match, never the GPIOs.
//
// VCC must be 3.3 V. 5 V destroys the module.
//
// IRQ is deliberately left unconnected — the driver polls STATUS, which at
// a few packets per second costs less than an interrupt would.
#ifdef ENABLE_NRF
#define PIN_NRF_CE  16
#define PIN_NRF_CSN 17
#endif

#endif // PINS_H
