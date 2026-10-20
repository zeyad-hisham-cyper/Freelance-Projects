/*
 * NRF24.h
 * ------------------------------------------------------------------------
 * SHARED FILE. An identical copy lives in the other firmware:
 *      UniversalWeighingController/src/drivers/NRF24.h
 *      WirelessDashboard/src/drivers/NRF24.h
 * They are separate Arduino sketches and cannot share a directory. Edit
 * both together; diff them whenever the two boards stop understanding
 * each other.
 * ------------------------------------------------------------------------
 * Register-level driver for the Nordic nRF24L01+ 2.4 GHz transceiver.
 *
 * WHY NOT THE RF24 LIBRARY:
 * The rest of this project talks to its hardware directly (ADS1232, HX711)
 * so that every timing rule and every register write is visible in the
 * repository and can be reasoned about during a bring-up. Pulling in a
 * 3000-line third-party library for a chip with 24 registers would be the
 * only opaque box in the firmware. The API below is deliberately close to
 * RF24's, so anyone who knows that library will recognise it.
 *
 * WHAT THIS DRIVER DOES NOT DO:
 *   - No interrupt handling. The IRQ pin is not connected on this board
 *     (see the pinout), so send() polls STATUS. At a 200 ms transmit
 *     interval that costs nothing.
 *   - No multiceiver fan-in beyond the six hardware pipes, no ACK payloads.
 *     A weighing indicator sends one small packet one way.
 *
 * TIMING RULES THE DATASHEET IMPOSES (all handled inside):
 *   - Power-down -> standby needs 1.5 ms (Tpd2stby); we allow 5 ms.
 *   - CE must be held high for at least 10 us to trigger a transmission.
 *   - After a transmission the chip needs 130 us to return to standby
 *     (Tstby2a); polling STATUS covers this.
 *
 * SHARED SPI BUS: see SpiBus.h. CSN idles HIGH, every access is inside an
 * SPI transaction, and the MAX7219 cannot drive MISO — so the two devices
 * coexist without arbitration.
 *
 * POWER WARNING (the single most common nRF24 fault):
 * the module draws current in short bursts that an ESP32's 3.3 V regulator
 * often cannot supply cleanly. If begin() succeeds but transmissions fail
 * with MaxRetries, solder a 10 uF capacitor directly across the module's
 * VCC/GND pins before suspecting the firmware. Never feed VCC from 5 V.
 * ------------------------------------------------------------------------
 */

#ifndef NRF24_H
#define NRF24_H

#include <Arduino.h>
#include "../config/config.h"

#ifdef ENABLE_NRF

class NRF24 {
public:
    // Why a send() failed, or why the driver refused to try.
    enum class Error : uint8_t {
        None = 0,
        NotInitialised,   // begin() not called, or it failed
        NotConnected,     // no chip answering on SPI
        WrongMode,        // send() while listening, read() while transmitting
        PayloadTooLarge,  // more than 32 bytes, or more than the fixed size
        MaxRetries,       // sent, but no auto-acknowledge came back
        TxTimeout,        // the chip never raised TX_DS or MAX_RT at all
        NoData            // read() with an empty RX FIFO
    };

    enum class Mode : uint8_t {
        Transmit,   // PRIM_RX = 0, CE low except during a burst
        Receive     // PRIM_RX = 1, CE held high
    };

    // Lower rates carry further; 250 kbps is the most robust and is plenty
    // for a 16-byte weight packet a few times a second.
    enum class DataRate : uint8_t {
        Rate1Mbps = 0,
        Rate2Mbps,
        Rate250kbps
    };

    enum class Power : uint8_t {
        Min = 0,   // -18 dBm
        Low,       // -12 dBm
        High,      //  -6 dBm
        Max        //   0 dBm
    };

    static const uint8_t MAX_PAYLOAD = 32;
    static const uint8_t MAX_CHANNEL = 125;

    NRF24();

    // Brings up the shared SPI bus and configures the radio from the values
    // in hardware.h. Returns false when no chip answers (see isChipConnected)
    // — in that case every later call fails fast with Error::NotConnected
    // rather than pretending to transmit.
    bool begin();
    bool begin(uint8_t cePin, uint8_t csnPin);

    bool isReady() const { return _ready; }

    // Probes for a live chip by writing a known value to a scratch register
    // and reading it back. Detects an unpowered module, a swapped CE/CSN, a
    // broken MISO line or a bus the MAX7219 is holding.
    bool isChipConnected();

    // --------------------------------------------------------- Configuration
    //
    // All of these may be called before or after begin(); calling them after
    // takes effect immediately. Changing the mode-sensitive ones (address,
    // payload size) while listening is safe — they are re-applied here.

    void setChannel(uint8_t channel);            // 0..125 (2400 + n MHz)
    uint8_t channel() const { return _channel; }

    void setDataRate(DataRate rate);
    void setPowerLevel(Power level);

    // Auto-retransmit: `delay` is in units of 250 us (0..15, so 250 us to
    // 4 ms) and `count` is 0..15 attempts. The delay must exceed the time
    // the far end needs to turn its own radio around.
    void setRetries(uint8_t delay250us, uint8_t count);

    void setAddressWidth(uint8_t bytes);         // 3, 4 or 5
    uint8_t addressWidth() const { return _addrWidth; }

    // Fixed payload size, 1..32 bytes. Dynamic payloads are not used: both
    // ends of this link exchange one struct of a known size, and a fixed
    // size removes a whole class of length-mismatch bugs.
    void setPayloadSize(uint8_t bytes);
    uint8_t payloadSize() const { return _payloadSize; }

    // The address this radio transmits to. Pipe 0's receive address is set
    // to match, which is what the auto-acknowledge mechanism requires.
    void openWritingPipe(const uint8_t *address);

    // pipe 0..5. Pipes 2-5 share the four most significant address bytes
    // with pipe 1 and differ only in the last byte — a hardware restriction,
    // not a driver one.
    void openReadingPipe(uint8_t pipe, const uint8_t *address);

    // Switches direction. Receive holds CE high; Transmit drops it and
    // flushes the TX FIFO so a stale payload cannot go out later.
    void setMode(Mode mode);
    Mode mode() const { return _mode; }

    // ------------------------------------------------------------- Transfer

    // Blocking send with auto-acknowledge. Returns Error::None only when the
    // far end acknowledged. Worst case blocks for retries x (delay + airtime),
    // which with the defaults in hardware.h is a few milliseconds.
    Error send(const void *data, uint8_t length);

    // Fire-and-forget: no acknowledge, no retries, returns as soon as the
    // payload is clocked out. For a periodic weight broadcast where the next
    // reading is more useful than a retransmission of the last one.
    Error sendNoAck(const void *data, uint8_t length);

    // True when a payload is waiting. `pipe`, if given, receives the pipe
    // number it arrived on.
    bool available(uint8_t *pipe = nullptr);

    // Copies one payload out of the RX FIFO. `length` must not exceed the
    // configured payload size.
    Error read(void *buffer, uint8_t length);

    // ---------------------------------------------------------- Diagnostics

    Error lastError() const { return _lastError; }
    static const char *errorText(Error error);

    uint8_t status();          // STATUS register, refreshed by a NOP
    uint8_t fifoStatus();
    uint8_t lostPackets();     // PLOS_CNT: packets given up on since last
                               // channel change (saturates at 15)
    uint8_t retransmits();     // ARC_CNT: retries used by the last packet
    void    resetLostPackets();

    // Received Power Detector: true when a signal above -64 dBm was present
    // during the last reception. The nearest thing this chip has to an RSSI.
    bool carrierDetected();

    // Reads a register directly — for a bring-up sketch or a fault report.
    uint8_t readRegister(uint8_t reg);

    // Dumps the configuration and both addresses to Serial. Costs nothing
    // when DEBUG_SERIAL is off, because nothing calls it.
    void dumpRegisters(Stream &out);

    void powerUp();
    void powerDown();
    void flushTx();
    void flushRx();

private:
    uint8_t  _cePin;
    uint8_t  _csnPin;
    uint8_t  _channel;
    uint8_t  _addrWidth;
    uint8_t  _payloadSize;
    Mode     _mode;
    bool     _ready;
    Error    _lastError;
    uint8_t  _txAddress[5];
    uint32_t _sendTimeoutUs;   // derived from the retry settings

    // --- SPI primitives -------------------------------------------------
    uint8_t command(uint8_t cmd);
    uint8_t writeRegister(uint8_t reg, uint8_t value);
    uint8_t writeRegister(uint8_t reg, const uint8_t *buffer, uint8_t length);
    void    readRegister(uint8_t reg, uint8_t *buffer, uint8_t length);
    uint8_t writePayload(uint8_t cmd, const void *data, uint8_t length);
    void    readPayload(void *buffer, uint8_t length);

    void beginTransfer();
    void endTransfer();

    void clearStatusFlags(uint8_t mask);
    void recomputeSendTimeout();

    Error transmit(const void *data, uint8_t length, bool requireAck);
};

#endif  // ENABLE_NRF
#endif  // NRF24_H
