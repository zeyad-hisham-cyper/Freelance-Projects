/*
 * StorageManager.h
 * ------------------------------------------------------------------------
 * Persists calibration data and user settings.
 *
 * The stored record is protected three ways, because a scale that silently
 * loads garbage calibration is worse than one that loads none:
 *
 *   magic   — distinguishes our record from unrelated/blank flash
 *   version — a layout change invalidates old records instead of
 *             reinterpreting their bytes as different floats
 *   CRC-32  — catches a write interrupted by a power cut
 *
 * If any check fails, load() reports failure and the caller falls back to
 * the defaults in calibration.h.
 *
 * When ENABLE_EEPROM is off, this compiles down to a stub that always
 * reports "nothing stored" — the rest of the firmware is unaffected.
 * ------------------------------------------------------------------------
 */

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"

#ifdef ENABLE_EEPROM
    #include "../drivers/EepromStore.h"
#endif

// Everything the controller needs to remember across a power cycle.
// Keep this POD (no pointers, no virtuals) — it is stored as raw bytes.
struct ConfigRecord {
    uint32_t magic;
    uint16_t version;
    uint8_t  unit;              // last unit selected with MODE
    uint8_t  calibrated;        // 1 once a real calibration has been done
    float    zeroOffsetCounts;  // raw counts at no load
    float    countsPerGram;     // scale factor
    float    knownWeightG;      // reference mass used last time
    uint32_t crc;               // CRC-32 of every field above
};

class StorageManager {
public:
    StorageManager();

    bool begin();

    // Fills `out` with defaults, then overwrites it from flash if a valid
    // record exists. Returns true only when a valid record was found.
    bool load(ConfigRecord &out);

    // Writes the record (recomputing magic/version/CRC first).
    // Skips the flash write entirely when the contents are unchanged, to
    // avoid burning erase cycles on every boot.
    bool save(ConfigRecord &record);

    // Restores factory defaults in flash.
    bool clear();

    static void applyDefaults(ConfigRecord &record);

    bool isAvailable() const { return _available; }

private:
#ifdef ENABLE_EEPROM
    EepromStore _store;
#endif
    bool         _available;
    ConfigRecord _cached;   // last known flash contents, for change detection

    static uint32_t computeCrc(const ConfigRecord &record);
};

#endif // STORAGE_MANAGER_H
