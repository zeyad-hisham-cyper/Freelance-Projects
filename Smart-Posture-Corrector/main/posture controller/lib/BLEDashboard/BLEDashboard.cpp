#include "BLEDashboard.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// -----------------------------------------------------------------
// UUIDs — must match Flutter side exactly
// -----------------------------------------------------------------
#define SERVICE_UUID          "12345678-1234-1234-1234-123456789abc"
#define POSTURE_CHAR_UUID     "12345678-1234-1234-1234-000000000001"
#define COMMAND_CHAR_UUID     "12345678-1234-1234-1234-000000000002"
#define STATUS_CHAR_UUID      "12345678-1234-1234-1234-000000000003"

// -----------------------------------------------------------------
// BLE Callbacks
// -----------------------------------------------------------------

// Pointer to the singleton so callbacks can reach it
static BLEDashboard* _instance = nullptr;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        _instance->_deviceConnected = true;
        Serial.println("[BLE] Client connected");
    }

    void onDisconnect(BLEServer* pServer) override {
        _instance->_deviceConnected = false;
        Serial.println("[BLE] Client disconnected");
    }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        String val = pChar->getValue().c_str();
        val.trim();
        Serial.printf("[BLE] Command received: %s\n", val.c_str());

        if (val == "CALIBRATE") {
            if (_instance->_calibCb) _instance->_calibCb();
        } else if (val == "TOGGLE_MODE") {
            if (_instance->_toggleCb) _instance->_toggleCb();
        } else if (val.startsWith("LEVEL:")) {
            uint8_t lvl = (uint8_t)(val.substring(6).toInt());
            if (lvl > 5) lvl = 5;
            if (_instance->_levelCb) _instance->_levelCb(lvl);
        }
    }
};

// -----------------------------------------------------------------
// Implementation
// -----------------------------------------------------------------

BLEDashboard::BLEDashboard()
    : _pServer(nullptr),
      _pPostureChar(nullptr),
      _pCommandChar(nullptr),
      _pStatusChar(nullptr),
      _deviceConnected(false),
      _oldDeviceConnected(false)
{
    _instance = this;
}

void BLEDashboard::begin() {
    BLEDevice::init("PostureCorrector");

    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks());

    // Create service
    BLEService* pService = _pServer->createService(SERVICE_UUID);

    // 1. Posture Data — Notify
    _pPostureChar = pService->createCharacteristic(
        POSTURE_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    _pPostureChar->addDescriptor(new BLE2902());

    // 2. Command — Write
    _pCommandChar = pService->createCharacteristic(
        COMMAND_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pCommandChar->setCallbacks(new CommandCallbacks());

    // 3. Status — Read
    _pStatusChar = pService->createCharacteristic(
        STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    _pStatusChar->setValue("CALIBRATING");

    pService->start();

    // Start advertising
    BLEAdvertising* pAdv = BLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->setScanResponse(true);
    pAdv->setMinPreferred(0x06);
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Dashboard ready — advertising as 'PostureCorrector'");
}

void BLEDashboard::update() {
    // Re-start advertising when a client disconnects
    if (!_deviceConnected && _oldDeviceConnected) {
        delay(500); // give BLE stack a moment
        BLEDevice::startAdvertising();
        Serial.println("[BLE] Re-advertising");
    }
    _oldDeviceConnected = _deviceConnected;
}

void BLEDashboard::sendSensorData(float back, float neck,
                                   bool isCalib, float backTh,
                                   float neckTh, int mode)
{
    if (!_deviceConnected) return;

    // Update status characteristic
    const char* modeStr = "NORMAL";
    if (mode == 0)      modeStr = "CALIBRATING";
    else if (mode == 2) modeStr = "SPECIAL";
    _pStatusChar->setValue(modeStr);

    // Notify posture data as compact JSON
    char buf[160];
    snprintf(buf, sizeof(buf),
        "{\"back\":%.2f,\"neck\":%.2f,\"backTh\":%.1f,\"neckTh\":%.1f,"
        "\"mode\":%d,\"calibrating\":%s}",
        back, neck, backTh, neckTh, mode,
        isCalib ? "true" : "false");

    _pPostureChar->setValue(buf);
    _pPostureChar->notify();
}

bool BLEDashboard::isConnected() const {
    return _deviceConnected;
}

void BLEDashboard::onStartCalibration(CalibrationCallback cb) { _calibCb  = cb; }
void BLEDashboard::onToggleMode(ToggleModeCallback cb)        { _toggleCb = cb; }
void BLEDashboard::onSetLevel(SetLevelCallback cb)            { _levelCb  = cb; }
