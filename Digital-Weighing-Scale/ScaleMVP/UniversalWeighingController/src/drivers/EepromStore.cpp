/*
 * EepromStore.cpp
 * ------------------------------------------------------------------------
 * See EepromStore.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "EepromStore.h"

#ifdef ENABLE_EEPROM

#include <EEPROM.h>

EepromStore::EepromStore() : _ready(false) {}

bool EepromStore::begin(size_t size) {
    _ready = EEPROM.begin(size);
    return _ready;
}

bool EepromStore::read(uint16_t address, void *buffer, size_t length) {
    if (!_ready) return false;

    uint8_t *dst = static_cast<uint8_t *>(buffer);
    for (size_t i = 0; i < length; i++) {
        dst[i] = EEPROM.read(address + i);
    }
    return true;
}

bool EepromStore::write(uint16_t address, const void *buffer, size_t length) {
    if (!_ready) return false;

    const uint8_t *src = static_cast<const uint8_t *>(buffer);
    for (size_t i = 0; i < length; i++) {
        EEPROM.write(address + i, src[i]);
    }
    return EEPROM.commit();
}

uint32_t EepromStore::crc32(const void *data, size_t length) {
    // Bitwise CRC-32 rather than a 256-entry table: a calibration record is
    // ~24 bytes and written once per calibration, so speed is irrelevant and
    // the 1 KB table is not worth the Flash.
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
        }
    }

    return ~crc;
}

#endif // ENABLE_EEPROM
