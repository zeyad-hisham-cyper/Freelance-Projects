#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"
#include "StabilityDetector.h"
#include "WeightCalculator.h"

enum class CalState {
    IDLE,
    CAL_WAIT_ZERO_STABLE,
    CAL_WAIT_WEIGHT_PLACED,
    CAL_WAIT_WEIGHT_STABLE,
    CAL_INPUT_WEIGHT
};

class CalibrationManager {
public:
    CalibrationManager(StabilityDetector& stabilityDetector, WeightCalculator& weightCalculator);

    void begin();

    // Call this frequently in loop() to handle button presses and state machines
    void update(int32_t latestRawValue);

private:
    StabilityDetector& _stability;
    WeightCalculator& _weightCalc;
    Preferences _prefs;

    CalState _state;
    int32_t _currentRaw;
    
    // UI state
    unsigned long _lastGreenBlinkTime;
    int _greenBlinksRemaining;
    bool _greenLedState;

    unsigned long _lastRedBlinkTime;
    int _redBlinksRemaining;
    bool _redLedState;

    // Button debouncing
    unsigned long _lastTarePressTime;
    unsigned long _lastCalPressTime;
    bool _tareLastState;
    bool _calLastState;

    int32_t _zeroReadingForCal;

    void handleButtons();
    void handleLeds();
    void processCalibrationState();

    void executeTare();
    void startCalibration();
    void failCalibration(const char* reason);
    
    void blinkGreen(int times);
    void blinkRed(int times);

    void saveToNVS();
    void loadFromNVS();
};

#endif // CALIBRATION_MANAGER_H
