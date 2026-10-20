#pragma once

#include <Arduino.h>
#include <functional>

// Forward-declare ESP32 BLE types to keep header lightweight
class BLECharacteristic;
class BLEServer;

class BLEDashboard {
public:
    BLEDashboard();

    void begin();
    void update();   // call in loop — handles notify interval

    // Push posture data to connected Flutter app (via BLE notify)
    void sendSensorData(float backAngle, float neckAngle,
                        bool isCalibration, float backTh,
                        float neckTh, int mode);

    // Callbacks from Flutter commands
    using CalibrationCallback = std::function<void()>;
    using ToggleModeCallback  = std::function<void()>;
    using SetLevelCallback    = std::function<void(uint8_t)>;

    void onStartCalibration(CalibrationCallback cb);
    void onToggleMode(ToggleModeCallback cb);
    void onSetLevel(SetLevelCallback cb);

    // Connection state
    bool isConnected() const;

private:
    BLEServer*          _pServer;
    BLECharacteristic*  _pPostureChar;   // Notify — sensor data
    BLECharacteristic*  _pCommandChar;   // Write  — commands from Flutter
    BLECharacteristic*  _pStatusChar;    // Read   — current mode string

    CalibrationCallback _calibCb;
    ToggleModeCallback  _toggleCb;
    SetLevelCallback    _levelCb;

    bool _deviceConnected;
    bool _oldDeviceConnected;

    // Allow BLE callback classes to access internals
    friend class ServerCallbacks;
    friend class CommandCallbacks;
};
