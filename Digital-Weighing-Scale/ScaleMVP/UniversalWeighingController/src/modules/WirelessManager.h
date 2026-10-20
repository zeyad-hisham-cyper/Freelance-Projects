/*
 * WirelessManager.h
 * ------------------------------------------------------------------------
 * The wireless half of system.md's architecture, on the SENDING side:
 *
 *      NRF Initialization / Packet Transmission / Connection Status
 *
 * It is to NRF24 what DisplayManager is to OledDisplay — the layer that
 * knows what the firmware wants to say, while the driver knows how to say
 * it. The application never sees a register, an address or a pipe.
 *
 * THE RECEIVER IS A SEPARATE FIRMWARE. The remote panel lives in the
 * WirelessDashboard project; the two are independent Arduino sketches that
 * share nothing at build time except the hand-copied files listed in
 * protocol/WeightPacket.h. Nothing in this project decodes a WeightPacket.
 *
 * A dead link is deliberately NOT a fault here: a scale that cannot reach
 * its dashboard is still weighing correctly. Link state is reported in the
 * serial log; only a radio that does not answer on SPI reaches the display.
 * ------------------------------------------------------------------------
 */

#ifndef WIRELESS_MANAGER_H
#define WIRELESS_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"
#include "../protocol/WeightPacket.h"

#ifdef ENABLE_NRF
    #include "../drivers/NRF24.h"
#endif

// What the link is doing, in the three states the operator cares about.
enum class LinkState : uint8_t {
    NoRadio,   // no chip answering on SPI — a wiring or supply fault
    Down,      // radio works, but the dashboard is not there
    Up         // a transmission was acknowledged recently
};

class WirelessManager {
public:
    WirelessManager();

    // Brings the radio up in transmit mode. False means no chip answered;
    // the manager stays usable and keeps retrying in update().
    bool begin();

    // Call every loop. Re-probes a radio that has stopped answering.
    void update();

    // Rate-limited internally to WIRELESS_TX_INTERVAL_MS, so it is safe —
    // and intended — to call on every pass of the main loop. Returns true
    // only when a packet went out AND was acknowledged.
    bool sendWeight(float grams, float displayValue, uint8_t unit,
                    bool stable, bool tared, bool calibrated, bool fault);

    // -------------------------------------------------------- Connection
    LinkState link() const;
    bool isLinkUp() const { return link() == LinkState::Up; }
    bool hasRadio() const { return _radioPresent; }

    // -------------------------------------------------------- Diagnostics
    uint32_t sentCount() const { return _sent; }
    uint32_t failedCount() const { return _failed; }

    // Why the last transmission failed. Kept because send() itself is
    // silent — a marginal link fails too often for per-packet logging.
    const char *lastErrorText() const;

    // Full register dump plus this module's counters.
    void logStatus(Stream &out);

private:
#ifdef ENABLE_NRF
    NRF24 _radio;
#endif

    bool     _radioPresent;
    bool     _linkWasUp;       // for edge-triggered link logging

    uint32_t _lastTxMs;        // last transmission attempt
    uint32_t _lastOkMs;        // last acknowledged transmission
    uint32_t _lastProbeMs;     // last attempt to recover a missing radio

    uint16_t _sequence;
    uint32_t _sent;
    uint32_t _failed;

    uint8_t  _consecutiveFailures;

    void applyRole();
};

#endif  // WIRELESS_MANAGER_H
