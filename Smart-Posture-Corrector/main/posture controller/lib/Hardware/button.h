#pragma once
#include <Arduino.h>


enum class ButtonEvent {
    NONE,
    SHORT_PRESS,
    LONG_PRESS
};

/*
    * Button class
    * Handles button state and debouncing
    * Variables:
    *   - pin: the pin number the button is connected to
    *   - longPressDuration: duration in milliseconds to consider a press as long press
    *   - debounceDuration: duration in milliseconds to debounce the button
*/
class ButtonHandler {
public:
    ButtonHandler(uint8_t pin, unsigned long longPressMs = 1500, unsigned long debounceMs = 40);

    void begin();
    ButtonEvent update(unsigned long nowMs);

private:
    uint8_t _pin;
    unsigned long _longPressMs;
    unsigned long _debounceMs;

    bool _lastRawState;
    bool _stableState;
    unsigned long _lastDebounceMs;

    bool _pressed;
    unsigned long _pressStartMs;
    bool _longReported;
};