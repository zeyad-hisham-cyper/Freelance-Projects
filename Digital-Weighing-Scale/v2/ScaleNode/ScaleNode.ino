/*
 * ScaleNode.ino
 * ------------------------------------------------------------------------
 * Phase 3 — ESP-NOW Transmitter
 *
 * Architecture:
 *   Config.h             -> Hardware definitions & ESP-NOW constants
 *   ADS1232              -> Hardware driver
 *   ScaleReader          -> App-level wrapper around the ADC driver
 *   StabilityDetector    -> Circular buffer Peak-to-Peak check
 *   WeightCalculator     -> Applies zero offset and scaling
 *   CalibrationManager   -> Handles Tare/Cal logic, buttons, LEDs, and NVS
 *   EspNowManager        -> Pairing handshake and weight packet transmission
 *
 * Pairing:
 *   The Scale Node broadcasts a PAIRING_REQ packet every
 *   ESPNOW_PAIRING_INTERVAL_MS until the Controller responds with
 *   PAIRING_ACK. The Controller's MAC is then stored in NVS so subsequent
 *   boots skip the pairing phase immediately.
 * ------------------------------------------------------------------------
 */

#include "Config.h"
#include "ADS1232.h"
#include "ScaleReader.h"
#include "StabilityDetector.h"
#include "WeightCalculator.h"
#include "CalibrationManager.h"
#include "EspNowManager.h"

// ---- Hardware driver ----
ADS1232 adc(PIN_ADS1232_DOUT, PIN_ADS1232_SCLK,
            PIN_ADS1232_PDWN, PIN_ADS1232_SPEED, PIN_ADS1232_A0);

// ---- Application modules ----
ScaleReader         scale(adc);
StabilityDetector   stability;
WeightCalculator    weightCalc;
CalibrationManager  calManager(stability, weightCalc);
EspNowManager       espNow;

// ---- Timing ----
unsigned long lastPrintMs = 0;
unsigned long lastSendMs  = 0;

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);

    Serial.println();
    Serial.println("=== Scale Node — Phase 3: ESP-NOW Transmitter ===");

    scale.begin();
    calManager.begin();
    espNow.begin();

    Serial.println("Scale Node initialized.");
}

void loop()
{
    // ---- Give ESP-NOW a chance to handle pairing and other async events ----
    espNow.loop();

    // ---- Read ADC ----
    if (scale.update()) {
        int32_t raw = scale.getRawReading();

        stability.addSample(raw);
        calManager.update(raw);

        unsigned long now = millis();

        // ---- Periodic serial debug output ----
        if (now - lastPrintMs >= READ_INTERVAL_MS) {
            lastPrintMs = now;

            float weight = weightCalc.computeWeight(raw);

            Serial.print("Raw: ");
            Serial.print(raw);
            Serial.print("\tZero: ");
            Serial.print(weightCalc.getZeroOffset());
            Serial.print("\tFactor: ");
            Serial.print(weightCalc.getCalibrationFactor(), 2);
            Serial.print("\tWeight: ");
            Serial.print(weight, 3);
            Serial.print("\tStable: ");
            Serial.print(stability.isStable() ? "YES" : "NO");
            Serial.print("\tPaired: ");
            Serial.println(espNow.isPaired() ? "YES" : "NO");
        }

        // ---- Periodic ESP-NOW transmission ----
        if (now - lastSendMs >= ESPNOW_SEND_INTERVAL_MS) {
            lastSendMs = now;

            float weight = weightCalc.computeWeight(raw);
            espNow.sendWeight(weight);
        }
    }
}
