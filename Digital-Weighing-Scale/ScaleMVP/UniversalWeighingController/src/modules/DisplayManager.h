/*
 * DisplayManager.h
 * ------------------------------------------------------------------------
 * The unified display interface required by system.md:
 *
 *     DisplayManager.showWeight(...);
 *
 * The rest of the firmware never learns whether an OLED or a MAX7219
 * seven-segment module is fitted. Selecting a display in hardware.h swaps
 * the driver underneath; the calls above stay identical.
 *
 * This class is a dispatcher, not a renderer — panel-specific layout lives
 * in the driver, because what "two lines of text" means on a 6-digit LED
 * module is a question only that driver can answer.
 * ------------------------------------------------------------------------
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "../config/config.h"

#ifdef DISPLAY_OLED
    #include "../drivers/OledDisplay.h"
#endif

#ifdef DISPLAY_7SEG
    #include "../drivers/MAX7219.h"
#endif

class DisplayManager {
public:
    DisplayManager();

    // False means the panel did not respond — normally a wiring fault.
    bool begin();

    void showBoot();
    void showWeight(float value, const char *unitLabel, uint8_t decimals,
                    bool stable, bool tared);
    void showMessage(const char *title, const char *line1 = nullptr,
                     const char *line2 = nullptr);
    void showCalibrationStep(uint8_t step, const char *title,
                             const char *line1, const char *line2,
                             const char *value = nullptr);
    void showError(const char *title, const char *detail = nullptr);

    bool isReady() const { return _ready; }

    // Panel brightness, 0..15. Only the MAX7219 can act on it; the OLED
    // ignores it. Exposed here because it is a display property, not a
    // driver detail the application should have to know about.
    void setBrightness(uint8_t level);

    // Blocking lamp/segment test. A no-op on the OLED, which has no
    // segments to lose.
    void runSelfTest();

private:
#ifdef DISPLAY_OLED
    OledDisplay _driver;
#endif
#ifdef DISPLAY_7SEG
    MAX7219 _driver;
#endif
    bool _ready;
};

#endif // DISPLAY_MANAGER_H
