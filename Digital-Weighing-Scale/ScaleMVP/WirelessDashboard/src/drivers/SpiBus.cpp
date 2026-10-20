/*
 * SpiBus.cpp
 * ------------------------------------------------------------------------
 * SHARED FILE. An identical copy lives in the other firmware:
 *      UniversalWeighingController/src/drivers/SpiBus.cpp
 *      WirelessDashboard/src/drivers/SpiBus.cpp
 * They are separate Arduino sketches and cannot share a directory. Edit
 * both together; diff them whenever the two boards stop understanding
 * each other.
 * ------------------------------------------------------------------------
 * See SpiBus.h for the bus-sharing rules.
 * ------------------------------------------------------------------------
 */

#include "SpiBus.h"

#if defined(DISPLAY_7SEG) || defined(ENABLE_NRF)

namespace {
bool g_started = false;
}

void SpiBus::begin() {
    if (g_started) {
        return;
    }

    // Deselect EVERY device on the bus before the bus exists.
    //
    // This is not belt-and-braces. Each driver raises its own CS in its
    // begin(), but the drivers are initialised one after another, so
    // whichever goes first starts clocking data while the other device's
    // chip select is still an undriven input. Neither the MAX7219's CS nor
    // the nRF24's CSN has a pull-up, so "undriven" can sit low: the idle
    // device then latches its neighbour's traffic as commands. On the radio
    // that means a scrambled configuration; on the display it means a
    // control register — display test, shutdown, scan limit — written with
    // whatever happened to be on the wire.
    //
    // Owning this here is the bus's job, and it is the only place that runs
    // before any traffic regardless of which driver initialises first.
#ifdef PIN_MAX7219_CS
    pinMode(PIN_MAX7219_CS, OUTPUT);
    digitalWrite(PIN_MAX7219_CS, HIGH);
#endif
#ifdef PIN_NRF_CSN
    pinMode(PIN_NRF_CSN, OUTPUT);
    digitalWrite(PIN_NRF_CSN, HIGH);
#endif
#ifdef PIN_NRF_CE
    // Not a chip select, but a floating CE on a powered radio can put it
    // into receive or transmit before it has been configured.
    pinMode(PIN_NRF_CE, OUTPUT);
    digitalWrite(PIN_NRF_CE, LOW);
#endif

    // SS = -1: neither device uses the hardware chip select; both drive
    // their CS line themselves so the bus can be shared.
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

    g_started = true;
}

bool SpiBus::isReady() {
    return g_started;
}

#endif  // DISPLAY_7SEG || ENABLE_NRF
