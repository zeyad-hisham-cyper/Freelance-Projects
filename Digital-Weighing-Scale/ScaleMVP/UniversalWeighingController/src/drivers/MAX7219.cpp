/*
 * MAX7219.cpp
 * ------------------------------------------------------------------------
 * SHARED FILE. An identical copy lives in the other firmware:
 *      UniversalWeighingController/src/drivers/MAX7219.cpp
 *      WirelessDashboard/src/drivers/MAX7219.cpp
 * They are separate Arduino sketches and cannot share a directory. Edit
 * both together; diff them whenever the two boards stop understanding
 * each other.
 * ------------------------------------------------------------------------
 * See MAX7219.h for design rationale and hardware notes.
 * ------------------------------------------------------------------------
 */

#include "MAX7219.h"

#ifdef DISPLAY_7SEG

#include "SpiBus.h"

// Clock rate comes from hardware.h. The chip is rated to 10 MHz, but the
// 3.3 V -> 5 V level mismatch described in the header means the real limit
// is set by signal integrity, not by the datasheet — see the note on
// MAX7219_SPI_HZ there before raising it.
static const SPISettings MAX7219_SPI(MAX7219_SPI_HZ, MSBFIRST, SPI_MODE0);

MAX7219::MAX7219()
    : _csPin(PIN_MAX7219_CS),
      _digits(MAX7219_DIGITS),
      _brightness(MAX7219_BRIGHTNESS),
      _ready(false),
      _powerOn(false),
      _testMode(false),
      _lastConfigMs(0) {
    for (uint8_t i = 0; i < MAX_DIGITS; ++i) {
        _shadow[i] = 0x00;
    }
}

// =====================================================================
//  BRING-UP
// =====================================================================

bool MAX7219::begin() {
    return begin(PIN_MAX7219_CS, MAX7219_DIGITS);
}

bool MAX7219::begin(uint8_t csPin, uint8_t digitCount) {
    if (digitCount == 0 || digitCount > MAX_DIGITS) {
        _ready = false;
        return false;
    }

    _csPin  = csPin;
    _digits = digitCount;

    // CS must be HIGH before the bus comes up: the MAX7219 latches on the
    // rising edge, and a floating CS during SPI.begin() would latch noise.
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);

    SpiBus::begin();

    // Datasheet power-on defaults are undefined, so every register that
    // matters is written explicitly, with the display shut down while we do
    // it. That way the panel never briefly shows garbage at boot.
    sendRegister(REG_SHUTDOWN, 0x00);      // shutdown mode
    sendRegister(REG_DISPLAY_TEST, 0x00);  // lamp test off
    sendRegister(REG_DECODE_MODE, 0x00);   // no decode — we own the font
    sendRegister(REG_SCAN_LIMIT, (uint8_t)(_digits - 1));

    setBrightness(_brightness);

    // Blank every digit register, including the ones beyond the scan limit:
    // if someone later raises MAX7219_DIGITS, the extra digits start clean.
    for (uint8_t i = 0; i < MAX_DIGITS; ++i) {
        sendRegister((uint8_t)(REG_DIGIT0 + i), 0x00);
        _shadow[i] = 0x00;
    }

    _powerOn = true;
    sendRegister(REG_SHUTDOWN, 0x01);      // normal operation

    _ready        = true;
    _lastConfigMs = millis();
    return true;
}

// =====================================================================
//  LOW LEVEL
// =====================================================================

void MAX7219::sendRegister(uint8_t reg, uint8_t value) {
    SPI.beginTransaction(MAX7219_SPI);
    digitalWrite(_csPin, LOW);
    SPI.transfer(reg);
    SPI.transfer(value);
    digitalWrite(_csPin, HIGH);   // rising edge latches the 16-bit word
    SPI.endTransaction();
}

uint8_t MAX7219::regForPos(uint8_t pos) const {
#if MAX7219_DIGIT0_IS_RIGHTMOST
    return (uint8_t)(REG_DIGIT0 + (_digits - 1 - pos));
#else
    return (uint8_t)(REG_DIGIT0 + pos);
#endif
}

void MAX7219::setBrightness(uint8_t level) {
    if (level > 15) level = 15;
    _brightness = level;
    sendRegister(REG_INTENSITY, level);
}

void MAX7219::clear() {
    for (uint8_t pos = 0; pos < _digits; ++pos) {
        writeRaw(pos, 0x00);
    }
}

void MAX7219::setPowerOn(bool on) {
    _powerOn = on;
    sendRegister(REG_SHUTDOWN, on ? 0x01 : 0x00);
}

void MAX7219::displayTest(bool on) {
    _testMode = on;
    sendRegister(REG_DISPLAY_TEST, on ? 0x01 : 0x00);
}

void MAX7219::refreshConfig() {
    // Order matters on the way out of a corrupted state: get the chip back
    // into no-decode with the right scan limit BEFORE clearing display test,
    // so the digits that reappear are already the right ones.
    sendRegister(REG_DECODE_MODE, 0x00);
    sendRegister(REG_SCAN_LIMIT, (uint8_t)(_digits - 1));
    sendRegister(REG_INTENSITY, _brightness);
    sendRegister(REG_DISPLAY_TEST, _testMode ? 0x01 : 0x00);
    sendRegister(REG_SHUTDOWN, _powerOn ? 0x01 : 0x00);

    _lastConfigMs = millis();
}

void MAX7219::maybeRefreshConfig() {
#if MAX7219_CONFIG_REFRESH_MS > 0
    if (!_ready) {
        return;
    }
    if (millis() - _lastConfigMs >= MAX7219_CONFIG_REFRESH_MS) {
        refreshConfig();
    }
#endif
}

void MAX7219::runSelfTest(uint16_t stepMs) {
    if (!_ready) return;

    // 1. Lamp test — every segment, every digit, driven by the chip itself.
    //    Proves supply, CS and the panel's common lines.
    displayTest(true);
    delay(stepMs * 4);
    displayTest(false);

    // 2. Walking segment — one segment at a time across every digit.
    //    Proves the digit ordering and each individual segment line.
    static const uint8_t walk[8] = { SEG_A, SEG_B, SEG_C, SEG_D,
                                     SEG_E, SEG_F, SEG_G, SEG_DP };
    for (uint8_t s = 0; s < 8; ++s) {
        for (uint8_t pos = 0; pos < _digits; ++pos) {
            writeRaw(pos, walk[s]);
        }
        delay(stepMs);
    }

    // 3. Count 0-9 on every digit — proves the font path end to end.
    for (uint8_t v = 0; v <= 9; ++v) {
        for (uint8_t pos = 0; pos < _digits; ++pos) {
            writeDigit(pos, v);
        }
        delay(stepMs);
    }

    clear();
}

void MAX7219::writeRaw(uint8_t pos, uint8_t segments) {
    if (pos >= _digits) return;

    // Piggy-backed on the digit writes rather than given its own timer: a
    // panel that is being drawn is a panel worth healing, and an idle one
    // costs nothing.
    maybeRefreshConfig();

    _shadow[pos] = segments;
    sendRegister(regForPos(pos), segments);
}

void MAX7219::writeDigit(uint8_t pos, uint8_t value, bool dp) {
    uint8_t seg = (value <= 9) ? segmentsFor((char)('0' + value)) : 0x00;
    writeRaw(pos, dp ? (uint8_t)(seg | SEG_DP) : seg);
}

void MAX7219::writeChar(uint8_t pos, char c, bool dp) {
    uint8_t seg = segmentsFor(c);
    writeRaw(pos, dp ? (uint8_t)(seg | SEG_DP) : seg);
}

uint8_t MAX7219::shadow(uint8_t pos) const {
    return (pos < _digits) ? _shadow[pos] : 0x00;
}

// ------------------------------------------------------------------ Font
//
// Written as a switch rather than a 96-entry table: the glyphs are sparse,
// and a table indexed by ASCII is exactly the kind of thing that silently
// goes one entry out of step.
//
// Letters a seven-segment display genuinely cannot form (K, M, V, W, X, Z)
// are given the conventional approximations used by industrial indicators
// rather than left blank, so a word stays recognisable.
uint8_t MAX7219::segmentsFor(char c) {
    switch (c) {
        // --- digits ---
        case '0': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
        case '1': return SEG_B | SEG_C;
        case '2': return SEG_A | SEG_B | SEG_D | SEG_E | SEG_G;
        case '3': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_G;
        case '4': return SEG_B | SEG_C | SEG_F | SEG_G;
        case '5': return SEG_A | SEG_C | SEG_D | SEG_F | SEG_G;
        case '6': return SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
        case '7': return SEG_A | SEG_B | SEG_C;
        case '8': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
        case '9': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G;

        // --- punctuation ---
        case ' ': return 0x00;
        case '-': return SEG_G;
        case '_': return SEG_D;
        case '=': return SEG_D | SEG_G;
        case '.': return SEG_DP;
        case '\'': return SEG_F;
        case '"': return SEG_B | SEG_F;
        case '(':
        case '[': return SEG_A | SEG_D | SEG_E | SEG_F;
        case ')':
        case ']': return SEG_A | SEG_B | SEG_C | SEG_D;
        case '?': return SEG_A | SEG_B | SEG_E | SEG_G;
        case '/': return SEG_B | SEG_E | SEG_G;
        case '\\': return SEG_C | SEG_F | SEG_G;
        case '*': return SEG_A | SEG_B | SEG_F | SEG_G;   // degree sign

        // --- letters (case follows what the panel can actually draw) ---
        case 'A': case 'a': return SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
        case 'B': case 'b': return SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
        case 'C':           return SEG_A | SEG_D | SEG_E | SEG_F;
        case 'c':           return SEG_D | SEG_E | SEG_G;
        case 'D': case 'd': return SEG_B | SEG_C | SEG_D | SEG_E | SEG_G;
        case 'E': case 'e': return SEG_A | SEG_D | SEG_E | SEG_F | SEG_G;
        case 'F': case 'f': return SEG_A | SEG_E | SEG_F | SEG_G;
        case 'G': case 'g': return SEG_A | SEG_C | SEG_D | SEG_E | SEG_F;
        case 'H':           return SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
        case 'h':           return SEG_C | SEG_E | SEG_F | SEG_G;
        case 'I':           return SEG_B | SEG_C;
        case 'i':           return SEG_C;
        case 'J': case 'j': return SEG_B | SEG_C | SEG_D | SEG_E;
        case 'L':           return SEG_D | SEG_E | SEG_F;
        case 'l':           return SEG_E | SEG_F;
        case 'N': case 'n': return SEG_C | SEG_E | SEG_G;
        case 'O':           return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
        case 'o':           return SEG_C | SEG_D | SEG_E | SEG_G;
        case 'P': case 'p': return SEG_A | SEG_B | SEG_E | SEG_F | SEG_G;
        case 'Q': case 'q': return SEG_A | SEG_B | SEG_C | SEG_F | SEG_G;
        case 'R': case 'r': return SEG_E | SEG_G;
        case 'S': case 's': return SEG_A | SEG_C | SEG_D | SEG_F | SEG_G;
        case 'T': case 't': return SEG_D | SEG_E | SEG_F | SEG_G;
        case 'U':           return SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
        case 'u':           return SEG_C | SEG_D | SEG_E;
        case 'Y': case 'y': return SEG_B | SEG_C | SEG_D | SEG_F | SEG_G;

        // Approximations — not real glyphs, but readable in context.
        case 'K': case 'k': return SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;   // as H
        case 'M': case 'm': return SEG_A | SEG_B | SEG_C | SEG_E | SEG_F;   // boxy n
        case 'V': case 'v': return SEG_C | SEG_D | SEG_E;                   // as u
        case 'W': case 'w': return SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;   // as U
        case 'X': case 'x': return SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;   // as H
        case 'Z': case 'z': return SEG_A | SEG_B | SEG_D | SEG_E | SEG_G;   // as 2

        default:  return 0x00;
    }
}

// ---------------------------------------------------------------- Text

uint8_t MAX7219::textWidth(const char *text) {
    if (!text) return 0;

    uint8_t width = 0;
    bool folds = false;   // a '.' now would land on the previous digit

    for (const char *p = text; *p; ++p) {
        if (*p == '.' && folds) {
            folds = false;
            continue;     // costs no position of its own
        }
        ++width;
        folds = (*p != '.');
    }
    return width;
}

uint8_t MAX7219::renderText(const char *text, uint8_t startPos, uint8_t maxPositions) {
    if (!text) return 0;

    uint8_t pos   = startPos;
    uint8_t used  = 0;
    bool    folds = false;

    for (const char *p = text; *p; ++p) {
        if (*p == '.' && folds) {
            // Fold the point into the digit we just wrote.
            writeRaw((uint8_t)(pos - 1), (uint8_t)(_shadow[pos - 1] | SEG_DP));
            folds = false;
            continue;
        }

        if (used >= maxPositions || pos >= _digits) {
            break;
        }

        writeChar(pos, *p);
        ++pos;
        ++used;
        folds = (*p != '.');
    }

    return used;
}

void MAX7219::renderRight(const char *text, uint8_t startPos, uint8_t count) {
    if (startPos >= _digits) return;
    if (startPos + count > _digits) {
        count = (uint8_t)(_digits - startPos);
    }

    for (uint8_t i = 0; i < count; ++i) {
        writeRaw((uint8_t)(startPos + i), 0x00);
    }

    uint8_t width = textWidth(text);
    uint8_t at    = (width < count) ? (uint8_t)(startPos + count - width) : startPos;

    renderText(text, at, count);
}

void MAX7219::writeText(const char *text) {
    uint8_t used = renderText(text, 0, _digits);
    for (uint8_t pos = used; pos < _digits; ++pos) {
        writeRaw(pos, 0x00);
    }
}

void MAX7219::writeTextRight(const char *text) {
    renderRight(text, 0, _digits);
}

bool MAX7219::writeNumber(float value, uint8_t decimals) {
    char buf[20];

    // Shed decimals until the number fits. A six-digit panel showing
    // "12345.6 kg" has to become "12346" rather than a meaningless
    // truncation of the leading digits.
    for (;;) {
        dtostrf(value, 0, decimals, buf);

        if (textWidth(buf) <= _digits) {
            writeTextRight(buf);
            return true;
        }

        if (decimals == 0) {
            showOverflow();
            return false;
        }
        --decimals;
    }
}

void MAX7219::showOverflow() {
    clear();

    // "OL" is the conventional overload/out-of-range annunciator, centred.
    uint8_t at = (_digits > 2) ? (uint8_t)((_digits - 2) / 2) : 0;
    writeChar(at, 'O');
    writeChar((uint8_t)(at + 1), 'L');
}

// =====================================================================
//  SCREENS
// =====================================================================

void MAX7219::showBoot(const char *version) {
    if (!_ready) return;

    // A brief lamp test at every power-up: an operator can see at a glance
    // that no segment has died, which matters on a panel that spends its
    // life showing the same few digits.
    displayTest(true);
    delay(250);
    displayTest(false);

    writeTextRight(version);
}

void MAX7219::showWeight(float value, const char *unitLabel, uint8_t decimals,
                         bool stable, bool tared) {
    if (!_ready) return;

    // A bare seven-segment panel has no annunciator LEDs, so the unit and
    // the stability marker have nowhere to go. They stay in the signature so
    // DisplayManager can call OLED and MAX7219 identically.
    (void)unitLabel;
    (void)stable;

    writeNumber(value, decimals);

    // NET indicator: the leftmost decimal point. It is the only spare
    // element on the panel that is not part of the number.
    if (tared) {
        writeRaw(0, (uint8_t)(_shadow[0] | SEG_DP));
    }
}

void MAX7219::showMessage(const char *title, const char *line1, const char *line2) {
    if (!_ready) return;

    // The detail lines belong to the OLED layout; six digits get the title
    // only, left-aligned so a truncated word keeps its recognisable start.
    (void)line1;
    (void)line2;

    writeText(title ? title : "");
}

void MAX7219::showCalibrationStep(uint8_t step, uint8_t totalSteps,
                                  const char *title,
                                  const char *line1, const char *line2,
                                  const char *value) {
    if (!_ready) return;

    (void)totalSteps;   // no room for "2/3" on six digits
    (void)line1;
    (void)line2;

    clear();

    // While a value is being edited it gets the whole panel: on six digits
    // "500 g" fits, but " 500 g" alongside a step marker does not, and a
    // reference mass the operator cannot read in full is useless.
    if (value && *value) {
        writeTextRight(value);
        return;
    }

    // Otherwise "C1" keeps the operator's place in the wizard and the step
    // name takes the rest — "C1ZERO", "C2LOAD".
    writeChar(0, 'C');
    writeDigit(1, step);

    if (_digits > 2) {
        renderRight(title, 2, (uint8_t)(_digits - 2));
    }
}

void MAX7219::showError(const char *title, const char *detail) {
    if (!_ready) return;

    (void)detail;

    // "Err" plus a two-character tag taken from the title, e.g.
    // "ADC FAIL" -> "Err Ad", "OVERLOAD" -> "Err Ov", "CAL FAIL" -> "Err CA".
    // Spelling the whole title out is impossible, and a bare "Err" would not
    // tell the operator which fault they are looking at.
    clear();
    writeChar(0, 'E');
    writeChar(1, 'r');
    writeChar(2, 'r');

    uint8_t pos = 4;
    if (title) {
        bool first = true;
        for (const char *p = title; *p && pos < _digits; ++p) {
            if (!isalnum((unsigned char)*p)) continue;

            // First letter as written, second forced lowercase: "Ad" reads
            // as a word on seven segments where "AD" does not.
            writeChar(pos, first ? *p : (char)tolower((unsigned char)*p));
            first = false;
            ++pos;
        }
    }
}

#endif  // DISPLAY_7SEG
