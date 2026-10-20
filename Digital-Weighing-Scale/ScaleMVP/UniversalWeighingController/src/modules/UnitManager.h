/*
 * UnitManager.h
 * ------------------------------------------------------------------------
 * Display-unit conversion.
 *
 * The internal measurement is ALWAYS kept in grams. This module only
 * converts the number on its way to the screen, exactly as system.md
 * requires: pressing MODE must never change what is being measured, only
 * how it is written.
 *
 * It also owns the number of decimals per unit, because the right
 * precision is a property of the unit (1 g resolution is sensible; 0.001 kg
 * is the same thing written differently).
 * ------------------------------------------------------------------------
 */

#ifndef UNIT_MANAGER_H
#define UNIT_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"

enum class Unit : uint8_t {
    Gram = 0,
    Kilogram,
    Pound,
    Ounce,
    COUNT          // sentinel — keeps the MODE cycle in sync automatically
};

class UnitManager {
public:
    UnitManager();

    // MODE button: g -> kg -> lb -> oz -> g
    void next();

    void setUnit(Unit unit);
    Unit unit() const { return _unit; }

    // Converts an internal grams value into the active display unit.
    float convert(float grams) const;

    const char *label() const;
    uint8_t     decimals() const;

private:
    Unit _unit;
};

#endif // UNIT_MANAGER_H
