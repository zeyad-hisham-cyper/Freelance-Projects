#pragma once
#include <Arduino.h>

struct PostureAngles {
    float backAngleDeg;      
    float neckRelAngleDeg;   
};

class Sensors {
public:
    Sensors();

    void begin();
    void update(unsigned long nowMs);

    PostureAngles getAngles() const;

private:
    static constexpr uint8_t MPU_ADDR_BACK = 0x68;
    static constexpr uint8_t MPU_ADDR_NECK = 0x69;

    unsigned long _lastUpdateMs;
    unsigned long _updateIntervalMs;

    // Filtered angles
    float _backAngleDeg;
    float _neckAngleDeg; // Storing absolute angle for neck too
    float _neckRelAngleDeg;

    // Helper to read raw values
    void initMPU(uint8_t addr);
    void readSensors(uint8_t addr, int16_t& ax, int16_t& ay, int16_t& az, int16_t& gx, int16_t& gy, int16_t& gz);
    float computePitchAccel(int16_t ax, int16_t ay, int16_t az) const;

    // Internal state for complementary filter
    bool _firstRun;
    static constexpr float ALPHA = 0.96f; // Filter constant
};
