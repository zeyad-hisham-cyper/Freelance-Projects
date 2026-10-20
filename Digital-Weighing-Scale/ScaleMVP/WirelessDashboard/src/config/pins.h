/*
 * pins.h
 * ------------------------------------------------------------------------
 * Every GPIO assignment in the dashboard lives here.
 *
 * Moving to a different ESP32 board, or re-routing the PCB, should require
 * editing this file and nothing else.
 * ------------------------------------------------------------------------
 */

#ifndef PINS_H
#define PINS_H

#include <Arduino.h>
#include "hardware.h"

// ===================================================================
//  SPI BUS  — shared by the MAX7219 display and the nRF24L01+
// ===================================================================
//
// The ESP32's default VSPI pins. Both devices hang off the same three lines
// and are told apart by their chip-select pins alone; SpiBus.h explains why
// that is safe.
//
// GPIO19 (MISO) is only ever driven by the radio — the MAX7219 has no data
// output at all — so there is no bus contention to arbitrate.
#define PIN_SPI_SCK  18
#define PIN_SPI_MOSI 23
#define PIN_SPI_MISO 19

// ===================================================================
//  DISPLAY  (MAX7219)
// ===================================================================
// DIN and CLK are the shared bus lines above; they are listed again so the
// wiring diagram can be read off one place. Only CS is really the display's
// own pin.
#define PIN_MAX7219_DIN PIN_SPI_MOSI   // GPIO23
#define PIN_MAX7219_CLK PIN_SPI_SCK    // GPIO18
#define PIN_MAX7219_CS  5

// ===================================================================
//  WIRELESS  (nRF24L01+)
// ===================================================================
// CE strobes transmit/listen, CSN is the chip select.
//
// These are NOT the scale controller's radio pins (it uses GPIO16/GPIO17).
// The two boards are separate products with separate layouts; only the SPI
// bus lines happen to coincide, and neither firmware assumes otherwise.
//
// NOTE on GPIO15: it is a strapping pin (MTDO). Held LOW at reset it
// silences the boot ROM log on U0TXD. That is harmless here — CSN idles
// HIGH, the driver drives it HIGH before the SPI bus comes up, and the
// nRF24's CSN input is high-impedance so nothing pulls it down in between.
// But if this board ever loses its boot log, this pin is the first suspect.
//
// VCC must be 3.3 V. 5 V destroys the module.
//
// IRQ is deliberately left unconnected — the driver polls, which at a few
// packets per second costs less than an interrupt would.
#define PIN_NRF_CE  4
#define PIN_NRF_CSN 15

#endif // PINS_H
