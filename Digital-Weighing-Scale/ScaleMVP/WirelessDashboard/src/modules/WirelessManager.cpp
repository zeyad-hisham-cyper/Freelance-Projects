/*
 * WirelessManager.cpp
 * ------------------------------------------------------------------------
 * See WirelessManager.h for design rationale.
 * ------------------------------------------------------------------------
 */

#include "WirelessManager.h"

// A packet larger than the configured payload would be silently truncated on
// the air and decoded as garbage. Catch it at build time.
static_assert(sizeof(WeightPacket) <= NRF_PAYLOAD_SIZE,
              "NRF_PAYLOAD_SIZE in hardware.h is smaller than WeightPacket");

// How long to wait before re-probing a radio that stopped answering.
static const uint32_t RADIO_RECOVERY_INTERVAL_MS = 5000;

WirelessManager::WirelessManager()
    : _radioPresent(false),
      _linkWasUp(false),
      _hasPacket(false),
      _lastRxMs(0), _lastProbeMs(0),
      _received(0), _rejected(0), _missed(0),
      _haveSequence(false), _lastSequence(0) {
    memset(&_packet, 0, sizeof(_packet));
}

// =====================================================================
//  BRING-UP
// =====================================================================

bool WirelessManager::begin() {
    _radioPresent = _radio.begin();

    if (!_radioPresent) {
        LOGLN("[Wireless] no nRF24 detected - check 3V3, CE/CSN wiring");
        return false;
    }

    applyRole();

    LOG("[Wireless] listening on ch ");
    LOGLN(_radio.channel());
    return true;
}

void WirelessManager::applyRole() {
    static const uint8_t scaleAddr[5]     = NRF_ADDRESS_SCALE;
    static const uint8_t dashboardAddr[5] = NRF_ADDRESS_DASHBOARD;

    // The dashboard listens on the address the scale transmits to. Its own
    // outgoing address is set as well: the receiver still transmits
    // auto-acknowledges, and that is what tells the scale the link is alive.
    _radio.openWritingPipe(dashboardAddr);
    _radio.openReadingPipe(1, scaleAddr);
    _radio.setMode(NRF24::Mode::Receive);
}

// =====================================================================
//  MAIN CYCLE
// =====================================================================

void WirelessManager::update() {
    // --- Recover a radio that has gone quiet ---------------------------
    // A brown-out on the 3.3 V rail can reset the module without resetting
    // the ESP32, leaving a chip that is present on the bus but no longer
    // configured — it would simply never receive again. Re-running begin()
    // puts it back.
    if (!_radioPresent && (millis() - _lastProbeMs >= RADIO_RECOVERY_INTERVAL_MS)) {
        _lastProbeMs = millis();

        if (_radio.begin()) {
            applyRole();
            _radioPresent = true;
            LOGLN("[Wireless] radio recovered");
        }
    }

    if (!_radioPresent) {
        return;
    }

    // --- Receive -------------------------------------------------------
    // Drain the whole FIFO rather than one packet per loop: the three-deep
    // FIFO would otherwise hand out stale readings for two more passes.
    WeightPacket incoming;
    while (_radio.available()) {
        if (_radio.read(&incoming, sizeof(incoming)) != NRF24::Error::None) {
            break;
        }

        if (incoming.magic != PACKET_MAGIC || incoming.version != PACKET_VERSION) {
            // Another nRF24 installation on the same channel and address, or
            // a scale running a different protocol version. Never display it.
            ++_rejected;
            continue;
        }

        accept(incoming);
    }

    // Report the link as edges, not as a level: two lines per outage rather
    // than one per redraw.
    bool up = isLinkUp();
    if (up != _linkWasUp) {
        LOGLN(up ? "[Wireless] link up" : "[Wireless] link lost");
        _linkWasUp = up;
    }
}

void WirelessManager::accept(const WeightPacket &incoming) {
    // Count the gap in the sequence numbers. This is the only view the
    // dashboard has of link quality: the packets that never arrived leave
    // no other trace.
    if (_haveSequence) {
        uint16_t gap = (uint16_t)(incoming.sequence - _lastSequence);
        if (gap > 1) {
            _missed += (uint32_t)(gap - 1);
        }
    }
    _lastSequence = incoming.sequence;
    _haveSequence = true;

    _packet    = incoming;
    _hasPacket = true;
    _lastRxMs  = millis();
    ++_received;
}

// =====================================================================
//  CONNECTION STATUS
// =====================================================================

uint32_t WirelessManager::packetAgeMs() const {
    if (!_hasPacket) {
        return 0xFFFFFFFF;
    }
    return millis() - _lastRxMs;
}

LinkState WirelessManager::link() const {
    if (!_radioPresent) {
        return LinkState::NoRadio;
    }

    if (_hasPacket && packetAgeMs() < LINK_TIMEOUT_MS) {
        return LinkState::Up;
    }

    return LinkState::Down;
}

const char *WirelessManager::lastErrorText() const {
    if (!_radioPresent) {
        return "no chip on SPI";
    }
    return NRF24::errorText(_radio.lastError());
}

void WirelessManager::logStatus(Stream &out) {
    _radio.dumpRegisters(out);

    out.print(F("link      : "));
    switch (link()) {
        case LinkState::Up:      out.println(F("UP"));                 break;
        case LinkState::Down:    out.println(F("down (no scale)"));    break;
        case LinkState::NoRadio: out.println(F("NO RADIO"));           break;
    }

    out.print(F("recv/rej  : "));
    out.print(_received);
    out.print('/');
    out.println(_rejected);

    out.print(F("missed    : "));
    out.println(_missed);
}
