#include "PostureApp.h"

// --------------------------------------------------
// Pin Configuration
// --------------------------------------------------
static constexpr uint8_t PIN_BUTTON      = 16;
static constexpr uint8_t PIN_VIB_BACK    = 25;
static constexpr uint8_t PIN_VIB_NECK    = 17;

// --------------------------------------------------
// Time constants
// --------------------------------------------------
static constexpr unsigned long SPECIAL_REMINDER_MS = 5UL * 60UL * 1000UL;
static constexpr unsigned long INFO_PRINT_INTERVAL = 1000UL;

PostureApp::PostureApp()
    : _button(PIN_BUTTON),
      _vibBack(PIN_VIB_BACK),
      _vibNeck(PIN_VIB_NECK),
      _mode(AppMode::CALIBRATING),
      _lastDebugPrintMs(0),
      _specialLastReminderMs(0)
{
}

void PostureApp::begin() {
    Serial.println("Initializing PostureApp...");

    // Init Hardware
    _button.begin();
    _vibBack.begin();
    _vibNeck.begin();
    _sensors.begin();
    _logic.begin();
    delay(500); // Allow sensors to stabilize
    // Start BLE Dashboard
    _dashboard.begin();
    
    // Wire up BLE callbacks
    _dashboard.onStartCalibration([this]() {
        Serial.println("[BLE] Calibration requested");
        this->startCalibration(millis());
    });

    _dashboard.onToggleMode([this]() {
        Serial.println("[BLE] Toggle Mode requested");
        if (_mode != AppMode::CALIBRATING) {
            this->toggleSpecialMode(millis());
        } else {
            Serial.println("[BLE] Ignored — currently calibrating");
        }
    });

    _dashboard.onSetLevel([this](uint8_t lvl) {
        Serial.printf("[BLE] Level set to %d\n", (int)lvl);
        _logic.setAdaptiveLevel(lvl);
    });
    
    // Check saved state
    if (_logic.hasBaseline()) {
        Serial.println("Baseline found. Starting in NORMAL mode.");
        setMode(AppMode::NORMAL);
    } else {
        Serial.println("No baseline. Starting in CALIBRATION mode.");
        startCalibration(millis());
    }
}

void PostureApp::update() {
    unsigned long nowMs = millis();
    
    // 1. Hardware updates
    ButtonEvent be = _button.update(nowMs);
    _sensors.update(nowMs);
    _vibBack.update(nowMs);
    _vibNeck.update(nowMs);
    _dashboard.update();

    // 2. Logic updates
    if (be == ButtonEvent::SHORT_PRESS) {
        if (_mode != AppMode::CALIBRATING) toggleSpecialMode(nowMs);
    } else if (be == ButtonEvent::LONG_PRESS) {
        startCalibration(nowMs);
    }

    checkSerial(nowMs);

    // 3. Posture Logic
    PostureAngles angles = _sensors.getAngles();

    switch (_mode) {
        case AppMode::CALIBRATING:
            runCalibration(angles, nowMs);
            break;
        case AppMode::NORMAL:
            runNormal(angles, nowMs);
            break;
        case AppMode::SPECIAL:
            runSpecial(nowMs);
            break;
    }

    // 4. Reporting
    printStatus(angles, nowMs);
}

void PostureApp::checkSerial(unsigned long nowMs) {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        
        switch (c) {
            case 'c': startCalibration(nowMs); break;
            case 's': if (_mode != AppMode::CALIBRATING) toggleSpecialMode(nowMs); break;
            case 'n': if (_logic.hasBaseline()) setMode(AppMode::NORMAL); break;
            // Adaptive levels 0-5
            case '0': case '1': case '2': case '3': case '4': case '5':
                _logic.setAdaptiveLevel((uint8_t)(c - '0'));
                Serial.printf("Adaptive Level: %d\n", (int)(c - '0'));
                break;
        }
    }
}

void PostureApp::runCalibration(const PostureAngles& angles, unsigned long nowMs) {
    _logic.updateCalibration(angles, nowMs);
    
    if (_logic.isCalibrationDone(nowMs)) {
        _logic.finishCalibration();
        if (_logic.hasBaseline()) {
             // Feedback
            _vibBack.tripleShort();
            _vibNeck.tripleShort();
            setMode(AppMode::NORMAL);
        } else {
            // Failed? Restart or stay.
            Serial.println("Calibration failed to get baseline.");
        }
    }
}

void PostureApp::runNormal(const PostureAngles& angles, unsigned long nowMs) {
    if (!_logic.hasBaseline()) {
        startCalibration(nowMs);
        return;
    }
    
    PostureEvent evt = _logic.updateNormal(angles, nowMs);
    if (evt != PostureEvent::NONE) {
        handlePostureEvent(evt, angles);
    }
}

void PostureApp::runSpecial(unsigned long nowMs) {
    if (nowMs - _specialLastReminderMs >= SPECIAL_REMINDER_MS) {
        _specialLastReminderMs = nowMs;
        _vibBack.shortBuzz();
        _vibNeck.shortBuzz();
    }
}

void PostureApp::startCalibration(unsigned long nowMs) {
    Serial.println("Starting Calibration...");
    _logic.startCalibration(nowMs);
    setMode(AppMode::CALIBRATING);
}

void PostureApp::toggleSpecialMode(unsigned long nowMs) {
    if (_mode == AppMode::SPECIAL) {
        setMode(AppMode::NORMAL);
        _vibBack.shortBuzz();
        _vibNeck.shortBuzz();
    } else {
        setMode(AppMode::SPECIAL);
        _specialLastReminderMs = nowMs;
        _vibBack.doubleShort();
        _vibNeck.doubleShort();
    }
}

void PostureApp::setMode(AppMode m) {
    _mode = m;
}

void PostureApp::handlePostureEvent(PostureEvent evt, const PostureAngles& angles) {
    float backTh = _logic.getBackThreshold();
    float neckTh = _logic.getNeckThreshold();
    bool badBack = abs(angles.backAngleDeg) > backTh;
    bool badNeck = abs(angles.neckRelAngleDeg) > neckTh;

    // Fallback if neither flag triggered but event happened (edge case)
    if (!badBack && !badNeck) { badBack = true; badNeck = true; }

    if (evt == PostureEvent::FIRST_BAD) {
        if (badBack) _vibBack.doubleShort();
        if (badNeck) _vibNeck.doubleShort();
    } else if (evt == PostureEvent::PERSISTENT_BAD) {
        if (badBack) _vibBack.tripleShort();
        if (badNeck) _vibNeck.tripleShort();
    }
}

void PostureApp::printStatus(const PostureAngles& angles, unsigned long nowMs) {
    static unsigned long lastBLEUpdate = 0;
    if (nowMs - lastBLEUpdate > 200) {
        lastBLEUpdate = nowMs;
        _dashboard.sendSensorData(
            angles.backAngleDeg, 
            angles.neckRelAngleDeg, 
            (_mode == AppMode::CALIBRATING),
            _logic.getBackThreshold(),
            _logic.getNeckThreshold(),
            (int)_mode
        );
    }

    // Serial debug
    if (nowMs - _lastDebugPrintMs > INFO_PRINT_INTERVAL) {
        _lastDebugPrintMs = nowMs;
        Serial.printf("[APP] Mode=%d | Back=%.1f | Neck=%.1f\n", 
            (int)_mode, angles.backAngleDeg, angles.neckRelAngleDeg);
    }
}
