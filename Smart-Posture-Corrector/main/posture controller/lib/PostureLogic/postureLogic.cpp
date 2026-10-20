#include "PostureLogic.h"
#include <EEPROM.h>
#include <math.h>

// EEPROM params
static constexpr int EEPROM_SIZE      = 64;
static constexpr int ADDR_MAGIC       = 0;   // uint32_t
static constexpr int ADDR_LEVEL       = 4;   // uint8_t
static constexpr int ADDR_BASE_BACK   = 8;   // float
static constexpr int ADDR_BASE_NECK   = 12;  // float
static constexpr uint32_t MAGIC_VALUE = 0x504F5354; // 'POST'

PostureLogic::PostureLogic()
    : _hasBaseline(false),
      _baselineBack(0.0f),
      _baselineNeckRel(0.0f),
      _level(0),
      _calibrating(false),
      _calibStartMs(0),
      _sumBack(0.0f),
      _sumNeck(0.0f),
      _sampleCount(0),
      _currentlyBad(false),
      _badSinceMs(0),
      _badStage(0)
{
}

void PostureLogic::begin() {
    // Initialize EEPROM once at startup
    EEPROM.begin(EEPROM_SIZE);

    loadStateFromEEPROM();

    // Reset runtime state
    _calibrating  = false;
    _sampleCount  = 0;
    _sumBack      = 0.0f;
    _sumNeck      = 0.0f;
    _currentlyBad = false;
    _badSinceMs   = 0;
    _badStage     = 0;
}

void PostureLogic::startCalibration(unsigned long nowMs) {
    _calibrating   = true;
    _calibStartMs  = nowMs;
    _sumBack       = 0.0f;
    _sumNeck       = 0.0f;
    _sampleCount   = 0;
}

void PostureLogic::updateCalibration(const PostureAngles& angles, unsigned long nowMs) {
    (void)nowMs;

    if (!_calibrating) return;

    _sumBack += angles.backAngleDeg;
    _sumNeck += angles.neckRelAngleDeg;
    _sampleCount++;
}

bool PostureLogic::isCalibrationDone(unsigned long nowMs) const {
    if (!_calibrating) return false;
    if (_sampleCount == 0) return false;
    return (nowMs - _calibStartMs) >= CALIB_DURATION_MS;
}

void PostureLogic::finishCalibration() {
    if (!_calibrating || _sampleCount == 0) return;

    // Compute new baseline from accumulated samples
    _baselineBack    = _sumBack / (float)_sampleCount;
    _baselineNeckRel = _sumNeck / (float)_sampleCount;

    _hasBaseline = true;
    _calibrating = false;

    // Reset posture tracking
    _currentlyBad = false;
    _badSinceMs   = 0;
    _badStage     = 0;

    // Reset adaptive level to 0 after calibration (Week 1)
    _level = 0;

    // Save everything to EEPROM
    saveStateToEEPROM();
}

bool PostureLogic::hasBaseline() const {
    return _hasBaseline;
}

void PostureLogic::setAdaptiveLevel(uint8_t level) {
    _level = level;
    if (_hasBaseline) {
        saveStateToEEPROM();
    }
}

uint8_t PostureLogic::getAdaptiveLevel() const {
    return _level;
}

float PostureLogic::getBackThreshold() const {
    if (!_hasBaseline) return 999.0f; // effectively disabled

    float th = _baselineBack - THRESHOLD_STEP_DEG * (float)_level;
    if (th < BACK_MIN_THRESHOLD_DEG) th = BACK_MIN_THRESHOLD_DEG;
    return th;
}

float PostureLogic::getNeckThreshold() const {
    if (!_hasBaseline) return 999.0f;

    float th = _baselineNeckRel - THRESHOLD_STEP_DEG * (float)_level;
    if (th < NECK_MIN_THRESHOLD_DEG) th = NECK_MIN_THRESHOLD_DEG;
    return th;
}

bool PostureLogic::isPostureBad(const PostureAngles& angles) const {
    if (!_hasBaseline) return false;

    float backThresh = getBackThreshold();
    float neckThresh = getNeckThreshold();

    bool badBack = fabsf(angles.backAngleDeg)      > backThresh;
    bool badNeck = fabsf(angles.neckRelAngleDeg)   > neckThresh;

    return (badBack || badNeck);
}

PostureEvent PostureLogic::updateNormal(const PostureAngles& angles, unsigned long nowMs) {
    if (!_hasBaseline) return PostureEvent::NONE;

    bool bad = isPostureBad(angles);

    if (!bad) {
        _currentlyBad = false;
        _badSinceMs   = 0;
        _badStage     = 0;
        return PostureEvent::NONE;
    }

    if (!_currentlyBad) {
        // Just turned bad
        _currentlyBad = true;
        _badSinceMs   = nowMs;
        _badStage     = 0;
        return PostureEvent::NONE; // wait for delay
    }

    unsigned long badDuration = nowMs - _badSinceMs;

    if (_badStage == 0) {
        if (badDuration >= BAD_DELAY_MS) {
            _badStage = 1;
            return PostureEvent::FIRST_BAD;
        }
        return PostureEvent::NONE;
    }

    if (_badStage == 1) {
        if (badDuration >= PERSISTENT_DELAY_MS) {
            _badStage = 2;
            return PostureEvent::PERSISTENT_BAD;
        }
        return PostureEvent::NONE;
    }

    // _badStage == 2: already reported persistent; no more events
    return PostureEvent::NONE;
}

// ---------- EEPROM helpers ----------

void PostureLogic::loadStateFromEEPROM() {
    uint32_t magic = 0;
    EEPROM.get(ADDR_MAGIC, magic);

    if (magic != MAGIC_VALUE) {
        // No valid data stored
        _hasBaseline     = false;
        _baselineBack    = 0.0f;
        _baselineNeckRel = 0.0f;
        _level           = 0;
        return;
    }

    uint8_t lvl = 0;
    float baseBack = 0.0f;
    float baseNeck = 0.0f;

    EEPROM.get(ADDR_LEVEL, lvl);
    EEPROM.get(ADDR_BASE_BACK, baseBack);
    EEPROM.get(ADDR_BASE_NECK, baseNeck);

    if (!isfinite(baseBack) || !isfinite(baseNeck)) {
        _hasBaseline     = false;
        _baselineBack    = 0.0f;
        _baselineNeckRel = 0.0f;
        _level           = 0;
        return;
    }

    // Harden against invalid level
    if (lvl > 5) lvl = 5;

    _hasBaseline     = true;
    _baselineBack    = baseBack;
    _baselineNeckRel = baseNeck;
    _level           = lvl;
}

void PostureLogic::saveStateToEEPROM() const {
    if (!_hasBaseline) {
        // Clear magic so we know there's no valid baseline
        uint32_t zero = 0;
        EEPROM.put(ADDR_MAGIC, zero);
        EEPROM.commit();
        return;
    }

    // Write magic and fields
    EEPROM.put(ADDR_MAGIC, MAGIC_VALUE);
    EEPROM.put(ADDR_LEVEL, _level);
    EEPROM.put(ADDR_BASE_BACK, _baselineBack);
    EEPROM.put(ADDR_BASE_NECK, _baselineNeckRel);

    EEPROM.commit();
}
