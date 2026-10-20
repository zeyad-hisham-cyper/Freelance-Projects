#pragma once

#include <Arduino.h>
#include <MD_MAX72xx.h>

#define HARDWARE_TYPE  MD_MAX72XX::FC16_HW
#define MAX_DEVICES    1

class Display {
public:
    Display();
    void begin();
    void showWeight(float weight);
    void clear();

private:
    MD_MAX72XX mx;
};
