/*
 * UnitManager.cpp
 * ------------------------------------------------------------------------
 * See UnitManager.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "UnitManager.h"

// Conversion factors from grams. Values are the internationally defined
// exact conversions (1 lb = 453.59237 g by definition).
static const float GRAMS_PER_KILOGRAM = 1000.0f;
static const float GRAMS_PER_POUND    = 453.59237f;
static const float GRAMS_PER_OUNCE    = 28.349523125f;

UnitManager::UnitManager() : _unit(Unit::Kilogram) {}

void UnitManager::next() {
    // Wrapping on Unit::COUNT means adding a unit to the enum automatically
    // adds it to the MODE cycle — no second list to keep in sync.
    uint8_t index = (uint8_t)_unit + 1;
    if (index >= (uint8_t)Unit::COUNT) {
        index = 0;
    }
    _unit = (Unit)index;
}

void UnitManager::setUnit(Unit unit) {
    if ((uint8_t)unit < (uint8_t)Unit::COUNT) {
        _unit = unit;
    }
}

float UnitManager::convert(float grams) const {
    switch (_unit) {
        case Unit::Kilogram: return grams / GRAMS_PER_KILOGRAM;
        case Unit::Pound:    return grams / GRAMS_PER_POUND;
        case Unit::Ounce:    return grams / GRAMS_PER_OUNCE;
        case Unit::Gram:
        default:             return grams;
    }
}

const char *UnitManager::label() const {
    switch (_unit) {
        case Unit::Kilogram: return "kg";
        case Unit::Pound:    return "lb";
        case Unit::Ounce:    return "oz";
        case Unit::Gram:
        default:             return "g";
    }
}

uint8_t UnitManager::decimals() const {
    // Chosen so every unit shows roughly the same physical resolution:
    // 1 g == 0.001 kg == 0.002 lb == 0.04 oz.
    switch (_unit) {
        case Unit::Kilogram: return 3;
        case Unit::Pound:    return 3;
        case Unit::Ounce:    return 2;
        case Unit::Gram:
        default:             return 1;
    }
}
