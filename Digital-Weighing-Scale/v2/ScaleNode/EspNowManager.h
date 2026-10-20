/*
 * EspNowManager.h — Scale Node
 * ------------------------------------------------------------------------
 * Encapsulates all ESP-NOW logic for the Scale Node (transmitter side).
 *
 * Pairing Flow:
 *   1. On boot, the Scale Node checks NVS for a saved Controller MAC.
 *   2. If none is found, it broadcasts a PAIRING_REQ packet every
 *      ESPNOW_PAIRING_INTERVAL_MS until the Controller responds.
 *   3. On receiving a PAIRING_ACK the Controller MAC is saved to NVS and
 *      the node switches to PAIRED state.
 *   4. While PAIRED, sendWeight() transmits weight packets directly to the
 *      saved Controller MAC.
 *
 * Encryption:
 *   The PMK is set on both nodes. Unicast encryption with LMK is reserved
 *   for Phase 4; the flag is defined but not yet active.
 * ------------------------------------------------------------------------
 */

#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Preferences.h>
#include "DataTypes.h"
#include "Config.h"

enum class EspNowPairingState
{
    UNPAIRED,
    PAIRED
};

class EspNowManager
{
public:
    EspNowManager();

    // Initialize WiFi and ESP-NOW. Must be called in setup().
    bool begin();

    // Must be called in loop(). Handles periodic pairing requests and ACK processing.
    void loop();

    // Transmit a weight reading to the Controller. Returns false if not paired.
    bool sendWeight(float weight);

    // True once a Controller MAC has been confirmed and saved.
    bool isPaired() const;

    // Erase the saved Controller MAC from NVS and re-enter pairing mode.
    void clearPairing();

private:
    static EspNowManager* instance;

    Preferences    preferences;
    uint8_t        controllerMac[6];
    EspNowPairingState pairingState;

    unsigned long  lastPairingRequestMs;

    // Volatile flag set inside the ISR-style ESP-NOW callback
    volatile bool  receivedAck;
    uint8_t        ackSenderMac[6]; // MAC extracted from the ACK sender

    // ---- Internal helpers ----
    void     addBroadcastPeer();
    void     addControllerPeer(const uint8_t *mac);
    void     sendPairingRequest();
    void     loadPairing();
    void     savePairing(const uint8_t *mac);

    // ---- ESP-NOW receive callback (static trampoline → member) ----
    static void onDataRecvStatic(const esp_now_recv_info_t *recvInfo,
                                 const uint8_t *incomingData,
                                 int len);
    void onDataRecv(const uint8_t *senderMac,
                    const uint8_t *incomingData,
                    int len);
};
