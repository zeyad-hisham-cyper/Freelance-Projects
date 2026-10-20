#pragma once
#include <Arduino.h>
#include "Sensors.h"

enum class PostureEvent {
    NONE,
    FIRST_BAD,
    PERSISTENT_BAD
};

class PostureLogic {
public:
    PostureLogic();

    // Load baseline + level from EEPROM
    void begin();

    // Calibration
    void startCalibration(unsigned long nowMs);
    void updateCalibration(const PostureAngles& angles, unsigned long nowMs);
    bool isCalibrationDone(unsigned long nowMs) const;
    void finishCalibration();     // compute and save baseline (and level) to EEPROM
    bool hasBaseline() const;

    // Adaptive threshold levels (0,1,2,...)
    void setAdaptiveLevel(uint8_t level);
    uint8_t getAdaptiveLevel() const;

    float getBackThreshold() const;
    float getNeckThreshold() const;

    // Normal posture monitoring
    PostureEvent updateNormal(const PostureAngles& angles, unsigned long nowMs);

private:
    // ----- Calibration config -----
    static constexpr unsigned long CALIB_DURATION_MS = 2UL * 60UL * 1000UL; // 2 min for testing

    // ----- Bad posture detection config -----
    static constexpr unsigned long BAD_DELAY_MS        = 3000UL;  // 3 sec
    static constexpr unsigned long PERSISTENT_DELAY_MS = 15000UL; // 15 sec

    static constexpr float BACK_MIN_THRESHOLD_DEG = 15.0f;
    static constexpr float NECK_MIN_THRESHOLD_DEG = 10.0f;
    static constexpr float THRESHOLD_STEP_DEG     = 5.0f;

    // Baseline
    bool  _hasBaseline;
    float _baselineBack;
    float _baselineNeckRel;

    uint8_t _level; // adaptive level

    // Calibration state
    bool _calibrating;
    unsigned long _calibStartMs;
    float _sumBack;
    float _sumNeck;
    uint32_t _sampleCount;

    // Bad posture tracking
    bool _currentlyBad;
    unsigned long _badSinceMs;
    uint8_t _badStage; // 0 = GOOD, 1 = FIRST_SENT, 2 = PERSISTENT_SENT

    bool isPostureBad(const PostureAngles& angles) const;

    // EEPROM helpers
    void loadStateFromEEPROM();
    void saveStateToEEPROM() const;
};
