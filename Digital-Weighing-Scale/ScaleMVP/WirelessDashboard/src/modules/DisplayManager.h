/*
 * DisplayManager.h
 * ------------------------------------------------------------------------
 * Turns "what the dashboard knows" into "what the panel shows".
 *
 * The MAX7219 driver already owns the *layout* of a number on six digits
 * (see the design note in MAX7219.h). What this module owns is the layer
 * above that: which of the panel's few possible screens is the right one
 * right now, and how a received packet maps onto it.
 *
 * That is a real decision, not a pass-through, because a remote display has
 * a failure mode a local one does not: it can keep showing a number long
 * after the number stopped being true. Every method here exists so that
 * nothing but a fresh, fault-free reading can ever look like a reading.
 * ------------------------------------------------------------------------
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"
#include "../drivers/MAX7219.h"
#include "../protocol/WeightPacket.h"

class DisplayManager {
public:
    DisplayManager();

    // False means the digit count in hardware.h is impossible. The MAX7219
    // is write-only, so an unpowered or unwired module cannot be detected
    // here — runSelfTest() is how you find that out.
    bool begin();

    bool isReady() const { return _ready; }

    // Lamp test, then the firmware version.
    void showBoot();

    // "----" while no scale has been heard from yet.
    void showWaiting();

    // The reading itself: value in the packet's unit, NET marked with the
    // leftmost decimal point.
    void showReading(const WeightPacket &packet);

    // Fault screens. `title` is rendered as "Err" plus a two-letter tag.
    void showFault(const char *title);

    // Plain text, for anything that is not a fault (max 6 characters).
    void showMessage(const char *text);

    void setBrightness(uint8_t level);   // 0..15

    // Lamp test -> walking segment -> 0-9 on every digit. Blocking, ~3 s.
    // The only way to prove every segment of a panel that normally shows
    // the same handful of digits all day.
    void runSelfTest();

private:
    MAX7219 _panel;
    bool    _ready;
};

#endif  // DISPLAY_MANAGER_H
