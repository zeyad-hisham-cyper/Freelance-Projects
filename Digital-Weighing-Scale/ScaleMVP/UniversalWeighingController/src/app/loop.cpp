/*
 * loop.cpp
 * ------------------------------------------------------------------------
 * The main cycle.
 *
 * The loop itself never blocks: the ADC is polled only when a conversion is
 * ready, buttons are debounced by timestamp rather than delay(), and the
 * display is redrawn on an interval instead of every pass. That keeps
 * button response instant even though the ADS1232 runs at 10 SPS.
 *
 * (The single exception is the calibration capture, which is deliberately
 * blocking — see WeightManager::captureAverage.)
 * ------------------------------------------------------------------------
 */

#include "App.h"

void App::loop() {
    // 1. Inputs -------------------------------------------------------
    _input.update();

    // 2. Acquire: read ADC -> filter -> average cells ------------------
    _weight.update();

    // 3. Housekeeping state machines ----------------------------------
    _calibration.update();
    _wireless.update();        // recovers a radio that has stopped answering
    _diagnostics.update();

    // 4. React to the operator ----------------------------------------
    handleKeys();

    // 5. Present ------------------------------------------------------
    if (millis() - _lastDisplayMs >= DISPLAY_REFRESH_MS) {
        _lastDisplayMs = millis();
        refreshDisplay();
    }

    logSerial();

    // 6. Transmit wireless data ---------------------------------------
    // Last, so the packet carries the same reading the local display is
    // already showing. Rate-limited inside WirelessManager.
    transmitWeight();
}

// =====================================================================
//  WIRELESS
// =====================================================================

void App::transmitWeight() {
#ifdef ENABLE_NRF
    // Send even while a fault is active, with the fault flag set: a
    // dashboard that is told "the scale is in trouble" is far more useful
    // than one that simply stops updating for an unknown reason.
    bool valid = _weight.hasValidReading();

    float grams = valid ? _calibration.netGrams(_weight.rawCounts()) : 0.0f;
    float shown = _unit.convert(grams);

    _wireless.sendWeight(grams, shown, (uint8_t)_unit.unit(),
                         valid && _weight.isStable(),
                         _calibration.isTared(),
                         _calibration.isCalibrated(),
                         _diagnostics.hasFault() || !valid);
#endif
}

// =====================================================================
//  KEYPAD HANDLING
// =====================================================================
//
// The key map itself lives in InputManager.h. Only its consequences are
// here.

void App::handleKeys() {
    // Drained in a loop rather than one key per pass: a fast typist
    // entering a reference mass can easily land two digits between two
    // iterations, and dropping one would be silent and maddening.
    char key;
    while (_input.nextKey(key)) {
        // The wizard takes over the whole panel while it runs, so UNIT does
        // not silently change units behind the operator's back mid-procedure.
        if (_calibration.isRunning()) {
            handleCalibrationKey(key);
        } else {
            handleWeighingKey(key);
        }
    }
}

void App::handleWeighingKey(char key) {
    switch (key) {
        // ---- TARE ------------------------------------------------------
        // Store the current gross reading as the tare offset, so the
        // container on the platform stops counting.
        case InputManager::KEY_TARE:
            if (_weight.hasValidReading()) {
                _calibration.setTare(_calibration.grossGrams(_weight.rawCounts()));
                showBanner("TARE", "Zeroed");
            } else {
                showBanner("WAIT", "No reading yet");
            }
            break;

        // ---- CANCEL ----------------------------------------------------
        // Back to gross weighing. This was a long press on TARE when the
        // panel had three buttons; it now has a key of its own.
        case InputManager::KEY_CANCEL:
            if (_calibration.isTared()) {
                _calibration.clearTare();
                showBanner("GROSS", "Tare cleared");
            }
            break;

        // ---- UNIT ------------------------------------------------------
        // Cycles g -> kg -> lb -> oz. Only the displayed value changes; the
        // internal measurement stays in grams.
        case InputManager::KEY_UNIT:
            _unit.next();
            _calibration.saveUnit((uint8_t)_unit.unit());
            LOG("[App] unit = ");
            LOGLN(_unit.label());
            break;

        // ---- ENTER -----------------------------------------------------
        case InputManager::KEY_ENTER:
            _calibration.start();
            _input.flush();   // this press belongs to the old screen
            break;

        default:
            // Digits and punctuation mean nothing on the weighing screen.
            break;
    }
}

void App::handleCalibrationKey(char key) {
    // ---- Abort --------------------------------------------------------
    // Checked first: an operator who wants out should always get out,
    // whatever step they are on.
    if (key == InputManager::KEY_CANCEL) {
        _calibration.abort();
        _input.flush();
        showBanner("CANCEL", "Calibration aborted");
        return;
    }

    // ---- Advance / confirm --------------------------------------------
    if (key == InputManager::KEY_ENTER) {
        _calibration.next();
        return;
    }

    // ---- Type the reference mass ---------------------------------------
    // CalibrationManager ignores these outside the VALUE step, so there is
    // no state check to keep in sync here.
    uint8_t digit;
    if (InputManager::isDigit(key, digit)) {
        _calibration.appendDigit(digit);
        return;
    }

    if (key == InputManager::KEY_POINT) {
        _calibration.appendPoint();
        return;
    }

    if (key == InputManager::KEY_DELETE) {
        _calibration.backspace();
        return;
    }

    // TARE and UNIT are deliberately inert during calibration.
}

// =====================================================================
//  DISPLAY
// =====================================================================

void App::showBanner(const char *title, const char *line, uint32_t durationMs) {
    _messageTitle   = title;
    _messageLine    = line;
    _messageUntilMs = millis() + durationMs;

    // Draw immediately rather than waiting for the refresh tick, so the
    // acknowledgement feels attached to the button press.
    _lastDisplayMs = millis();
    refreshDisplay();
}

void App::refreshDisplay() {
    if (!_display.isReady()) {
        return;
    }

    // Priority order: calibration wizard > hardware fault > banner > weight.
    // The wizard outranks faults because the ADC-failure screen would
    // otherwise hide the very procedure used to diagnose a bad calibration.
    if (_calibration.isRunning()) {
        drawCalibrationScreen();
        return;
    }

    if (_diagnostics.hasFault()) {
        _display.showError(_diagnostics.faultTitle(), _diagnostics.faultDetail());
        return;
    }

    if (bannerActive()) {
        _display.showMessage(_messageTitle, _messageLine,
                             _calibration.isCalibrated() ? nullptr : "Not calibrated");
        return;
    }

    // Until every filter window has filled, the average is computed over
    // too few samples to be worth showing.
    if (!_weight.hasValidReading()) {
        _display.showMessage("WARM UP", "Reading sensor...", nullptr);
        return;
    }

    // --- The measurement chain, in system.md's order ------------------
    float rawCounts = _weight.rawCounts();          // filtered + averaged
    float grams     = _calibration.netGrams(rawCounts); // calibrate + tare
    float shown     = _unit.convert(grams);             // convert unit

    _display.showWeight(shown, _unit.label(), _unit.decimals(),
                        _weight.isStable(), _calibration.isTared());
}

void App::drawCalibrationScreen() {
    switch (_calibration.state()) {
        case CalState::CaptureZero:
            _display.showCalibrationStep(_calibration.stepNumber(), "ZERO",
                                         "Remove all weight",
                                         "C=next    D=exit");
            break;

        case CalState::PlaceWeight:
            _display.showCalibrationStep(_calibration.stepNumber(), "LOAD",
                                         "Place ref. weight",
                                         "C=next    D=exit");
            break;

        case CalState::EnterWeight: {
            // Show the keystrokes, not a re-rendered float: entryText() is
            // exactly what the operator typed, so a half-finished "1." does
            // not flicker back to "1" between digits.
            snprintf(_valueBuf, sizeof(_valueBuf), "%s g", _calibration.entryText());

            // The help line earns its space three different ways: the key
            // legend once the value is usable, the previous reference mass
            // while the field is empty (most recalibrations reuse the same
            // weight), and a plain instruction when there is no previous
            // mass to offer.
            const char *hint;
            if (_calibration.entryValid()) {
                hint = "0-9 type  *=.  #=del";
            } else if (_calibration.lastKnownWeight() >= CAL_WEIGHT_MIN_G) {
                snprintf(_hintBuf, sizeof(_hintBuf), "Last used: %ld g",
                         (long)_calibration.lastKnownWeight());
                hint = _hintBuf;
            } else {
                hint = "Type the mass in g";
            }

            _display.showCalibrationStep(_calibration.stepNumber(), "VALUE",
                                         hint,
                                         "C=save    D=exit",
                                         _valueBuf);
            break;
        }

        case CalState::Result:
            switch (_calibration.result()) {
                case CalResult::Saved:
                    _display.showMessage("SAVED", "Calibration stored", nullptr);
                    break;
                case CalResult::FailedSpan:
                    _display.showError("CAL FAIL", "No weight detected");
                    break;
                case CalResult::FailedAdc:
                    _display.showError("CAL FAIL", "No ADC response");
                    break;
                case CalResult::FailedValue:
                    _display.showError("CAL FAIL", "Bad reference mass");
                    break;
                default:
                    break;
            }
            break;

        case CalState::Idle:
        default:
            break;
    }
}

// =====================================================================
//  SERIAL DIAGNOSTICS
// =====================================================================

void App::logSerial() {
#ifdef DEBUG_SERIAL
    if (millis() - _lastLogMs < 500) {
        return;
    }
    _lastLogMs = millis();

    if (_calibration.isRunning()) {
        return;   // the wizard prints its own progress
    }

    float rawCounts = _weight.rawCounts();

    Serial.print("raw=");
    Serial.print(rawCounts, 0);
    Serial.print("  net=");
    Serial.print(_unit.convert(_calibration.netGrams(rawCounts)), _unit.decimals());
    Serial.print(_unit.label());
    Serial.print(_weight.isStable() ? "  [stable]" : "  [moving]");
    if (_calibration.isTared()) Serial.print("  [net]");
    if (!_calibration.isCalibrated()) Serial.print("  [uncalibrated]");
    if (_weight.timeoutCount()) {
        Serial.print("  timeouts=");
        Serial.print(_weight.timeoutCount());
    }

    // The scale keeps weighing whether or not the dashboard is listening, so
    // the link is reported here and nowhere else.
    //
    // The ok/fail counters matter more than the up/down tag: a link that is
    // "up" while losing half its packets looks fine from the tag alone, and
    // that is exactly the failure worth catching.
#ifdef ENABLE_NRF
    if (!_wireless.hasRadio()) {
        Serial.print("  [no radio]");
    } else {
        Serial.print(_wireless.isLinkUp() ? "  [link]" : "  [no link]");
        Serial.print(" tx=");
        Serial.print(_wireless.sentCount());
        Serial.print('/');
        Serial.print(_wireless.failedCount());
    }
#endif

    Serial.println();
#endif  // DEBUG_SERIAL
}
