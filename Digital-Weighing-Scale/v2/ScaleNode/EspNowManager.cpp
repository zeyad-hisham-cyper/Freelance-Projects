/*
 * EspNowManager.cpp — Scale Node
 * ------------------------------------------------------------------------
 * See EspNowManager.h for architecture notes and pairing flow.
 * ------------------------------------------------------------------------
 */

#include "EspNowManager.h"

// Broadcast MAC used while searching for the Controller
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Static singleton pointer — ESP-NOW callback cannot carry a context pointer
EspNowManager* EspNowManager::instance = nullptr;

// ============================================================
//  Construction
// ============================================================

EspNowManager::EspNowManager()
    : pairingState(EspNowPairingState::UNPAIRED),
      lastPairingRequestMs(0),
      receivedAck(false)
{
    instance = this;
    memset(controllerMac, 0, sizeof(controllerMac));
    memset(ackSenderMac,  0, sizeof(ackSenderMac));
}

// ============================================================
//  Public API
// ============================================================

bool EspNowManager::begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Initialization failed");
        return false;
    }

    // Set the Primary Master Key — must match the Controller
    esp_now_set_pmk((uint8_t *)ESPNOW_PMK);

    // Register the receive callback
    esp_now_register_recv_cb(EspNowManager::onDataRecvStatic);

    // Try to restore a previously saved Controller MAC from NVS
    loadPairing();

    if (pairingState == EspNowPairingState::UNPAIRED) {
        // Add the broadcast peer so we can transmit PAIRING_REQ packets
        addBroadcastPeer();
        Serial.println("[ESP-NOW] No Controller MAC in NVS. Entering pairing mode...");
    }

    return true;
}

void EspNowManager::loop()
{
    // ---- Handle a received ACK (set in the callback, processed here) ----
    if (receivedAck) {
        receivedAck = false;

        savePairing(ackSenderMac);

        // Remove broadcast peer; add the Controller as a direct unicast peer
        esp_now_del_peer(BROADCAST_MAC);
        addControllerPeer(controllerMac);

        pairingState = EspNowPairingState::PAIRED;
        Serial.printf("[ESP-NOW] Paired with Controller: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      controllerMac[0], controllerMac[1], controllerMac[2],
                      controllerMac[3], controllerMac[4], controllerMac[5]);
    }

    // ---- Retransmit pairing request while unpaired ----
    if (pairingState == EspNowPairingState::UNPAIRED) {
        unsigned long now = millis();
        if (now - lastPairingRequestMs >= ESPNOW_PAIRING_INTERVAL_MS) {
            lastPairingRequestMs = now;
            sendPairingRequest();
        }
    }
}

bool EspNowManager::sendWeight(float weight)
{
    if (pairingState != EspNowPairingState::PAIRED) {
        return false;
    }

    ScaleData packet;
    packet.packetType = PACKET_TYPE_DATA;
    packet.magic[0]   = 'S';
    packet.magic[1]   = 'C';
    packet.magic[2]   = 'L';
    packet.weight     = weight;

    esp_err_t result = esp_now_send(controllerMac,
                                    reinterpret_cast<const uint8_t*>(&packet),
                                    sizeof(ScaleData));
    return (result == ESP_OK);
}

bool EspNowManager::isPaired() const
{
    return (pairingState == EspNowPairingState::PAIRED);
}

void EspNowManager::clearPairing()
{
    if (pairingState == EspNowPairingState::PAIRED) {
        esp_now_del_peer(controllerMac);
    }

    preferences.begin("scale_app", false);
    preferences.remove("ctrl_mac");
    preferences.end();

    memset(controllerMac, 0, sizeof(controllerMac));
    pairingState = EspNowPairingState::UNPAIRED;

    addBroadcastPeer();
    Serial.println("[ESP-NOW] Pairing cleared. Re-entering pairing mode...");
}

// ============================================================
//  Private helpers
// ============================================================

void EspNowManager::addBroadcastPeer()
{
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_MAC, 6);
    peer.channel = 0;
    peer.encrypt = false;   // Broadcast cannot be encrypted in ESP-NOW

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[ESP-NOW] Failed to add broadcast peer");
    }
}

void EspNowManager::addControllerPeer(const uint8_t *mac)
{
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;
    peer.encrypt = false;   // Phase 4: set to true and supply LMK for encryption

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[ESP-NOW] Failed to add Controller peer");
    }
}

void EspNowManager::sendPairingRequest()
{
    ScaleData packet;
    packet.packetType = PACKET_TYPE_PAIRING_REQ;
    packet.magic[0]   = 'S';
    packet.magic[1]   = 'C';
    packet.magic[2]   = 'L';
    packet.weight     = 0.0f;

    esp_now_send(BROADCAST_MAC,
                 reinterpret_cast<const uint8_t*>(&packet),
                 sizeof(ScaleData));

    Serial.println("[ESP-NOW] Pairing request broadcast sent");
}

void EspNowManager::loadPairing()
{
    preferences.begin("scale_app", true); // read-only
    size_t len = preferences.getBytes("ctrl_mac", controllerMac, 6);
    preferences.end();

    if (len == 6) {
        pairingState = EspNowPairingState::PAIRED;
        addControllerPeer(controllerMac);
        Serial.printf("[ESP-NOW] Controller MAC loaded from NVS: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      controllerMac[0], controllerMac[1], controllerMac[2],
                      controllerMac[3], controllerMac[4], controllerMac[5]);
    }
}

void EspNowManager::savePairing(const uint8_t *mac)
{
    memcpy(controllerMac, mac, 6);

    preferences.begin("scale_app", false);
    preferences.putBytes("ctrl_mac", mac, 6);
    preferences.end();

    Serial.printf("[ESP-NOW] Controller MAC saved to NVS: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ============================================================
//  ESP-NOW receive callback
// ============================================================

void EspNowManager::onDataRecvStatic(const esp_now_recv_info_t *recvInfo,
                                     const uint8_t *incomingData,
                                     int len)
{
    if (instance && recvInfo) {
        instance->onDataRecv(recvInfo->src_addr, incomingData, len);
    }
}

void EspNowManager::onDataRecv(const uint8_t *senderMac,
                               const uint8_t *incomingData,
                               int len)
{
    if (len != sizeof(ScaleData)) {
        return;
    }

    ScaleData packet;
    memcpy(&packet, incomingData, sizeof(ScaleData));

    // Validate magic identifier
    if (packet.magic[0] != 'S' || packet.magic[1] != 'C' || packet.magic[2] != 'L') {
        return;
    }

    if (packet.packetType == PACKET_TYPE_PAIRING_ACK) {
        if (pairingState == EspNowPairingState::UNPAIRED && !receivedAck) {
            // Capture the Controller MAC and raise the flag for loop() to handle
            memcpy(ackSenderMac, senderMac, 6);
            receivedAck = true;
        }
    }
}
