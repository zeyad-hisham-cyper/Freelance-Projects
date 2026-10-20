/*
 * MatrixKeypad.h
 * ------------------------------------------------------------------------
 * Scanning driver for the 4x4 membrane keypad.
 *
 * Produces a stream of debounced key-press events. It reports WHICH KEY was
 * pressed and nothing about what that key means — the mapping from '7' or
 * 'A' to a scale function lives one layer up, in InputManager.
 *
 * SINGLE KEY AT A TIME, ON PURPOSE:
 * A membrane keypad has no diodes, so pressing two keys that share a row and
 * a column makes a third, phantom key appear — classic matrix ghosting.
 * Rather than pretend otherwise, the scan reports the first key it finds and
 * ignores the rest until the operator lets go. Nothing in a weighing
 * indicator needs chords, and a driver that silently invents keypresses is
 * far worse than one that drops a simultaneous second press.
 *
 * EVENTS ARE QUEUED, NOT LATCHED:
 * A single boolean "a key was pressed" flag loses the second of two presses
 * that land in the same loop pass. The queue below cannot, which matters for
 * typing digits quickly.
 *
 * DEBOUNCE: a detected key must read the same for KEYPAD_DEBOUNCE_MS before
 * it is accepted, and holding a key repeats it — so backspace can be held
 * down to clear an entry.
 *
 * NO BLOCKING DELAYS. update() scans all four rows and returns; at the
 * ~10 us the ESP32 needs per row that is invisible next to the ADC.
 * ------------------------------------------------------------------------
 */

#ifndef MATRIX_KEYPAD_H
#define MATRIX_KEYPAD_H

#include <Arduino.h>
#include "../config/config.h"

class MatrixKeypad {
public:
    // Nothing pressed / nothing queued.
    static const char NO_KEY = '\0';

    MatrixKeypad();

    // Configures rows as outputs (idle HIGH) and columns as inputs with
    // pull-ups. See the strapping-pin warning in pins.h before changing the
    // direction of either.
    void begin();

    // Scans the matrix and queues any new press. Call every loop.
    void update();

    // Removes and returns the oldest queued press, or NO_KEY if none.
    char read();

    bool available() const { return _queueCount > 0; }

    // The key currently held down, or NO_KEY. Useful for "is the operator
    // still holding this" checks; ordinary input should use read().
    char heldKey() const { return _stableKey; }

    // Discards queued presses and suppresses the repeat of a key that is
    // still held — used when switching UI modes so a press meant for the
    // previous screen is not replayed on the new one.
    void flush();

    // The keycap at a matrix position. Static so a bring-up sketch can print
    // the layout without instantiating the driver.
    static char keyAt(uint8_t row, uint8_t col);

private:
    // Eight is far more than a human can generate between two loop passes;
    // it exists so a momentarily slow loop cannot drop a digit.
    static const uint8_t QUEUE_SIZE = 8;

    char     _queue[QUEUE_SIZE];
    uint8_t  _queueHead;
    uint8_t  _queueCount;

    char     _stableKey;        // debounced key currently down
    char     _candidateKey;     // what the last scan saw
    uint32_t _candidateSinceMs; // when the scan first saw it
    uint32_t _pressedAtMs;      // when _stableKey went down
    uint32_t _lastRepeatMs;
    bool     _suppressHeld;     // set by flush() while a key is still down

    // One pass over the matrix. Returns the first key found, or NO_KEY.
    char scan();

    void enqueue(char key);
};

#endif  // MATRIX_KEYPAD_H
