#pragma once

#include <Arduino.h>
#include "sensors.h"
#include "postureLogic.h"
#include "BLEDashboard.h"
#include "button.h"
#include "vibrationCoin.h"

enum class AppMode {
    CALIBRATING,
    NORMAL,
    SPECIAL
};

class PostureApp {
public:
    PostureApp();
    void begin();
    void update();

private:
   // Hardware & Logic Modules
    Sensors        _sensors;
    PostureLogic   _logic;
    BLEDashboard   _dashboard;
    ButtonHandler  _button;
    VibrationMotor _vibBack;
    VibrationMotor _vibNeck;

    // State
    AppMode _mode;
    unsigned long _lastDebugPrintMs;
    unsigned long _specialLastReminderMs;
    


    // Helper methods
    void checkButton(unsigned long nowMs);
    void checkSerial(unsigned long nowMs);
    void runCalibration(const PostureAngles& angles, unsigned long nowMs);
    void runNormal(const PostureAngles& angles, unsigned long nowMs);
    void runSpecial(unsigned long nowMs);

    void setMode(AppMode m);
    void printStatus(const PostureAngles& angles, unsigned long nowMs);
    
    // Actions
    void startCalibration(unsigned long nowMs);
    void toggleSpecialMode(unsigned long nowMs);
    
    // Feedback
    void handlePostureEvent(PostureEvent evt, const PostureAngles& angles);
};
