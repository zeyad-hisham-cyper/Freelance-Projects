/*
 * MatrixKeypad.cpp
 * ------------------------------------------------------------------------
 * See MatrixKeypad.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "MatrixKeypad.h"

// The keycaps, in the physical arrangement drawn in pins.h.
static const char KEYCAP[KEYPAD_ROWS][KEYPAD_COLS] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' }
};

MatrixKeypad::MatrixKeypad()
    : _queueHead(0), _queueCount(0),
      _stableKey(NO_KEY), _candidateKey(NO_KEY),
      _candidateSinceMs(0), _pressedAtMs(0), _lastRepeatMs(0),
      _suppressHeld(false) {
    for (uint8_t i = 0; i < QUEUE_SIZE; ++i) {
        _queue[i] = NO_KEY;
    }
}

char MatrixKeypad::keyAt(uint8_t row, uint8_t col) {
    if (row >= KEYPAD_ROWS || col >= KEYPAD_COLS) {
        return NO_KEY;
    }
    return KEYCAP[row][col];
}

void MatrixKeypad::begin() {
    // Rows idle HIGH: only the row being scanned is pulled low, so a key
    // press can never connect two driven outputs together.
    for (uint8_t r = 0; r < KEYPAD_ROWS; ++r) {
        pinMode(PIN_KEYPAD_ROW[r], OUTPUT);
        digitalWrite(PIN_KEYPAD_ROW[r], HIGH);
    }

    // Internal pull-ups only. An external pull-up on the GPIO12 column would
    // stop the board booting — see pins.h.
    for (uint8_t c = 0; c < KEYPAD_COLS; ++c) {
        pinMode(PIN_KEYPAD_COL[c], INPUT_PULLUP);
    }

    _stableKey        = NO_KEY;
    _candidateKey     = NO_KEY;
    _candidateSinceMs = millis();
    _queueHead        = 0;
    _queueCount       = 0;
    _suppressHeld     = false;
}

char MatrixKeypad::scan() {
    char found = NO_KEY;

    for (uint8_t r = 0; r < KEYPAD_ROWS && found == NO_KEY; ++r) {
        digitalWrite(PIN_KEYPAD_ROW[r], LOW);

        // The column lines have only the ESP32's weak internal pull-up
        // (~45 kOhm) against the keypad's ribbon capacitance, so give the
        // line a moment to actually reach the level before sampling it.
        // Without this the first column read after a row change can return
        // the previous row's state.
        delayMicroseconds(5);

        for (uint8_t c = 0; c < KEYPAD_COLS; ++c) {
            if (digitalRead(PIN_KEYPAD_COL[c]) == LOW) {
                found = KEYCAP[r][c];
                break;
            }
        }

        digitalWrite(PIN_KEYPAD_ROW[r], HIGH);
    }

    return found;
}

void MatrixKeypad::update() {
    uint32_t now = millis();
    char     raw = scan();

    // --- Debounce ------------------------------------------------------
    // A reading has to persist before it is believed. This covers both the
    // contact bounce on press and the brief all-open moment when a finger
    // slides from one key to its neighbour.
    if (raw != _candidateKey) {
        _candidateKey     = raw;
        _candidateSinceMs = now;
        return;
    }

    if ((now - _candidateSinceMs) < KEYPAD_DEBOUNCE_MS) {
        return;
    }

    // --- Accept the debounced state ------------------------------------
    if (raw != _stableKey) {
        _stableKey = raw;

        if (_stableKey != NO_KEY) {
            _pressedAtMs  = now;
            _lastRepeatMs = now;

            // Fires on the PRESS edge, not the release. With sixteen keys
            // there is no short-versus-long ambiguity to resolve, and a
            // keypad that responds when you push it feels immediate in a
            // way that one waiting for the release does not.
            if (!_suppressHeld) {
                enqueue(_stableKey);
            }
        } else {
            // Released: whatever flush() was suppressing is over.
            _suppressHeld = false;
        }
        return;
    }

    // --- Auto-repeat while held ----------------------------------------
    // Mostly for backspace, which the operator will want to hold down to
    // clear a mis-typed weight.
    if (_stableKey != NO_KEY && !_suppressHeld) {
        if ((now - _pressedAtMs) >= KEYPAD_REPEAT_DELAY_MS &&
            (now - _lastRepeatMs) >= KEYPAD_REPEAT_RATE_MS) {
            _lastRepeatMs = now;
            enqueue(_stableKey);
        }
    }
}

void MatrixKeypad::enqueue(char key) {
    if (_queueCount >= QUEUE_SIZE) {
        // Full. Drop the OLDEST, not the newest: if the loop has stalled
        // badly enough to overflow this, the operator's most recent
        // keystrokes are the ones they still expect to see.
        _queueHead = (uint8_t)((_queueHead + 1) % QUEUE_SIZE);
        --_queueCount;
    }

    _queue[(_queueHead + _queueCount) % QUEUE_SIZE] = key;
    ++_queueCount;
}

char MatrixKeypad::read() {
    if (_queueCount == 0) {
        return NO_KEY;
    }

    char key   = _queue[_queueHead];
    _queueHead = (uint8_t)((_queueHead + 1) % QUEUE_SIZE);
    --_queueCount;
    return key;
}

void MatrixKeypad::flush() {
    _queueHead  = 0;
    _queueCount = 0;

    // A key still down when the screen changed belongs to the old screen.
    // Suppress its repeats until it is released, or the new screen inherits
    // a stream of presses nobody asked for.
    _suppressHeld = (_stableKey != NO_KEY);
}
