/*
 * SpiBus.h
 * ------------------------------------------------------------------------
 * SHARED FILE. An identical copy lives in the other firmware:
 *      UniversalWeighingController/src/drivers/SpiBus.h
 *      WirelessDashboard/src/drivers/SpiBus.h
 * They are separate Arduino sketches and cannot share a directory. Edit
 * both together; diff them whenever the two boards stop understanding
 * each other.
 * ------------------------------------------------------------------------
 * One-time bring-up of the SPI bus that the MAX7219 and the nRF24L01+ share.
 *
 * WHY THIS FILE EXISTS:
 * Both devices hang off the same SCK / MOSI / MISO lines, so exactly one of
 * them must call SPI.begin() — whichever happens to initialise first. Rather
 * than making one driver depend on the other, both call SpiBus::begin(),
 * which is idempotent.
 *
 * BUS SHARING RULES (both drivers obey them; break them and you get garbled
 * digits or a radio that silently stops answering):
 *
 *   1. Every access is wrapped in SPI.beginTransaction()/endTransaction()
 *      with the device's own SPISettings. The two chips want different
 *      clocks, and the transaction API is what keeps them from stealing
 *      each other's settings.
 *
 *   2. Chip selects idle HIGH and are driven low only inside a transaction.
 *      The MAX7219 latches its 16-bit word on the RISING edge of CS, so it
 *      simply ignores every clock edge meant for the radio, and vice versa.
 *
 *   3. The MAX7219 has no data output, so it never fights the radio for
 *      MISO. This is the only reason a 5 V display can share a bus with a
 *      3.3 V radio at all.
 *
 * SPI.begin() is called with SS = -1: the hardware chip-select is left
 * unused because each driver owns its own CS pin as a plain GPIO.
 * ------------------------------------------------------------------------
 */

#ifndef SPI_BUS_H
#define SPI_BUS_H

#include <Arduino.h>
#include "../config/config.h"

#if defined(DISPLAY_7SEG) || defined(ENABLE_NRF)

#include <SPI.h>

namespace SpiBus {

// Brings up the VSPI peripheral on the pins in pins.h, after driving every
// device's chip select inactive. Safe to call from every driver's begin();
// only the first call does any work.
void begin();

bool isReady();

}  // namespace SpiBus

#endif  // DISPLAY_7SEG || ENABLE_NRF
#endif  // SPI_BUS_H
