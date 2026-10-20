/*
 * UnitFormat.h
 * ------------------------------------------------------------------------
 * How a received unit is written on the panel.
 *
 * The packet carries the unit and the already-converted value, so the
 * dashboard never performs a conversion — it only has to know how to LABEL
 * and how many decimals to show. Those two answers must match the scale's
 * UnitManager, or the same reading would appear at different resolutions on
 * the two panels.
 *
 * If UnitManager::label() / ::decimals() ever change on the scale, change
 * them here too. There is no compiler that can catch this for you across two
 * separate sketches.
 * ------------------------------------------------------------------------
 */

#ifndef UNIT_FORMAT_H
#define UNIT_FORMAT_H

#include <Arduino.h>
#include "../protocol/WeightPacket.h"

// Unknown units fall through to grams rather than to an empty label: an
// unexpected value means the scale is running newer firmware, and showing
// the raw number is more useful than showing nothing.
inline const char *unitLabel(uint8_t unit) {
    switch (unit) {
        case PKT_UNIT_KILOGRAM: return "kg";
        case PKT_UNIT_POUND:    return "lb";
        case PKT_UNIT_OUNCE:    return "oz";
        case PKT_UNIT_GRAM:
        default:                return "g";
    }
}

// Chosen on the scale so every unit shows roughly the same physical
// resolution: 1 g == 0.001 kg == 0.002 lb == 0.04 oz.
inline uint8_t unitDecimals(uint8_t unit) {
    switch (unit) {
        case PKT_UNIT_KILOGRAM: return 3;
        case PKT_UNIT_POUND:    return 3;
        case PKT_UNIT_OUNCE:    return 2;
        case PKT_UNIT_GRAM:
        default:                return 1;
    }
}

#endif  // UNIT_FORMAT_H
