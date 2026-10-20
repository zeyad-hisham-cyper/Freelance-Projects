/*
 * MAX7219.h
 * ------------------------------------------------------------------------
 * SHARED FILE. An identical copy lives in the other firmware:
 *      UniversalWeighingController/src/drivers/MAX7219.h
 *      WirelessDashboard/src/drivers/MAX7219.h
 * They are separate Arduino sketches and cannot share a directory. Edit
 * both together; diff them whenever the two boards stop understanding
 * each other.
 * ------------------------------------------------------------------------
 * Driver for a MAX7219 seven-segment display module (SPI).
 *
 * Structured in two layers, both public:
 *
 *   LOW LEVEL   setBrightness / clear / writeDigit / writeChar / writeRaw /
 *               writeText / writeNumber / displayTest
 *               — everything the datasheet offers, usable directly from a
 *                 bring-up sketch.
 *
 *   SCREENS     showBoot / showWeight / showMessage / showCalibrationStep /
 *               showError
 *               — the same semantic calls OledDisplay exposes, so
 *                 DisplayManager can swap panels with a #ifdef and nothing
 *                 above it changes.
 *
 * DESIGN NOTE — why the screen composition lives in the DRIVER:
 * see the same note in OledDisplay.h. Six digits cannot render "a title and
 * two lines of instructions", so deciding what "show a calibration step"
 * looks like on THIS panel is this file's job.
 *
 * DESIGN NOTE — why Code B decode is switched OFF:
 * The MAX7219's built-in BCD decoder can only draw 0-9, '-', 'E', 'H', 'L',
 * 'P' and blank. A weighing indicator has to write "CAL", "tArE", "Err" and
 * "OL" too, so this driver runs in no-decode mode and carries its own
 * segment font (see segmentsFor()). One rendering path, no per-digit decode
 * mask to keep in sync.
 *
 * HARDWARE NOTES:
 *
 *  - The module is powered from 5 V while the ESP32 drives DIN/CLK/CS at
 *    3.3 V. The MAX7219 specifies V_IH = 3.5 V at a 5 V supply, so this is
 *    *out of spec* even though it works on most boards.
 *
 *    THREE SYMPTOMS THAT ALL MEAN "THE LEVELS OR THE WIRING ARE MARGINAL":
 *      - digits flicker or show occasional garbage;
 *      - the panel works, then lights every segment and stays that way
 *        (a corrupted word landed on the display-test register);
 *      - the panel works, then freezes on the last value (a corrupted word
 *        landed on shutdown or scan limit).
 *
 *    refreshConfig() below makes the last two self-heal within a second,
 *    but it is a net, not a cure. The cure is a 74AHCT125 level shifter on
 *    DIN/CLK/CS, or running the module at 3.3 V and accepting dimmer
 *    digits, plus short leads and a lower MAX7219_SPI_HZ.
 *
 *  - The MAX7219 has no data output, which is why it can share MISO-bearing
 *    SPI traffic with the nRF24 without a bus conflict. See SpiBus.h.
 *
 *  - Digit registers are numbered from the chip's point of view (DIG0..DIG7)
 *    and how they map onto the physical digits depends on the module's PCB.
 *    Every position in this API is counted from the LEFT of the panel as the
 *    operator sees it; MAX7219_DIGIT0_IS_RIGHTMOST in hardware.h performs
 *    the translation. If your text comes out mirrored, flip that one define.
 * ------------------------------------------------------------------------
 */

#ifndef MAX7219_H
#define MAX7219_H

#include <Arduino.h>
#include "../config/config.h"

#ifdef DISPLAY_7SEG

class MAX7219 {
public:
    // Register map (datasheet Table 2). Public because a bring-up sketch may
    // legitimately want to poke a register this class does not wrap.
    enum Reg : uint8_t {
        REG_NOOP        = 0x00,
        REG_DIGIT0      = 0x01,   // ... through REG_DIGIT0 + 7
        REG_DECODE_MODE = 0x09,
        REG_INTENSITY   = 0x0A,
        REG_SCAN_LIMIT  = 0x0B,
        REG_SHUTDOWN    = 0x0C,
        REG_DISPLAY_TEST = 0x0F
    };

    // Segment bit positions in a no-decode digit register.
    //   bit7 = DP, bit6 = A ... bit0 = G
    enum Seg : uint8_t {
        SEG_G  = 0x01,
        SEG_F  = 0x02,
        SEG_E  = 0x04,
        SEG_D  = 0x08,
        SEG_C  = 0x10,
        SEG_B  = 0x20,
        SEG_A  = 0x40,
        SEG_DP = 0x80
    };

    static const uint8_t MAX_DIGITS = 8;

    MAX7219();

    // Brings up the shared SPI bus and the chip using the values in
    // pins.h / hardware.h. Returns false only if the configuration is
    // impossible (bad digit count) — the MAX7219 is write-only, so a missing
    // or unpowered module is indistinguishable from a working one on the bus
    // and cannot be detected here. Use runSelfTest() for that.
    bool begin();
    bool begin(uint8_t csPin, uint8_t digitCount);

    bool isReady() const { return _ready; }
    uint8_t digitCount() const { return _digits; }

    // ---------------------------------------------------------------- Low level

    // 0 = dimmest (1/32 duty), 15 = brightest (31/32 duty).
    void setBrightness(uint8_t level);
    uint8_t brightness() const { return _brightness; }

    // Blanks every digit. Does not power the chip down.
    void clear();

    // true  = normal operation
    // false = shutdown mode (display blanked, registers retained, ~150 uA)
    void setPowerOn(bool on);

    // Lights every segment of every digit at full brightness, bypassing the
    // digit registers. The datasheet's own lamp test.
    void displayTest(bool on);

    // Blocking bring-up sequence for the bench: lamp test, then a walking
    // segment, then all digits 0-9, then clear. Proves wiring, digit order
    // and brightness in one call.
    void runSelfTest(uint16_t stepMs = 120);

    // Re-sends every control register — decode mode, scan limit, intensity,
    // display test and shutdown — restoring the state this driver believes
    // the chip is in.
    //
    // WHY THIS EXISTS: the MAX7219 cannot be read back, so a control
    // register corrupted by a mis-clocked word stays corrupted forever. The
    // classic symptom is a panel that works, then suddenly lights every
    // segment of every digit and stays that way: one bad word landed on the
    // display-test register (0x0F) with a non-zero data byte. Rewriting the
    // digits does not clear it, because display test overrides them.
    //
    // writeRaw() calls this automatically every MAX7219_CONFIG_REFRESH_MS,
    // so any such corruption self-heals within one refresh period instead of
    // needing a power cycle. It re-asserts the INTENDED power and test
    // state, so setPowerOn(false) and displayTest(true) are not undone.
    void refreshConfig();

    // pos is counted from the LEFT of the panel (0 = leftmost digit).
    void writeRaw(uint8_t pos, uint8_t segments);

    // value 0-9; anything else blanks the position.
    void writeDigit(uint8_t pos, uint8_t value, bool dp = false);

    // Any character the font knows; unknown characters render blank.
    void writeChar(uint8_t pos, char c, bool dp = false);

    // Writes text left-aligned and blanks the rest of the panel. A '.' in
    // the string is folded into the preceding digit's decimal point rather
    // than consuming a digit of its own, so "1.5" needs two digits, not
    // three. Text longer than the panel is truncated.
    void writeText(const char *text);

    // Same, right-aligned — the natural alignment for a number.
    void writeTextRight(const char *text);

    // Renders `value` right-aligned with `decimals` places, dropping
    // decimals as needed to make it fit. Returns false (and shows the
    // overflow pattern) when even zero decimals will not fit.
    bool writeNumber(float value, uint8_t decimals);

    // Reads back the driver's shadow copy of a digit. The MAX7219 itself
    // cannot be read; this is what we last sent.
    uint8_t shadow(uint8_t pos) const;

    // Segment pattern for a character, or 0x00 when the font has no glyph.
    static uint8_t segmentsFor(char c);

    // ------------------------------------------------------------------ Screens

    void showBoot(const char *version);

    // `unitLabel` and `stable` cannot be rendered on a bare six-digit panel
    // and are accepted only to keep the signature identical to OledDisplay's.
    // `tared` lights the leftmost decimal point as the NET annunciator.
    void showWeight(float value, const char *unitLabel, uint8_t decimals,
                    bool stable, bool tared);

    // Only `title` can be shown; the two detail lines are for the OLED.
    void showMessage(const char *title, const char *line1, const char *line2);

    // Renders "C<step>" on the left and the step title (or the value being
    // edited, when there is one) on the right.
    void showCalibrationStep(uint8_t step, uint8_t totalSteps,
                             const char *title,
                             const char *line1, const char *line2,
                             const char *value);

    void showError(const char *title, const char *detail);

private:
    uint8_t  _csPin;
    uint8_t  _digits;
    uint8_t  _brightness;
    bool     _ready;
    bool     _powerOn;             // intended shutdown-register state
    bool     _testMode;            // intended display-test state
    uint32_t _lastConfigMs;        // when refreshConfig() last ran
    uint8_t  _shadow[MAX_DIGITS];  // last pattern sent to each position

    // One 16-bit write: address byte then data byte, latched on CS rising.
    void sendRegister(uint8_t reg, uint8_t value);

    // Calls refreshConfig() if the interval has elapsed. Driven from
    // writeRaw(), so a panel that is being updated keeps healing itself and
    // one that is idle costs nothing.
    void maybeRefreshConfig();

    // Panel position (0 = leftmost) -> chip digit register.
    uint8_t regForPos(uint8_t pos) const;

    // Writes `text` into the panel starting at `startPos`, folding '.' into
    // the previous digit. Returns the number of positions consumed.
    uint8_t renderText(const char *text, uint8_t startPos, uint8_t maxPositions);

    // Blanks the window [startPos, startPos+count) and writes `text`
    // right-aligned inside it. Text too wide for the window is written from
    // startPos and truncated at the right.
    void renderRight(const char *text, uint8_t startPos, uint8_t count);

    // How many panel positions `text` needs once '.' folding is accounted for.
    static uint8_t textWidth(const char *text);

    // "  OL  " style pattern for a value that does not fit.
    void showOverflow();
};

#endif  // DISPLAY_7SEG
#endif  // MAX7219_H
