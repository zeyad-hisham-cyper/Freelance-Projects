#include "EspNowManager.h"

EspNowManager* EspNowManager::instance = nullptr;

EspNowManager::EspNowManager() : paired(false), hasNewData(false), latestWeight(0.0f), pendingPairing(false)
{
    instance = this;
    memset(peerMac, 0, sizeof(peerMac));
    memset(pendingMac, 0, sizeof(pendingMac));
}

bool EspNowManager::begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return false;
    }

    // Set Primary Master Key
    esp_now_set_pmk((uint8_t *)PMK_KEY_STR);

    // Register callback
    esp_now_register_recv_cb(EspNowManager::onDataRecvStatic);

    loadPairing();

    return true;
}

void EspNowManager::loadPairing()
{
    preferences.begin("scale_app", false);
    size_t macLen = preferences.getBytes("peer_mac", peerMac, 6);
    preferences.end();

    if (macLen == 6) {
        Serial.println("[ESP-NOW] Found paired Scale MAC in NVS. Adding peer...");
        paired = true;
        addPeer(peerMac);
    } else {
        Serial.println("[ESP-NOW] No paired Scale MAC in NVS. Waiting for pairing request...");
        paired = false;
    }
}

void EspNowManager::savePairing(const uint8_t *mac)
{
    preferences.begin("scale_app", false);
    preferences.putBytes("peer_mac", mac, 6);
    preferences.end();

    memcpy(peerMac, mac, 6);
    paired = true;
    
    Serial.printf("Pairing saved: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void EspNowManager::clearPairing()
{
    preferences.begin("scale_app", false);
    preferences.remove("peer_mac");
    preferences.end();

    if (paired) {
        esp_now_del_peer(peerMac);
        paired = false;
        memset(peerMac, 0, 6);
    }
    Serial.println("Pairing cleared.");
}

void EspNowManager::addPeer(const uint8_t *mac)
{
    // If the peer is already registered (e.g. loaded from NVS), skip adding again
    if (esp_now_is_peer_exist(mac)) {
        Serial.println("[ESP-NOW] Peer already registered");
        return;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = 0;
    // Phase 4: set encrypt = true and supply LMK for encrypted communication
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ESP-NOW] Failed to add peer");
    } else {
        Serial.println("[ESP-NOW] Peer added successfully");
    }
}

void EspNowManager::loop()
{
    if (pendingPairing) {
        pendingPairing = false;

        // Save MAC if this is a new peer (will be a no-op if MAC already matches NVS)
        savePairing(pendingMac);

        // Register peer if not already registered
        addPeer(pendingMac);

        // Reply with PAIRING_ACK so the Scale Node knows our MAC
        ScaleData ackData = {};
        ackData.packetType = PACKET_TYPE_PAIRING_ACK;
        ackData.magic[0]   = 'S';
        ackData.magic[1]   = 'C';
        ackData.magic[2]   = 'L';
        ackData.weight     = 0.0f;

        esp_err_t result = esp_now_send(pendingMac,
                                        reinterpret_cast<const uint8_t*>(&ackData),
                                        sizeof(ScaleData));
        if (result == ESP_OK) {
            Serial.println("[ESP-NOW] Pairing ACK sent to Scale Node");
        } else {
            Serial.printf("[ESP-NOW] Failed to send ACK (err=0x%x)\n", result);
        }
    }
}

bool EspNowManager::available()
{
    if (hasNewData) {
        hasNewData = false;
        return true;
    }
    return false;
}

float EspNowManager::getWeight()
{
    return latestWeight;
}

bool EspNowManager::isPaired()
{
    return paired;
}

void EspNowManager::onDataRecvStatic(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len)
{
    if (instance && recvInfo) {
        instance->onDataRecv(recvInfo->src_addr, incomingData, len);
    }
}

void EspNowManager::onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    if (len == sizeof(ScaleData)) {
        ScaleData data;
        memcpy(&data, incomingData, sizeof(ScaleData));

        if (data.packetType == PACKET_TYPE_PAIRING_REQ) {
            if (data.magic[0] != 'S' || data.magic[1] != 'C' || data.magic[2] != 'L') {
                return; // Not our device
            }

            if (!paired) {
                // First pairing — accept any Scale Node
                Serial.println("[ESP-NOW] PAIRING_REQ received — new peer");
                memcpy(pendingMac, mac, 6);
                pendingPairing = true;
            } else if (memcmp(mac, peerMac, 6) == 0) {
                // Already paired with this Scale Node but it rebooted / lost NVS.
                // Re-send the ACK so it can recover its Controller MAC.
                Serial.println("[ESP-NOW] PAIRING_REQ from known Scale — re-sending ACK");
                memcpy(pendingMac, mac, 6);
                pendingPairing = true;
            } else {
                Serial.println("[ESP-NOW] PAIRING_REQ from unknown device — ignored");
            }
        } 
        else if (data.packetType == PACKET_TYPE_DATA) {
            if (paired) {
                // Ensure this is from our paired peer
                if (memcmp(mac, peerMac, 6) == 0) {
                    latestWeight = data.weight;
                    hasNewData = true;
                }
            }
        }
    }
}
