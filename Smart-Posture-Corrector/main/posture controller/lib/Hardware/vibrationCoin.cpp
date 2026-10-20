#include "vibrationCoin.h"

VibrationMotor::VibrationMotor(uint8_t pin)
    : _pin(pin),
      _pattern(Pattern::NONE),
      _patternStartMs(0),
      _step(0)
{
}

void VibrationMotor::begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
}

void VibrationMotor::update(unsigned long nowMs) {
    if (_pattern == Pattern::NONE) return;
    drivePattern(nowMs);
}

void VibrationMotor::shortBuzz() {
    startPattern(Pattern::SHORT, millis());
}

void VibrationMotor::longBuzz() {
    startPattern(Pattern::LONG, millis());
}

void VibrationMotor::doubleShort() {
    startPattern(Pattern::DOUBLE_SHORT, millis());
}

void VibrationMotor::tripleShort() {
    startPattern(Pattern::TRIPLE_SHORT, millis());
}

void VibrationMotor::startPattern(Pattern p, unsigned long nowMs) {
    _pattern = p;
    _patternStartMs = nowMs;
    _step = 0;
    digitalWrite(_pin, HIGH); // start with motor ON
}

// Non-blocking pattern driver
void VibrationMotor::drivePattern(unsigned long nowMs) {
    // durations in ms
    const unsigned long SHORT_ON  = 200;
    const unsigned long SHORT_OFF = 150;
    const unsigned long LONG_ON   = 700;
    const unsigned long LONG_OFF  = 200;

    unsigned long elapsed = nowMs - _patternStartMs;

    switch (_pattern) {
        case Pattern::SHORT:
            if (elapsed >= SHORT_ON) {
                digitalWrite(_pin, LOW);
                _pattern = Pattern::NONE;
            }
            break;

        case Pattern::LONG:
            if (elapsed >= LONG_ON) {
                digitalWrite(_pin, LOW);
                _pattern = Pattern::NONE;
            }
            break;

        case Pattern::DOUBLE_SHORT:
            // steps: 0 = first ON, 1 = first OFF, 2 = second ON, 3 = second OFF
            if (_step == 0 && elapsed >= SHORT_ON) {
                digitalWrite(_pin, LOW);
                _step = 1;
                _patternStartMs = nowMs;
            } else if (_step == 1 && elapsed >= SHORT_OFF) {
                digitalWrite(_pin, HIGH);
                _step = 2;
                _patternStartMs = nowMs;
            } else if (_step == 2 && elapsed >= SHORT_ON) {
                digitalWrite(_pin, LOW);
                _step = 3;
                _patternStartMs = nowMs;
            } else if (_step == 3 && elapsed >= SHORT_OFF) {
                digitalWrite(_pin, LOW);
                _pattern = Pattern::NONE;
            }
            break;

        case Pattern::TRIPLE_SHORT:
            // steps: 0 = ON, 1 = OFF, 2 = ON, 3 = OFF, 4 = ON, 5 = OFF
            if (_step == 0 && elapsed >= SHORT_ON) {
                digitalWrite(_pin, LOW);
                _step = 1;
                _patternStartMs = nowMs;
            } else if (_step == 1 && elapsed >= SHORT_OFF) {
                digitalWrite(_pin, HIGH);
                _step = 2;
                _patternStartMs = nowMs;
            } else if (_step == 2 && elapsed >= SHORT_ON) {
                digitalWrite(_pin, LOW);
                _step = 3;
                _patternStartMs = nowMs;
            } else if (_step == 3 && elapsed >= SHORT_OFF) {
                digitalWrite(_pin, HIGH);
                _step = 4;
                _patternStartMs = nowMs;
            } else if (_step == 4 && elapsed >= SHORT_ON) {
                digitalWrite(_pin, LOW);
                _step = 5;
                _patternStartMs = nowMs;
            } else if (_step == 5 && elapsed >= SHORT_OFF) {
                digitalWrite(_pin, LOW);
                _pattern = Pattern::NONE;
            }
            break;

        case Pattern::NONE:
        default:
            break;
    }
}
