#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Preferences.h>
#include "DataTypes.h"

class EspNowManager {
public:
    EspNowManager();
    
    // Initialize WiFi, ESP-NOW, and load pairing from NVS
    bool begin();
    
    // Check if new weight data is available
    bool available();
    
    // Get the latest received weight
    float getWeight();
    
    // Check if we have paired with a Scale Node
    bool isPaired();
    
    // Clear pairing data from NVS
    void clearPairing();

    // Call this in the main loop to process pending tasks (like adding peers safely)
    void loop();

private:
    static EspNowManager* instance;
    
    Preferences preferences;
    uint8_t peerMac[6];
    bool paired;
    
    // The ESP-NOW shared key (must match on Scale Node)
    // 16 bytes for PMK/LMK
    const char* PMK_KEY_STR = "Scale_PMK_Key_12";
    const char* LMK_KEY_STR = "Scale_LMK_Key_12";

    volatile bool hasNewData;
    float latestWeight;
    
    // For pairing processing
    volatile bool pendingPairing;
    uint8_t pendingMac[6];

    static void onDataRecvStatic(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len);
    void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);
    
    void addPeer(const uint8_t *mac);
    void loadPairing();
    void savePairing(const uint8_t *mac);
};
