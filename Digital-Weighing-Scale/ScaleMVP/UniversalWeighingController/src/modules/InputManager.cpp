/*
 * InputManager.cpp
 * ------------------------------------------------------------------------
 * See InputManager.h for the key map and design rationale.
 * ------------------------------------------------------------------------
 */

#include "InputManager.h"

void InputManager::begin() {
    _keypad.begin();
}

void InputManager::update() {
    _keypad.update();
}

bool InputManager::nextKey(char &key) {
    char k = _keypad.read();
    if (k == MatrixKeypad::NO_KEY) {
        return false;
    }
    key = k;
    return true;
}

void InputManager::flush() {
    _keypad.flush();
}

bool InputManager::isDigit(char key, uint8_t &value) {
    if (key < '0' || key > '9') {
        return false;
    }
    value = (uint8_t)(key - '0');
    return true;
}
