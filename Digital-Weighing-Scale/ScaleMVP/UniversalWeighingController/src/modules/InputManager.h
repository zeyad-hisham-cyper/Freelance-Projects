/*
 * InputManager.h
 * ------------------------------------------------------------------------
 * The front panel, as the application sees it.
 *
 * MatrixKeypad knows how to find out that the '7' key is down. This module
 * owns the other half: which keycap means what. That split is why the
 * keypad replaced three push buttons without the driver knowing anything
 * about weighing, and why re-labelling the panel is an edit to one table
 * here rather than a hunt through App.
 *
 * KEY MAP
 *
 *      1 2 3 A        A = TARE    zero the platform / clear the tare
 *      4 5 6 B        B = UNIT    cycle g -> kg -> lb -> oz
 *      7 8 9 C        C = ENTER   start calibration, and confirm each step
 *      * 0 # D        D = CANCEL  abort calibration / clear the tare
 *
 *      0-9 = digits, during the calibration VALUE step
 *      *   = decimal point
 *      #   = backspace (hold to clear)
 *
 * WHAT HAPPENED TO LONG PRESSES:
 * With three buttons, "clear the tare" and "abort calibration" had to be
 * long presses — there were not enough buttons to give them their own. A
 * keypad has sixteen, so both are now the CANCEL key. A held key no longer
 * means anything different from a tapped one, which removes the entire
 * short-versus-long timing question and the class of bugs that came with it.
 * ------------------------------------------------------------------------
 */

#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"
#include "../drivers/MatrixKeypad.h"

class InputManager {
public:
    // Function keys, named so App never spells out a keycap.
    static const char KEY_TARE   = 'A';
    static const char KEY_UNIT   = 'B';
    static const char KEY_ENTER  = 'C';
    static const char KEY_CANCEL = 'D';
    static const char KEY_POINT  = '*';
    static const char KEY_DELETE = '#';

    static const char KEY_NONE = MatrixKeypad::NO_KEY;

    void begin();
    void update();

    // Consumes the next key press. Returns false when there are none left,
    // so the caller drains it with a while loop and never loses a keystroke
    // to a slow pass through the main loop.
    bool nextKey(char &key);

    // Drops pending keys — call when the UI changes what a key would mean.
    void flush();

    // True for '0'..'9'. `value` receives the numeric value.
    static bool isDigit(char key, uint8_t &value);

private:
    MatrixKeypad _keypad;
};

#endif  // INPUT_MANAGER_H
