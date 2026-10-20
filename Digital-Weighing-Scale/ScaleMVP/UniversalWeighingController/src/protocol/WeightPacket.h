/*
 * WeightPacket.h
 * ------------------------------------------------------------------------
 * THE WIRE CONTRACT BETWEEN THE TWO FIRMWARES.
 *
 * This file is duplicated, byte for byte, in two independent projects:
 *
 *      UniversalWeighingController/src/protocol/WeightPacket.h   (sender)
 *      WirelessDashboard/src/protocol/WeightPacket.h             (receiver)
 *
 * They are separate Arduino sketches and cannot share a directory, so the
 * only thing keeping them compatible is that these two files stay identical.
 * If you change one, change the other in the same commit and re-flash BOTH
 * boards. `fc` / `diff` the two files whenever the link misbehaves.
 *
 * RULES FOR CHANGING THE PACKET:
 *
 *   1. Never insert a field in the middle and never reorder — append only.
 *      Both ends memcpy the struct straight off the air.
 *   2. Bump PACKET_VERSION when the layout changes. A receiver rejects any
 *      version it does not recognise instead of decoding garbage, which is
 *      what makes a half-updated pair fail loudly rather than quietly.
 *   3. Keep sizeof(WeightPacket) <= NRF_PAYLOAD_SIZE on both sides. Both
 *      projects static_assert this.
 *
 * WHY THIS IS SAFE DESPITE BEING A RAW STRUCT COPY: both ends are the same
 * compiler, the same architecture and the same endianness. The explicit
 * `reserved` field keeps the two floats 4-byte aligned so no padding is
 * inserted anywhere. That assumption breaks the moment something other than
 * an ESP32 joins this link — at which point this becomes a serialiser.
 * ------------------------------------------------------------------------
 */

#ifndef WEIGHT_PACKET_H
#define WEIGHT_PACKET_H

#include <Arduino.h>

// First byte of every packet. Cheap rejection of another nRF24 installation
// that happens to share the channel and address.
static const uint8_t PACKET_MAGIC = 0x5A;

// Layout version. Bump on any change to the struct below.
static const uint8_t PACKET_VERSION = 0x01;

// Display unit, as sent on the air. These values MUST match the sender's
// UnitManager `Unit` enum — the scale static_asserts that they do.
enum PacketUnit : uint8_t {
    PKT_UNIT_GRAM = 0,
    PKT_UNIT_KILOGRAM,
    PKT_UNIT_POUND,
    PKT_UNIT_OUNCE,
    PKT_UNIT_COUNT       // sentinel; also the range check on the receiver
};

enum WeightPacketFlag : uint8_t {
    PKT_FLAG_STABLE     = 0x01,
    PKT_FLAG_TARED      = 0x02,
    PKT_FLAG_CALIBRATED = 0x04,
    PKT_FLAG_FAULT      = 0x08   // the scale has a hardware fault; the value
                                 // in this packet must not be displayed
};

struct WeightPacket {
    uint8_t  magic;         // PACKET_MAGIC
    uint8_t  version;       // PACKET_VERSION
    uint8_t  unit;          // PacketUnit
    uint8_t  flags;         // WeightPacketFlag bitmask
    uint16_t sequence;      // increments per transmission; wraps harmlessly
    uint16_t reserved;      // keeps the floats aligned; send as 0
    float    grams;         // canonical measurement, always grams
    float    displayValue;  // the same reading converted to `unit`
};

#endif  // WEIGHT_PACKET_H
