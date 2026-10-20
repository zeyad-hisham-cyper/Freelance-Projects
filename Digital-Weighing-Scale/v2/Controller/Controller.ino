/*
 * Controller.ino
 * ------------------------------------------------------------------------
 * Phase 3 — ESP-NOW Receiver + Display
 *
 * Architecture:
 *   DataTypes.h      -> Shared packet struct (ScaleData)
 *   Display          -> MAX7219 8-digit 7-segment display driver
 *   EspNowManager   -> Pairing handshake and weight packet reception
 *
 * Pairing:
 *   The Controller listens for a PAIRING_REQ broadcast from the Scale Node.
 *   On receipt it saves the Scale MAC to NVS and replies with PAIRING_ACK.
 *   Subsequent boots load the MAC directly from NVS, skipping pairing.
 *
 * Display format: DDD.DDDDD  (e.g. 005.12345  123.45678)
 * ------------------------------------------------------------------------
 */

#include "EspNowManager.h"
#include "Display.h"

EspNowManager espNow;
Display       display;

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("=== Controller Node — Phase 3: ESP-NOW Receiver ===");

    display.begin();
    display.showWeight(0.0f);   // Show 000.00000 on startup

    espNow.begin();

    Serial.println("Controller Node initialized. Waiting for Scale Node...");
}

void loop()
{
    // Give EspNowManager a chance to process pending pairing events
    espNow.loop();

    // Poll for new weight data from the Scale Node
    if (espNow.available()) {
        float weight = espNow.getWeight();
        display.showWeight(weight);

        Serial.print("[RX] Weight: ");
        Serial.println(weight, 5);
    }
}