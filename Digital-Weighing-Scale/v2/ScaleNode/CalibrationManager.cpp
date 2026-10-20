#include "CalibrationManager.h"

#define DEBOUNCE_DELAY_MS 50
#define BLINK_INTERVAL_MS 200

CalibrationManager::CalibrationManager(StabilityDetector& stabilityDetector, WeightCalculator& weightCalculator)
    : _stability(stabilityDetector), _weightCalc(weightCalculator), _state(CalState::IDLE), _currentRaw(0),
      _lastGreenBlinkTime(0), _greenBlinksRemaining(0), _greenLedState(false),
      _lastRedBlinkTime(0), _redBlinksRemaining(0), _redLedState(false),
      _lastTarePressTime(0), _lastCalPressTime(0),
      _tareLastState(HIGH), _calLastState(HIGH), _zeroReadingForCal(0) {
}

void CalibrationManager::begin() {
    pinMode(PIN_BTN_TARE, INPUT_PULLUP);
    pinMode(PIN_BTN_CAL, INPUT_PULLUP);
    
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);

    loadFromNVS();

    // Blink green 3 times on successful startup
    blinkGreen(3);
}

void CalibrationManager::update(int32_t latestRawValue) {
    _currentRaw = latestRawValue;
    
    handleButtons();
    handleLeds();
    processCalibrationState();
}

void CalibrationManager::handleButtons() {
    bool tareCurrent = digitalRead(PIN_BTN_TARE);
    bool calCurrent = digitalRead(PIN_BTN_CAL);

    unsigned long now = millis();

    // Tare Button
    if (tareCurrent != _tareLastState) {
        if (now - _lastTarePressTime > DEBOUNCE_DELAY_MS) {
            if (tareCurrent == LOW && _state == CalState::IDLE) {
                executeTare();
            }
            _lastTarePressTime = now;
        }
        _tareLastState = tareCurrent;
    }

    // Cal Button
    if (calCurrent != _calLastState) {
        if (now - _lastCalPressTime > DEBOUNCE_DELAY_MS) {
            if (calCurrent == LOW && _state == CalState::IDLE) {
                startCalibration();
            }
            _lastCalPressTime = now;
        }
        _calLastState = calCurrent;
    }
}

void CalibrationManager::handleLeds() {
    unsigned long now = millis();

    // Green LED
    if (_greenBlinksRemaining > 0) {
        if (now - _lastGreenBlinkTime >= BLINK_INTERVAL_MS) {
            _lastGreenBlinkTime = now;
            _greenLedState = !_greenLedState;
            digitalWrite(PIN_LED_GREEN, _greenLedState ? HIGH : LOW);
            if (!_greenLedState) {
                _greenBlinksRemaining--;
            }
        }
    } else {
        digitalWrite(PIN_LED_GREEN, (_state == CalState::IDLE) ? HIGH : LOW); // Solid green when IDLE and ready
    }

    // Red LED
    if (_redBlinksRemaining > 0) {
        if (now - _lastRedBlinkTime >= BLINK_INTERVAL_MS) {
            _lastRedBlinkTime = now;
            _redLedState = !_redLedState;
            digitalWrite(PIN_LED_RED, _redLedState ? HIGH : LOW);
            if (!_redLedState) {
                _redBlinksRemaining--;
            }
        }
    } else {
        digitalWrite(PIN_LED_RED, LOW);
    }
}

void CalibrationManager::executeTare() {
    if (!_stability.isStable()) {
        Serial.println("Tare failed: Scale is unstable.");
        blinkRed(3);
        return;
    }

    _weightCalc.setZeroOffset(_currentRaw);
    saveToNVS();
    
    Serial.println("Tare successful.");
    blinkGreen(2);
}

void CalibrationManager::startCalibration() {
    Serial.println("--- CALIBRATION MODE STARTED ---");
    Serial.println("Please empty the scale and wait for stability...");
    _state = CalState::CAL_WAIT_ZERO_STABLE;
}

void CalibrationManager::processCalibrationState() {
    if (_state == CalState::IDLE) return;

    // Fast blink green while calibrating
    if (_greenBlinksRemaining == 0) blinkGreen(1);

    switch (_state) {
        case CalState::CAL_WAIT_ZERO_STABLE:
            if (_stability.isStable()) {
                _zeroReadingForCal = _currentRaw;
                Serial.println("Zero point captured!");
                Serial.println("Place known reference weight on scale.");
                _state = CalState::CAL_WAIT_WEIGHT_PLACED;
                // Add a small delay to allow user to start placing weight before checking stability
                delay(2000); 
            }
            break;

        case CalState::CAL_WAIT_WEIGHT_PLACED:
            if (abs(_currentRaw - _zeroReadingForCal) > 5000) { // Rough check that something was placed
                Serial.println("Weight detected. Waiting for stability...");
                _state = CalState::CAL_WAIT_WEIGHT_STABLE;
            }
            break;

        case CalState::CAL_WAIT_WEIGHT_STABLE:
            if (_stability.isStable()) {
                Serial.println("Weight is stable.");
                Serial.println("Enter reference weight via Serial (e.g. '5.0' for 5kg) and press Enter:");
                // Clear serial buffer
                while(Serial.available()) Serial.read();
                _state = CalState::CAL_INPUT_WEIGHT;
            }
            break;

        case CalState::CAL_INPUT_WEIGHT:
            if (Serial.available() > 0) {
                String input = Serial.readStringUntil('\n');
                input.trim();
                if (input.length() > 0) {
                    float refWeight = input.toFloat();
                    if (refWeight > 0.0f) {
                        float factor = (float)(_currentRaw - _zeroReadingForCal) / refWeight;
                        
                        _weightCalc.setZeroOffset(_zeroReadingForCal);
                        _weightCalc.setCalibrationFactor(factor);
                        saveToNVS();
                        
                        Serial.println("Calibration successful!");
                        Serial.print("New Zero Offset: "); Serial.println(_zeroReadingForCal);
                        Serial.print("New Cal Factor: "); Serial.println(factor);
                        
                        blinkGreen(5);
                        _state = CalState::IDLE;
                    } else {
                        failCalibration("Invalid weight entered.");
                    }
                }
            }
            break;

        default:
            _state = CalState::IDLE;
            break;
    }
}

void CalibrationManager::failCalibration(const char* reason) {
    Serial.print("Calibration Failed: ");
    Serial.println(reason);
    blinkRed(5);
    _state = CalState::IDLE;
}

void CalibrationManager::blinkGreen(int times) {
    _greenBlinksRemaining = times;
    _greenLedState = false;
    _lastGreenBlinkTime = millis();
}

void CalibrationManager::blinkRed(int times) {
    _redBlinksRemaining = times;
    _redLedState = false;
    _lastRedBlinkTime = millis();
}

void CalibrationManager::saveToNVS() {
    if (_prefs.begin(NVS_NAMESPACE, false)) {
        _prefs.putLong(NVS_KEY_OFFSET, _weightCalc.getZeroOffset());
        _prefs.putFloat(NVS_KEY_FACTOR, _weightCalc.getCalibrationFactor());
        _prefs.end();
        Serial.println("Calibration saved to NVS.");
    } else {
        Serial.println("Error opening NVS for saving!");
    }
}

void CalibrationManager::loadFromNVS() {
    if (_prefs.begin(NVS_NAMESPACE, true)) {
        int32_t offset = _prefs.getLong(NVS_KEY_OFFSET, 0);
        float factor = _prefs.getFloat(NVS_KEY_FACTOR, 1.0f);
        
        _weightCalc.setZeroOffset(offset);
        _weightCalc.setCalibrationFactor(factor);
        
        _prefs.end();
        
        Serial.print("Loaded from NVS - Offset: "); Serial.print(offset);
        Serial.print(" Factor: "); Serial.println(factor);
    } else {
        Serial.println("NVS empty or error opening. Using defaults.");
    }
}
