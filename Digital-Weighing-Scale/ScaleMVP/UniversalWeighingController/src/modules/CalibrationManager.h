/*
 * CalibrationManager.h
 * ------------------------------------------------------------------------
 * Owns everything that turns raw ADC counts into grams:
 *
 *   - the zero offset and the scale factor
 *   - the tare offset
 *   - the interactive calibration procedure
 *   - persisting all of the above through StorageManager
 *
 * MEASUREMENT CHAIN (system.md "Measurement Flow"):
 *
 *      raw counts  --grossGrams()-->  gross grams  --netGrams()-->  net grams
 *                      (zero + scale)                 (tare)
 *
 * The tare offset lives here, next to the zero offset, because the two are
 * the same kind of quantity: a subtraction applied before the value is
 * shown. The difference is only that zero is calibrated and persistent
 * while tare is set by the operator and cleared on power-up.
 *
 * PROCEDURE STATE MACHINE
 * The wizard is a state machine driven by the operator's input (next, plus
 * the digit-entry calls) so the application layer only has to forward key
 * events and ask what to draw. No UI strings live in this module.
 *
 * ENTERING THE REFERENCE MASS
 * With three buttons this was a pair of increase/decrease calls that dialled
 * the value with an adaptive step — the best that could be done when the
 * panel had no digits on it. The 4x4 keypad makes that obsolete: the value
 * is typed. What is stored is the typed TEXT, not just the resulting float,
 * because "1.50" and "1.5" are the same number but not the same thing to
 * someone half-way through typing, and a display that rewrites the operator's
 * keystrokes underneath them is maddening to use.
 *
 *   Idle --next--> CaptureZero --next--> PlaceWeight --next--> EnterWeight
 *                                                                  |
 *                                                                 next
 *                                                                  v
 *                                                    Result (Saved / Failed)
 *                                                                  |
 *                                                     (timeout)    v
 *                                                                Idle
 * ------------------------------------------------------------------------
 */

#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"
#include "WeightManager.h"
#include "StorageManager.h"

enum class CalState : uint8_t {
    Idle = 0,
    CaptureZero,   // "remove all weight"
    PlaceWeight,   // "put the reference mass on"
    EnterWeight,   // "dial in how much it is"
    Result         // "saved" / "failed"
};

enum class CalResult : uint8_t {
    None = 0,
    Saved,
    FailedAdc,     // the ADC stopped delivering conversions
    FailedSpan,    // zero and loaded readings too close: no weight applied
    FailedValue    // the typed reference mass was empty or out of range
};

class CalibrationManager {
public:
    CalibrationManager();

    // Wires up collaborators and loads stored calibration (or defaults).
    void begin(WeightManager *weight, StorageManager *storage);

    // ---- Measurement chain -----------------------------------------
    float grossGrams(float rawCounts) const;
    float netGrams(float rawCounts) const;

    // ---- Tare -------------------------------------------------------
    void  setTare(float grossValueGrams);
    void  clearTare();
    bool  isTared() const { return _tareOffsetG != 0.0f; }
    float tareOffset() const { return _tareOffsetG; }

    // ---- Calibration values ----------------------------------------
    bool  isCalibrated() const { return _record.calibrated != 0; }
    float countsPerGram() const { return _record.countsPerGram; }
    float zeroOffset() const { return _record.zeroOffsetCounts; }

    // ---- User settings ---------------------------------------------
    uint8_t storedUnit() const { return _record.unit; }
    void    saveUnit(uint8_t unit);

    // ---- Procedure --------------------------------------------------
    void start();
    void abort();
    void next();        // ENTER key

    // ---- Reference-mass entry (only act in EnterWeight) --------------
    void appendDigit(uint8_t digit);
    void appendPoint();
    void backspace();

    // What the operator has typed so far, ready to display. Never null;
    // reads "0" before the first keystroke so the field is not blank.
    const char *entryText() const;

    // True when entryText() parses to a mass inside the accepted bounds.
    // The UI can use this to warn before ENTER is pressed; next() checks it
    // too, so an out-of-range value cannot reach the maths.
    bool entryValid() const;

    // Drives the Result-screen timeout. Call every loop.
    void update();

    CalState  state() const { return _state; }
    bool      isRunning() const { return _state != CalState::Idle; }
    CalResult result() const { return _result; }
    float     knownWeight() const { return _knownWeightG; }

    // The reference mass used by the STORED calibration. Offered to the
    // operator as a prompt while the entry field is still empty — most
    // recalibrations use the same weight as last time.
    float     lastKnownWeight() const { return _record.knownWeightG; }

    // Step number for the UI ("CAL 2/3"); 0 while idle.
    uint8_t stepNumber() const;
    static uint8_t totalSteps() { return 3; }

private:
    WeightManager  *_weight;
    StorageManager *_storage;

    ConfigRecord _record;
    float        _tareOffsetG;

    CalState  _state;
    CalResult _result;
    uint32_t  _resultShownAt;

    float _knownWeightG;
    float _zeroCapture;      // counts captured with an empty platform
    bool  _zeroCaptured;

    // The typed reference mass. Nine characters is enough for the largest
    // scale this firmware claims to support (10 000 000 g) with a decimal
    // point and a terminator to spare.
    static const uint8_t ENTRY_MAX = 10;
    char    _entry[ENTRY_MAX + 1];
    uint8_t _entryLen;

    void clearEntry();
    void syncEntryValue();   // _entry -> _knownWeightG

    void finish();           // capture loaded point, compute, store
};

#endif // CALIBRATION_MANAGER_H
