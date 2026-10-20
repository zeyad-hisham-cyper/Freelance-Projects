/*
 * NRF24.cpp
 * ------------------------------------------------------------------------
 * SHARED FILE. An identical copy lives in the other firmware:
 *      UniversalWeighingController/src/drivers/NRF24.cpp
 *      WirelessDashboard/src/drivers/NRF24.cpp
 * They are separate Arduino sketches and cannot share a directory. Edit
 * both together; diff them whenever the two boards stop understanding
 * each other.
 * ------------------------------------------------------------------------
 * See NRF24.h for design rationale, timing rules and the power warning.
 * ------------------------------------------------------------------------
 */

#include "NRF24.h"

#ifdef ENABLE_NRF

#include "SpiBus.h"

// The nRF24L01+ is rated to 10 MHz. 8 MHz matches what the MAX7219 uses on
// the same bus and leaves margin for the flying leads most modules are
// wired with.
static const SPISettings NRF24_SPI(NRF_SPI_HZ, MSBFIRST, SPI_MODE0);

// ---------------------------------------------------------------- Commands
static const uint8_t CMD_R_REGISTER         = 0x00;
static const uint8_t CMD_W_REGISTER         = 0x20;
static const uint8_t REGISTER_MASK          = 0x1F;
static const uint8_t CMD_R_RX_PAYLOAD       = 0x61;
static const uint8_t CMD_W_TX_PAYLOAD       = 0xA0;
static const uint8_t CMD_FLUSH_TX           = 0xE1;
static const uint8_t CMD_FLUSH_RX           = 0xE2;
static const uint8_t CMD_ACTIVATE           = 0x50;
static const uint8_t CMD_W_TX_PAYLOAD_NOACK = 0xB0;
static const uint8_t CMD_NOP                = 0xFF;

// --------------------------------------------------------------- Registers
static const uint8_t REG_CONFIG      = 0x00;
static const uint8_t REG_EN_AA       = 0x01;
static const uint8_t REG_EN_RXADDR   = 0x02;
static const uint8_t REG_SETUP_AW    = 0x03;
static const uint8_t REG_SETUP_RETR  = 0x04;
static const uint8_t REG_RF_CH       = 0x05;
static const uint8_t REG_RF_SETUP    = 0x06;
static const uint8_t REG_STATUS      = 0x07;
static const uint8_t REG_OBSERVE_TX  = 0x08;
static const uint8_t REG_RPD         = 0x09;
static const uint8_t REG_RX_ADDR_P0  = 0x0A;
static const uint8_t REG_TX_ADDR     = 0x10;
static const uint8_t REG_RX_PW_P0    = 0x11;
static const uint8_t REG_FIFO_STATUS = 0x17;
static const uint8_t REG_DYNPD       = 0x1C;
static const uint8_t REG_FEATURE     = 0x1D;

// --------------------------------------------------------------- Bit fields
static const uint8_t CONFIG_MASK_ALL_IRQ = 0x70;   // IRQ pin unused on this board
static const uint8_t CONFIG_EN_CRC       = 0x08;
static const uint8_t CONFIG_CRCO         = 0x04;   // 2-byte CRC
static const uint8_t CONFIG_PWR_UP       = 0x02;
static const uint8_t CONFIG_PRIM_RX      = 0x01;

static const uint8_t STATUS_RX_DR   = 0x40;
static const uint8_t STATUS_TX_DS   = 0x20;
static const uint8_t STATUS_MAX_RT  = 0x10;
static const uint8_t STATUS_RX_P_NO = 0x0E;

static const uint8_t FIFO_RX_EMPTY = 0x01;

static const uint8_t RF_SETUP_DR_LOW  = 0x20;
static const uint8_t RF_SETUP_DR_HIGH = 0x08;
static const uint8_t RF_SETUP_PWR     = 0x06;

static const uint8_t FEATURE_EN_DYN_ACK = 0x01;

NRF24::NRF24()
    : _cePin(PIN_NRF_CE),
      _csnPin(PIN_NRF_CSN),
      _channel(NRF_CHANNEL),
      _addrWidth(NRF_ADDRESS_WIDTH),
      _payloadSize(NRF_PAYLOAD_SIZE),
      _mode(Mode::Transmit),
      _ready(false),
      _lastError(Error::NotInitialised),
      _sendTimeoutUs(15000) {
    for (uint8_t i = 0; i < sizeof(_txAddress); ++i) {
        _txAddress[i] = 0;
    }
}

// =====================================================================
//  SPI PRIMITIVES
// =====================================================================

void NRF24::beginTransfer() {
    SPI.beginTransaction(NRF24_SPI);
    digitalWrite(_csnPin, LOW);
}

void NRF24::endTransfer() {
    digitalWrite(_csnPin, HIGH);
    SPI.endTransaction();
}

// Every SPI exchange returns the STATUS register in the first byte, which is
// why so many of these helpers hand it back.
uint8_t NRF24::command(uint8_t cmd) {
    beginTransfer();
    uint8_t st = SPI.transfer(cmd);
    endTransfer();
    return st;
}

uint8_t NRF24::readRegister(uint8_t reg) {
    beginTransfer();
    SPI.transfer((uint8_t)(CMD_R_REGISTER | (reg & REGISTER_MASK)));
    uint8_t value = SPI.transfer(CMD_NOP);
    endTransfer();
    return value;
}

void NRF24::readRegister(uint8_t reg, uint8_t *buffer, uint8_t length) {
    beginTransfer();
    SPI.transfer((uint8_t)(CMD_R_REGISTER | (reg & REGISTER_MASK)));
    for (uint8_t i = 0; i < length; ++i) {
        buffer[i] = SPI.transfer(CMD_NOP);
    }
    endTransfer();
}

uint8_t NRF24::writeRegister(uint8_t reg, uint8_t value) {
    beginTransfer();
    uint8_t st = SPI.transfer((uint8_t)(CMD_W_REGISTER | (reg & REGISTER_MASK)));
    SPI.transfer(value);
    endTransfer();
    return st;
}

uint8_t NRF24::writeRegister(uint8_t reg, const uint8_t *buffer, uint8_t length) {
    beginTransfer();
    uint8_t st = SPI.transfer((uint8_t)(CMD_W_REGISTER | (reg & REGISTER_MASK)));
    for (uint8_t i = 0; i < length; ++i) {
        SPI.transfer(buffer[i]);
    }
    endTransfer();
    return st;
}

uint8_t NRF24::writePayload(uint8_t cmd, const void *data, uint8_t length) {
    const uint8_t *bytes = (const uint8_t *)data;

    beginTransfer();
    uint8_t st = SPI.transfer(cmd);
    for (uint8_t i = 0; i < length; ++i) {
        SPI.transfer(bytes[i]);
    }
    // Static payload width: the far end is configured to expect exactly
    // _payloadSize bytes, so a short packet has to be padded rather than
    // silently arriving as a different length.
    for (uint8_t i = length; i < _payloadSize; ++i) {
        SPI.transfer(0x00);
    }
    endTransfer();
    return st;
}

void NRF24::readPayload(void *buffer, uint8_t length) {
    uint8_t *bytes = (uint8_t *)buffer;

    beginTransfer();
    SPI.transfer(CMD_R_RX_PAYLOAD);
    for (uint8_t i = 0; i < _payloadSize; ++i) {
        uint8_t b = SPI.transfer(CMD_NOP);
        if (i < length) {
            bytes[i] = b;   // bytes past the caller's buffer are discarded,
                            // but must still be clocked out of the FIFO
        }
    }
    endTransfer();
}

void NRF24::clearStatusFlags(uint8_t mask) {
    writeRegister(REG_STATUS, mask);
}

// =====================================================================
//  BRING-UP
// =====================================================================

bool NRF24::begin() {
    return begin(PIN_NRF_CE, PIN_NRF_CSN);
}

bool NRF24::begin(uint8_t cePin, uint8_t csnPin) {
    _cePin  = cePin;
    _csnPin = csnPin;

    // CSN must idle HIGH before the bus starts, or the radio will interpret
    // the MAX7219's traffic as commands addressed to it.
    pinMode(_csnPin, OUTPUT);
    digitalWrite(_csnPin, HIGH);
    pinMode(_cePin, OUTPUT);
    digitalWrite(_cePin, LOW);

    SpiBus::begin();

    // Tpor (100 ms from power-on) has long since elapsed by the time setup()
    // reaches here, but the chip still needs a moment after the bus settles.
    delay(5);

    if (!isChipConnected()) {
        _ready     = false;
        _lastError = Error::NotConnected;
        return false;
    }

    // Configure with the radio powered DOWN, so nothing is transmitted with
    // a half-written configuration.
    writeRegister(REG_CONFIG, (uint8_t)(CONFIG_MASK_ALL_IRQ | CONFIG_EN_CRC | CONFIG_CRCO));

    setAddressWidth(NRF_ADDRESS_WIDTH);
    setChannel(NRF_CHANNEL);
    setDataRate((DataRate)NRF_DATA_RATE);
    setPowerLevel((Power)NRF_PA_LEVEL);
    setRetries(NRF_RETRY_DELAY, NRF_RETRY_COUNT);
    setPayloadSize(NRF_PAYLOAD_SIZE);

    writeRegister(REG_EN_AA, 0x3F);       // auto-ack on every pipe
    writeRegister(REG_EN_RXADDR, 0x03);   // pipes 0 and 1 enabled
    writeRegister(REG_DYNPD, 0x00);       // fixed payload width everywhere

    // EN_DYN_ACK is what makes the W_TX_PAYLOAD_NOACK command legal, and it
    // lives in FEATURE. Genuine nRF24L01+ silicon accepts a plain write;
    // several clones need the undocumented ACTIVATE unlock first, so verify
    // the write and unlock only if it did not stick.
    writeRegister(REG_FEATURE, FEATURE_EN_DYN_ACK);
    if (readRegister(REG_FEATURE) != FEATURE_EN_DYN_ACK) {
        beginTransfer();
        SPI.transfer(CMD_ACTIVATE);
        SPI.transfer(0x73);
        endTransfer();
        writeRegister(REG_FEATURE, FEATURE_EN_DYN_ACK);
    }

    flushTx();
    flushRx();
    clearStatusFlags((uint8_t)(STATUS_RX_DR | STATUS_TX_DS | STATUS_MAX_RT));

    powerUp();
    setMode(Mode::Transmit);

    _ready     = true;
    _lastError = Error::None;
    return true;
}

bool NRF24::isChipConnected() {
    // A write/read-back on RF_CH. An absent, unpowered or mis-wired module
    // reads back all-zeros or all-ones on both patterns, and the two
    // complementary values rule out a MISO line stuck at either level.
    uint8_t saved = readRegister(REG_RF_CH);

    writeRegister(REG_RF_CH, 0x2A);
    bool ok = (readRegister(REG_RF_CH) == 0x2A);

    writeRegister(REG_RF_CH, 0x55);
    ok = ok && (readRegister(REG_RF_CH) == 0x55);

    writeRegister(REG_RF_CH, saved);

    // SETUP_AW only ever holds 1, 2 or 3 on real silicon.
    uint8_t aw = (uint8_t)(readRegister(REG_SETUP_AW) & 0x03);
    ok = ok && (aw >= 1 && aw <= 3);

    if (!ok) {
        _lastError = Error::NotConnected;
    }
    return ok;
}

// =====================================================================
//  CONFIGURATION
// =====================================================================

void NRF24::setChannel(uint8_t channel) {
    if (channel > MAX_CHANNEL) channel = MAX_CHANNEL;
    _channel = channel;
    writeRegister(REG_RF_CH, channel);
}

void NRF24::setDataRate(DataRate rate) {
    uint8_t setup = (uint8_t)(readRegister(REG_RF_SETUP) &
                              ~(RF_SETUP_DR_LOW | RF_SETUP_DR_HIGH));

    switch (rate) {
        case DataRate::Rate2Mbps:   setup |= RF_SETUP_DR_HIGH; break;
        case DataRate::Rate250kbps: setup |= RF_SETUP_DR_LOW;  break;
        case DataRate::Rate1Mbps:
        default:                    break;   // both bits clear
    }

    writeRegister(REG_RF_SETUP, setup);

    // Airtime changed, so the send timeout has to follow.
    recomputeSendTimeout();
}

void NRF24::setPowerLevel(Power level) {
    uint8_t setup = (uint8_t)(readRegister(REG_RF_SETUP) & ~RF_SETUP_PWR);
    setup |= (uint8_t)(((uint8_t)level & 0x03) << 1);
    writeRegister(REG_RF_SETUP, setup);
}

void NRF24::setRetries(uint8_t delay250us, uint8_t count) {
    if (delay250us > 15) delay250us = 15;
    if (count > 15) count = 15;

    writeRegister(REG_SETUP_RETR, (uint8_t)((delay250us << 4) | count));
    recomputeSendTimeout();
}

void NRF24::recomputeSendTimeout() {
    uint8_t retr  = readRegister(REG_SETUP_RETR);
    uint32_t ard  = (uint32_t)(((retr >> 4) & 0x0F) + 1) * 250;   // us
    uint32_t arc  = (uint32_t)(retr & 0x0F);

    // Each attempt costs the retransmit delay plus the airtime of a packet
    // and its acknowledge. 1500 us covers a full 32-byte payload even at
    // 250 kbps; the extra 5 ms is slack for a busy loop.
    _sendTimeoutUs = (ard + 1500) * (arc + 1) + 5000;
}

void NRF24::setAddressWidth(uint8_t bytes) {
    if (bytes < 3) bytes = 3;
    if (bytes > 5) bytes = 5;

    _addrWidth = bytes;
    writeRegister(REG_SETUP_AW, (uint8_t)(bytes - 2));   // 3/4/5 -> 1/2/3
}

void NRF24::setPayloadSize(uint8_t bytes) {
    if (bytes < 1) bytes = 1;
    if (bytes > MAX_PAYLOAD) bytes = MAX_PAYLOAD;

    _payloadSize = bytes;

    // Same fixed width on every pipe: the link carries one struct.
    for (uint8_t pipe = 0; pipe < 6; ++pipe) {
        writeRegister((uint8_t)(REG_RX_PW_P0 + pipe), bytes);
    }
}

void NRF24::openWritingPipe(const uint8_t *address) {
    memcpy(_txAddress, address, _addrWidth);

    writeRegister(REG_TX_ADDR, address, _addrWidth);

    // Pipe 0 must listen on the transmit address, because that is where the
    // auto-acknowledge comes back to. This is why a node that both sends and
    // receives has to re-open reading pipe 0 after changing its TX address.
    writeRegister(REG_RX_ADDR_P0, address, _addrWidth);
}

void NRF24::openReadingPipe(uint8_t pipe, const uint8_t *address) {
    if (pipe > 5) return;

    // Pipes 0 and 1 hold a full address; pipes 2-5 store only the last byte
    // and borrow the rest from pipe 1.
    if (pipe < 2) {
        writeRegister((uint8_t)(REG_RX_ADDR_P0 + pipe), address, _addrWidth);
    } else {
        writeRegister((uint8_t)(REG_RX_ADDR_P0 + pipe), address[0]);
    }

    uint8_t enabled = readRegister(REG_EN_RXADDR);
    writeRegister(REG_EN_RXADDR, (uint8_t)(enabled | (1 << pipe)));
}

void NRF24::setMode(Mode mode) {
    _mode = mode;

    uint8_t config = readRegister(REG_CONFIG);

    if (mode == Mode::Receive) {
        writeRegister(REG_CONFIG, (uint8_t)(config | CONFIG_PRIM_RX));
        clearStatusFlags((uint8_t)(STATUS_RX_DR | STATUS_TX_DS | STATUS_MAX_RT));
        flushRx();

        // CE stays high for as long as we are listening.
        digitalWrite(_cePin, HIGH);
        delayMicroseconds(150);   // Tstby2a, with margin
    } else {
        digitalWrite(_cePin, LOW);
        writeRegister(REG_CONFIG, (uint8_t)(config & ~CONFIG_PRIM_RX));

        // Anything left in the TX FIFO from a failed send would go out on
        // the next CE pulse as a stale reading.
        flushTx();
        delayMicroseconds(150);
    }
}

void NRF24::powerUp() {
    uint8_t config = readRegister(REG_CONFIG);
    if (config & CONFIG_PWR_UP) {
        return;
    }

    writeRegister(REG_CONFIG, (uint8_t)(config | CONFIG_PWR_UP));
    delay(5);   // Tpd2stby is 1.5 ms; 5 ms costs nothing at boot
}

void NRF24::powerDown() {
    digitalWrite(_cePin, LOW);
    uint8_t config = readRegister(REG_CONFIG);
    writeRegister(REG_CONFIG, (uint8_t)(config & ~CONFIG_PWR_UP));
}

void NRF24::flushTx() { command(CMD_FLUSH_TX); }
void NRF24::flushRx() { command(CMD_FLUSH_RX); }

// =====================================================================
//  TRANSFER
// =====================================================================

NRF24::Error NRF24::send(const void *data, uint8_t length) {
    return transmit(data, length, true);
}

NRF24::Error NRF24::sendNoAck(const void *data, uint8_t length) {
    return transmit(data, length, false);
}

NRF24::Error NRF24::transmit(const void *data, uint8_t length, bool requireAck) {
    if (!_ready) {
        _lastError = Error::NotInitialised;
        return _lastError;
    }
    if (_mode != Mode::Transmit) {
        _lastError = Error::WrongMode;
        return _lastError;
    }
    if (length == 0 || length > _payloadSize) {
        _lastError = Error::PayloadTooLarge;
        return _lastError;
    }

    clearStatusFlags((uint8_t)(STATUS_TX_DS | STATUS_MAX_RT));

    writePayload(requireAck ? CMD_W_TX_PAYLOAD : CMD_W_TX_PAYLOAD_NOACK,
                 data, length);

    // A CE pulse of at least 10 us starts the transmission; the chip returns
    // to standby by itself afterwards, so CE does not stay high.
    digitalWrite(_cePin, HIGH);
    delayMicroseconds(15);
    digitalWrite(_cePin, LOW);

    // The IRQ line is not wired on this board, so poll STATUS. Without
    // auto-ack the chip raises TX_DS as soon as the packet is out, so this
    // loop is microseconds long in the NoAck case.
    uint32_t started = micros();
    uint8_t  st      = 0;
    do {
        st = status();
        if (st & (STATUS_TX_DS | STATUS_MAX_RT)) {
            break;
        }
    } while ((uint32_t)(micros() - started) < _sendTimeoutUs);

    if (!(st & (STATUS_TX_DS | STATUS_MAX_RT))) {
        // Neither flag ever came up: the radio stopped responding mid-send,
        // which in practice means the supply browned out under TX current.
        flushTx();
        clearStatusFlags((uint8_t)(STATUS_TX_DS | STATUS_MAX_RT));
        _lastError = Error::TxTimeout;
        return _lastError;
    }

    clearStatusFlags((uint8_t)(STATUS_TX_DS | STATUS_MAX_RT));

    if (st & STATUS_MAX_RT) {
        // The payload is still in the FIFO after MAX_RT and would block
        // every later send until it is flushed.
        flushTx();
        _lastError = Error::MaxRetries;
        return _lastError;
    }

    _lastError = Error::None;
    return _lastError;
}

bool NRF24::available(uint8_t *pipe) {
    if (!_ready) {
        return false;
    }

    // FIFO_STATUS rather than the RX_DR flag: RX_DR is a latched interrupt
    // and can be set with an empty FIFO (or clear with a full one) after
    // several packets arrive back to back.
    if (fifoStatus() & FIFO_RX_EMPTY) {
        return false;
    }

    if (pipe) {
        *pipe = (uint8_t)((status() & STATUS_RX_P_NO) >> 1);
    }
    return true;
}

NRF24::Error NRF24::read(void *buffer, uint8_t length) {
    if (!_ready) {
        _lastError = Error::NotInitialised;
        return _lastError;
    }
    if (length > _payloadSize) {
        _lastError = Error::PayloadTooLarge;
        return _lastError;
    }
    if (!available(nullptr)) {
        _lastError = Error::NoData;
        return _lastError;
    }

    readPayload(buffer, length);
    clearStatusFlags(STATUS_RX_DR);

    _lastError = Error::None;
    return _lastError;
}

// =====================================================================
//  DIAGNOSTICS
// =====================================================================

uint8_t NRF24::status() {
    return command(CMD_NOP);
}

uint8_t NRF24::fifoStatus() {
    return readRegister(REG_FIFO_STATUS);
}

uint8_t NRF24::lostPackets() {
    return (uint8_t)((readRegister(REG_OBSERVE_TX) >> 4) & 0x0F);
}

uint8_t NRF24::retransmits() {
    return (uint8_t)(readRegister(REG_OBSERVE_TX) & 0x0F);
}

void NRF24::resetLostPackets() {
    // PLOS_CNT is cleared only by writing RF_CH — an oddity of the chip.
    writeRegister(REG_RF_CH, _channel);
}

bool NRF24::carrierDetected() {
    return (readRegister(REG_RPD) & 0x01) != 0;
}

const char *NRF24::errorText(Error error) {
    switch (error) {
        case Error::None:            return "ok";
        case Error::NotInitialised:  return "not initialised";
        case Error::NotConnected:    return "no chip on SPI";
        case Error::WrongMode:       return "wrong TX/RX mode";
        case Error::PayloadTooLarge: return "payload too large";
        case Error::MaxRetries:      return "no ack (max retries)";
        case Error::TxTimeout:       return "TX timeout";
        case Error::NoData:          return "RX empty";
        default:                     return "?";
    }
}

void NRF24::dumpRegisters(Stream &out) {
    uint8_t address[5];

    out.println(F("--- nRF24L01+ ---"));

    out.print(F("connected : "));
    out.println(isChipConnected() ? F("yes") : F("NO - check wiring/3V3"));

    out.print(F("mode      : "));
    out.println(_mode == Mode::Receive ? F("RX") : F("TX"));

    uint8_t ch = readRegister(REG_RF_CH);
    out.print(F("channel   : "));
    out.print(ch);
    out.print(F("  ("));
    out.print(2400 + ch);
    out.println(F(" MHz)"));

    uint8_t setup = readRegister(REG_RF_SETUP);
    out.print(F("data rate : "));
    if (setup & RF_SETUP_DR_LOW)       out.println(F("250 kbps"));
    else if (setup & RF_SETUP_DR_HIGH) out.println(F("2 Mbps"));
    else                               out.println(F("1 Mbps"));

    static const char *const paNames[4] = { "-18 dBm", "-12 dBm", "-6 dBm", "0 dBm" };
    out.print(F("PA level  : "));
    out.println(paNames[(setup & RF_SETUP_PWR) >> 1]);

    out.print(F("payload   : "));
    out.print(_payloadSize);
    out.println(F(" bytes (fixed)"));

    readRegister(REG_TX_ADDR, address, _addrWidth);
    out.print(F("TX addr   : "));
    for (uint8_t i = 0; i < _addrWidth; ++i) {
        if (address[i] < 0x10) out.print('0');
        out.print(address[i], HEX);
    }
    out.println();

    readRegister(REG_RX_ADDR_P0, address, _addrWidth);
    out.print(F("RX addr P0: "));
    for (uint8_t i = 0; i < _addrWidth; ++i) {
        if (address[i] < 0x10) out.print('0');
        out.print(address[i], HEX);
    }
    out.println();

    readRegister(REG_RX_ADDR_P0 + 1, address, _addrWidth);
    out.print(F("RX addr P1: "));
    for (uint8_t i = 0; i < _addrWidth; ++i) {
        if (address[i] < 0x10) out.print('0');
        out.print(address[i], HEX);
    }
    out.println();

    out.print(F("STATUS    : 0x"));
    out.println(status(), HEX);
    out.print(F("FIFO      : 0x"));
    out.println(fifoStatus(), HEX);
    out.print(F("lost/retry: "));
    out.print(lostPackets());
    out.print('/');
    out.println(retransmits());
    out.print(F("last error: "));
    out.println(errorText(_lastError));
    out.println(F("-----------------"));
}

#endif  // ENABLE_NRF
