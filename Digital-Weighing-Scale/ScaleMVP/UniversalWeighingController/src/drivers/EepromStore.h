/*
 * EepromStore.h
 * ------------------------------------------------------------------------
 * Thin byte-level persistence driver.
 *
 * On the ESP32 the Arduino EEPROM library is emulated on top of an NVS
 * flash partition: writes go to a RAM mirror and only reach flash on
 * commit(). Flash has a finite erase budget, so callers must write only
 * when something actually changed — StorageManager enforces that.
 *
 * This driver knows nothing about calibration records; it moves bytes and
 * computes a CRC. Record layout and versioning live in StorageManager.
 * ------------------------------------------------------------------------
 */

#ifndef EEPROM_STORE_H
#define EEPROM_STORE_H

#include <Arduino.h>
#include "../config/config.h"

#ifdef ENABLE_EEPROM

class EepromStore {
public:
    EepromStore();

    bool begin(size_t size);

    bool read(uint16_t address, void *buffer, size_t length);

    // Writes then commits. Returns false if the commit to flash failed.
    bool write(uint16_t address, const void *buffer, size_t length);

    // CRC-32 (IEEE 802.3), used to detect a half-written or corrupt record.
    static uint32_t crc32(const void *data, size_t length);

    bool isReady() const { return _ready; }

private:
    bool _ready;
};

#endif // ENABLE_EEPROM
#endif // EEPROM_STORE_H
