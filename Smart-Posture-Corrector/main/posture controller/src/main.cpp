/*
 * Smart Posture Corrector - Main Application
 * -----------------------------------------
 * Board: ESP32
 * 
 * Logic encapsulated in PostureApp
 */

#include <Arduino.h>
#include "PostureApp.h"

PostureApp app;

void setup() {
    Serial.begin(115200);
    // Give serial a moment
    delay(500);
    Serial.printf("Reset reason: %d\n", (int)esp_reset_reason());
    app.begin();
}

void loop() {
    app.update();
    // No delay needed here, handled inside app modules
}
