/*
 * WirelessManager.h
 * ------------------------------------------------------------------------
 * The receiving half of the link: NRF initialisation, packet reception,
 * validation, and connection status.
 *
 * It is to NRF24 what DisplayManager is to MAX7219 — the layer that knows
 * what the firmware wants to hear, while the driver knows how to hear it.
 * Nothing above this file sees a register, a pipe or a FIFO.
 *
 * TWO THINGS THIS MODULE EXISTS TO GET RIGHT:
 *
 *   1. A packet is only accepted if its magic and version match. Another
 *      nRF24 installation sharing the channel, or a scale running older
 *      firmware, must never be decoded as a weight.
 *
 *   2. "How old is the reading" is a first-class question, not an
 *      afterthought. A remote display's characteristic failure is showing a
 *      number that stopped being true minutes ago, so packetAgeMs() and
 *      isLinkUp() are the two calls the application leans on hardest.
 * ------------------------------------------------------------------------
 */

#ifndef WIRELESS_MANAGER_H
#define WIRELESS_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"
#include "../drivers/NRF24.h"
#include "../protocol/WeightPacket.h"

// What the link is doing, in the three states the operator cares about.
enum class LinkState : uint8_t {
    NoRadio,   // no chip answering on SPI — a fault on THIS board
    Down,      // the radio works; the scale is off, out of range or silent
    Up         // a valid packet arrived within LINK_TIMEOUT_MS
};

class WirelessManager {
public:
    WirelessManager();

    // Brings the radio up in receive mode. False means no chip answered;
    // the manager stays usable and keeps retrying in update().
    bool begin();

    // Call every loop. Drains the receive FIFO and re-probes a radio that
    // has stopped answering.
    void update();

    // True once a valid packet has ever arrived. Always pair it with
    // packetAgeMs() or isLinkUp() before displaying the contents.
    bool hasPacket() const { return _hasPacket; }
    const WeightPacket &packet() const { return _packet; }

    // Milliseconds since the last valid packet, or 0xFFFFFFFF if none has
    // ever arrived.
    uint32_t packetAgeMs() const;

    // -------------------------------------------------------- Connection
    LinkState link() const;
    bool isLinkUp() const { return link() == LinkState::Up; }
    bool hasRadio() const { return _radioPresent; }

    // -------------------------------------------------------- Diagnostics
    uint32_t receivedCount() const { return _received; }
    uint32_t rejectedCount() const { return _rejected; }

    // Packets the sequence numbers say never arrived. A steady trickle is
    // normal at the edge of range; a rising rate means interference or a
    // marginal supply.
    uint32_t missedCount() const { return _missed; }

    const char *lastErrorText() const;

    // Full register dump plus this module's counters.
    void logStatus(Stream &out);

private:
    NRF24 _radio;

    bool         _radioPresent;
    bool         _linkWasUp;   // for edge-triggered link logging
    bool         _hasPacket;
    WeightPacket _packet;

    uint32_t _lastRxMs;
    uint32_t _lastProbeMs;

    uint32_t _received;
    uint32_t _rejected;
    uint32_t _missed;

    bool     _haveSequence;    // false until the first packet sets the run
    uint16_t _lastSequence;

    void applyRole();
    void accept(const WeightPacket &incoming);
};

#endif  // WIRELESS_MANAGER_H
