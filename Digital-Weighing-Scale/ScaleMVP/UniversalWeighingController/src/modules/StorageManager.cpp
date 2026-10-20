/*
 * StorageManager.cpp
 * ------------------------------------------------------------------------
 * See StorageManager.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "StorageManager.h"

StorageManager::StorageManager() : _available(false) {
    applyDefaults(_cached);
}

void StorageManager::applyDefaults(ConfigRecord &record) {
    record.magic            = CONFIG_MAGIC;
    record.version          = CONFIG_VERSION;
    record.unit             = (uint8_t)1;                 // Kilogram
    record.calibrated       = 0;
    record.zeroOffsetCounts = DEFAULT_ZERO_OFFSET_COUNTS;
    record.countsPerGram    = DEFAULT_COUNTS_PER_GRAM;
    record.knownWeightG     = DEFAULT_KNOWN_WEIGHT_G;
    record.crc              = 0;
}

uint32_t StorageManager::computeCrc(const ConfigRecord &record) {
#ifdef ENABLE_EEPROM
    // The CRC covers every byte of the record except the CRC field itself,
    // which is the last member.
    return EepromStore::crc32(&record, sizeof(ConfigRecord) - sizeof(uint32_t));
#else
    (void)record;
    return 0;
#endif
}

bool StorageManager::begin() {
#ifdef ENABLE_EEPROM
    _available = _store.begin(EEPROM_SIZE);
    return _available;
#else
    _available = false;
    return false;
#endif
}

bool StorageManager::load(ConfigRecord &out) {
    applyDefaults(out);

#ifdef ENABLE_EEPROM
    if (!_available) {
        return false;
    }

    ConfigRecord stored;
    if (!_store.read(EEPROM_BASE_ADDR, &stored, sizeof(stored))) {
        return false;
    }

    if (stored.magic != CONFIG_MAGIC) {
        LOGLN("[Storage] no record (magic mismatch) -> defaults");
        return false;
    }

    if (stored.version != CONFIG_VERSION) {
        LOGLN("[Storage] record version mismatch -> defaults");
        return false;
    }

    if (stored.crc != computeCrc(stored)) {
        LOGLN("[Storage] CRC mismatch, record corrupt -> defaults");
        return false;
    }

    // A stored scale factor of zero would make every reading infinite;
    // treat it as corruption rather than trusting it.
    if (!(stored.countsPerGram > 0.0f) && !(stored.countsPerGram < 0.0f)) {
        LOGLN("[Storage] invalid scale factor -> defaults");
        return false;
    }

    out     = stored;
    _cached = stored;
    return true;
#else
    return false;
#endif
}

bool StorageManager::save(ConfigRecord &record) {
#ifdef ENABLE_EEPROM
    if (!_available) {
        return false;
    }

    record.magic   = CONFIG_MAGIC;
    record.version = CONFIG_VERSION;
    record.crc     = computeCrc(record);

    // Flash endurance: skip the write when nothing actually changed.
    if (memcmp(&record, &_cached, sizeof(ConfigRecord)) == 0) {
        return true;
    }

    if (!_store.write(EEPROM_BASE_ADDR, &record, sizeof(ConfigRecord))) {
        LOGLN("[Storage] commit failed");
        return false;
    }

    _cached = record;
    LOGLN("[Storage] saved");
    return true;
#else
    (void)record;
    return false;
#endif
}

bool StorageManager::clear() {
    ConfigRecord fresh;
    applyDefaults(fresh);
    return save(fresh);
}
