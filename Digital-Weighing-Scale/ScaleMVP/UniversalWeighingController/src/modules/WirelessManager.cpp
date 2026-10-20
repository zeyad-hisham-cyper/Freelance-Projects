/*
 * WirelessManager.cpp
 * ------------------------------------------------------------------------
 * See WirelessManager.h for design rationale.
 *
 * Every method is written so that it is harmless when ENABLE_NRF is off:
 * the class still exists and still compiles, it simply reports NoRadio.
 * That keeps App free of #ifdefs around every wireless call.
 * ------------------------------------------------------------------------
 */

#include "WirelessManager.h"
#include "UnitManager.h"

#ifdef ENABLE_NRF

// A packet that outgrew the configured payload would be silently truncated
// on the air, and the dashboard would decode garbage. Catch it at build time.
static_assert(sizeof(WeightPacket) <= NRF_PAYLOAD_SIZE,
              "NRF_PAYLOAD_SIZE in hardware.h is smaller than WeightPacket");

// The unit travels as a raw byte, so the two enums have to agree. If someone
// reorders UnitManager's `Unit`, this stops the build instead of quietly
// making the dashboard show kilograms as pounds.
static_assert((uint8_t)Unit::Gram     == PKT_UNIT_GRAM &&
              (uint8_t)Unit::Kilogram == PKT_UNIT_KILOGRAM &&
              (uint8_t)Unit::Pound    == PKT_UNIT_POUND &&
              (uint8_t)Unit::Ounce    == PKT_UNIT_OUNCE &&
              (uint8_t)Unit::COUNT    == PKT_UNIT_COUNT,
              "UnitManager::Unit no longer matches PacketUnit in WeightPacket.h");

// How long to wait before re-probing a radio that stopped answering.
static const uint32_t RADIO_RECOVERY_INTERVAL_MS = 5000;

// Consecutive send failures that trigger that re-probe. MaxRetries simply
// means the dashboard is off; it takes a run of them to suspect the local
// hardware.
static const uint8_t FAILURES_BEFORE_PROBE = 20;

#endif  // ENABLE_NRF

WirelessManager::WirelessManager()
    : _radioPresent(false),
      _linkWasUp(false),
      _lastTxMs(0), _lastOkMs(0), _lastProbeMs(0),
      _sequence(0),
      _sent(0), _failed(0),
      _consecutiveFailures(0) {}

// =====================================================================
//  BRING-UP
// =====================================================================

bool WirelessManager::begin() {
#ifdef ENABLE_NRF
    _radioPresent = _radio.begin();

    if (!_radioPresent) {
        LOGLN("[Wireless] no nRF24 detected - check 3V3, CE/CSN wiring");
        return false;
    }

    applyRole();

    LOG("[Wireless] transmitting on ch ");
    LOGLN(_radio.channel());
    return true;
#else
    return false;
#endif
}

void WirelessManager::applyRole() {
#ifdef ENABLE_NRF
    static const uint8_t scaleAddr[5]     = NRF_ADDRESS_SCALE;
    static const uint8_t dashboardAddr[5] = NRF_ADDRESS_DASHBOARD;

    // openWritingPipe also points reading pipe 0 at the same address, which
    // is what the auto-acknowledge needs. Pipe 1 is opened on the
    // dashboard's address so a future dashboard-to-scale command has
    // somewhere to arrive.
    _radio.openWritingPipe(scaleAddr);
    _radio.openReadingPipe(1, dashboardAddr);
    _radio.setMode(NRF24::Mode::Transmit);
#endif
}

// =====================================================================
//  MAIN CYCLE
// =====================================================================

void WirelessManager::update() {
#ifdef ENABLE_NRF
    bool probeDue = (millis() - _lastProbeMs) >= RADIO_RECOVERY_INTERVAL_MS;

    if (_radioPresent) {
        // A long run of failures means one of two very different things:
        // this board's radio has died, or the dashboard is simply switched
        // off. Ask the chip which, with the cheap read-back probe — and
        // NOTHING else.
        //
        // The earlier version re-ran begin() here. That was wrong twice
        // over: it reconfigured a perfectly healthy radio every five
        // seconds, and it cleared _consecutiveFailures, which restarted the
        // "first failure of a run" log so the console filled with identical
        // TX-failed lines for an outage that was one continuous event.
        if (_consecutiveFailures >= FAILURES_BEFORE_PROBE && probeDue) {
            _lastProbeMs = millis();

            if (!_radio.isChipConnected()) {
                LOGLN("[Wireless] radio stopped answering on SPI");
                _radioPresent = false;
            }
        }
    } else if (probeDue) {
        // The chip really is gone. A brown-out on the 3.3 V rail can reset
        // the module without resetting the ESP32, so re-running begin() is
        // the right response here.
        _lastProbeMs = millis();

        if (_radio.begin()) {
            applyRole();
            _radioPresent        = true;
            _consecutiveFailures = 0;
            LOGLN("[Wireless] radio recovered");
        }
    }

    // Report the link as edges, not as a level. A flapping link used to
    // produce one "TX failed" line per lost packet, which buried the
    // measurement log; two lines per outage says strictly more.
    bool up = isLinkUp();
    if (up != _linkWasUp) {
        LOGLN(up ? "[Wireless] link up" : "[Wireless] link lost");
        _linkWasUp = up;
    }
#endif
}

// =====================================================================
//  TRANSMISSION
// =====================================================================

bool WirelessManager::sendWeight(float grams, float displayValue, uint8_t unit,
                                 bool stable, bool tared, bool calibrated, bool fault) {
#ifdef ENABLE_NRF
    if (!_radioPresent) {
        return false;
    }

    if (millis() - _lastTxMs < WIRELESS_TX_INTERVAL_MS) {
        return false;
    }
    _lastTxMs = millis();

    WeightPacket packet;
    memset(&packet, 0, sizeof(packet));

    packet.magic        = PACKET_MAGIC;
    packet.version      = PACKET_VERSION;
    packet.unit         = unit;
    packet.sequence     = _sequence++;
    packet.grams        = grams;
    packet.displayValue = displayValue;

    packet.flags = 0;
    if (stable)     packet.flags |= PKT_FLAG_STABLE;
    if (tared)      packet.flags |= PKT_FLAG_TARED;
    if (calibrated) packet.flags |= PKT_FLAG_CALIBRATED;
    if (fault)      packet.flags |= PKT_FLAG_FAULT;

    NRF24::Error error = _radio.send(&packet, sizeof(packet));

    if (error == NRF24::Error::None) {
        ++_sent;
        _lastOkMs            = millis();
        _consecutiveFailures = 0;
        return true;
    }

    ++_failed;
    if (_consecutiveFailures < 255) {
        ++_consecutiveFailures;
    }

    // Deliberately silent. A missing dashboard produces one MaxRetries every
    // transmit interval forever, and a marginal link produces one every
    // other packet — either way, per-failure logging drowns the measurement
    // log without adding information. The link edge is logged in update(),
    // and the running tx=ok/fail counters are on every measurement line.
    return false;
#else
    (void)grams; (void)displayValue; (void)unit;
    (void)stable; (void)tared; (void)calibrated; (void)fault;
    return false;
#endif
}

// =====================================================================
//  CONNECTION STATUS
// =====================================================================

LinkState WirelessManager::link() const {
#ifdef ENABLE_NRF
    if (!_radioPresent) {
        return LinkState::NoRadio;
    }

    // An acknowledged transmission proves the dashboard is powered, in range
    // and listening on the right address — a stronger signal than "we sent
    // something".
    if (_lastOkMs != 0 && (millis() - _lastOkMs) < WIRELESS_LINK_TIMEOUT_MS) {
        return LinkState::Up;
    }

    return LinkState::Down;
#else
    return LinkState::NoRadio;
#endif
}

const char *WirelessManager::lastErrorText() const {
#ifdef ENABLE_NRF
    if (!_radioPresent) {
        return "no chip on SPI";
    }
    return NRF24::errorText(_radio.lastError());
#else
    return "wireless disabled";
#endif
}

void WirelessManager::logStatus(Stream &out) {
#ifdef ENABLE_NRF
    _radio.dumpRegisters(out);

    out.print(F("link      : "));
    switch (link()) {
        case LinkState::Up:      out.println(F("UP"));                    break;
        case LinkState::Down:    out.println(F("down (no dashboard)"));   break;
        case LinkState::NoRadio: out.println(F("NO RADIO"));              break;
    }

    out.print(F("sent/fail : "));
    out.print(_sent);
    out.print('/');
    out.println(_failed);
#else
    out.println(F("--- wireless disabled (ENABLE_NRF) ---"));
#endif
}
