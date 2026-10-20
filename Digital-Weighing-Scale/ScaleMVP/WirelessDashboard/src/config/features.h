/*
 * features.h
 * ------------------------------------------------------------------------
 * Compile-time feature switches for the Wireless Dashboard.
 *
 * NOTE ON THE FIRST TWO SWITCHES: a dashboard is a MAX7219 panel plus an
 * nRF24 receiver by definition, so neither is really optional here. They
 * exist, and are named exactly as they are in the Scale Controller project,
 * so that the three shared driver files
 *
 *      drivers/SpiBus.*  drivers/MAX7219.*  drivers/NRF24.*
 *
 * stay byte-for-byte identical between the two firmwares and can be diffed.
 * Do not delete them to "clean up" — that is what breaks the shared copies.
 * ------------------------------------------------------------------------
 */

#ifndef FEATURES_H
#define FEATURES_H

// ------------------------------------------------------- Fitted hardware
#define DISPLAY_7SEG   // MAX7219 seven-segment panel
#define ENABLE_NRF     // nRF24L01+ receiver

// --------------------------------------------------------------- Diagnostics
// Classify link and radio faults and put them on the panel. With this off
// the dashboard shows the last received value forever, which is exactly the
// failure mode a remote display must not have — leave it on.
#define ENABLE_DIAGNOSTICS

// -------------------------------------------------------------------- Debug
// Serial logging at 115200 baud. Costs Flash and slows the loop; disable for
// production panels that no one will ever plug a laptop into.
#define DEBUG_SERIAL

#ifdef DEBUG_SERIAL
    #define LOG(x)        Serial.print(x)
    #define LOGLN(x)      Serial.println(x)
    #define LOGF(x, y)    Serial.print(x, y)
    #define LOGFLN(x, y)  Serial.println(x, y)
#else
    #define LOG(x)
    #define LOGLN(x)
    #define LOGF(x, y)
    #define LOGFLN(x, y)
#endif

#endif // FEATURES_H
