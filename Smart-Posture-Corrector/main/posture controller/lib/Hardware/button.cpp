#include "button.h"

ButtonHandler::ButtonHandler(uint8_t pin, unsigned long longPressMs, unsigned long debounceMs)
    : _pin(pin),
      _longPressMs(longPressMs),
      _debounceMs(debounceMs),
      _lastRawState(HIGH),
      _stableState(HIGH),
      _lastDebounceMs(0),
      _pressed(false),
      _pressStartMs(0),
      _longReported(false) {
}

void ButtonHandler::begin() {
    /* Using Input Pull up */
    pinMode(_pin, INPUT_PULLUP);
}

ButtonEvent ButtonHandler::update(unsigned long nowMs) {
    bool rawState = digitalRead(_pin);

    /* Handle Debouncing */
    if (rawState != _lastRawState) {
        _lastDebounceMs = nowMs;
    }

    if ((nowMs - _lastDebounceMs) >= _debounceMs) {
        if (rawState != _stableState) {
            _stableState = rawState;

            if (_stableState == LOW) { // Button pressed
                _pressed = true;
                _pressStartMs = nowMs;
                _longReported = false;
            } else { // Button released
                if (_pressed) {
                    _pressed = false;
                    if (!_longReported) {
                        return ButtonEvent::SHORT_PRESS;
                    }
                }
            }
        }
    }

    // Long press detection
    if (_pressed && !_longReported && (nowMs - _pressStartMs >= _longPressMs)) {
        _longReported = true;
        return ButtonEvent::LONG_PRESS;
    }

    _lastRawState = rawState;
    return ButtonEvent::NONE;
}