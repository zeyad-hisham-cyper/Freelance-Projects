#pragma once

#include <Arduino.h>

// This struct is sent from the Scale Node to the Controller Node
enum PacketType {
    PACKET_TYPE_PAIRING_REQ = 0x01,
    PACKET_TYPE_PAIRING_ACK = 0x02,
    PACKET_TYPE_DATA = 0x03
};

struct ScaleData {
    uint8_t packetType;
    uint8_t magic[3]; // 'S', 'C', 'L' to identify our pairing packet
    float weight;
    // Reserved for future extensions
};

