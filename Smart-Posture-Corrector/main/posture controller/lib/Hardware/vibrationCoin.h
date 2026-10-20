#pragma once
#include <Arduino.h>

class VibrationMotor {
public:
    VibrationMotor(uint8_t pin);

    void begin();
    void update(unsigned long nowMs);

    void shortBuzz();       // 200 ms
    void longBuzz();        // 700 ms
    void doubleShort();     // 2 x short
    void tripleShort();     // 3 x short

private:
    enum class Pattern {
        NONE,
        SHORT,
        LONG,
        DOUBLE_SHORT,
        TRIPLE_SHORT
    };

    uint8_t _pin;
    Pattern _pattern;
    unsigned long _patternStartMs;
    int _step;

    void startPattern(Pattern p, unsigned long nowMs);
    void drivePattern(unsigned long nowMs);
};
