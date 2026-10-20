#include "sensors.h"
#include <Wire.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// MPU6050 registers we need
// MPU6050 registers
static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;

Sensors::Sensors()
    : _lastUpdateMs(0),
      _updateIntervalMs(10), // 100 Hz for smoother integration
      _backAngleDeg(0.0f),
      _neckAngleDeg(0.0f),
      _neckRelAngleDeg(0.0f),
      _firstRun(true)
{
}

void Sensors::begin() {
    Wire.begin(); // use default SDA/SCL for ESP32
    // Increase I2C clock for faster reads
    Wire.setClock(400000);

    initMPU(MPU_ADDR_BACK);
    initMPU(MPU_ADDR_NECK);
}

void Sensors::initMPU(uint8_t addr) {
    Wire.beginTransmission(addr);
    Wire.write(REG_PWR_MGMT_1);
    Wire.write(0x00); // wake up device
    Wire.endTransmission();
    delay(50);
}

void Sensors::update(unsigned long nowMs) {
    if (nowMs - _lastUpdateMs < _updateIntervalMs) return;
    
    // Calculate dt in seconds
    float dt = (nowMs - _lastUpdateMs) / 1000.0f;
    if (_firstRun) dt = 0.0f;
    _lastUpdateMs = nowMs;

    int16_t ax1, ay1, az1, gx1, gy1, gz1;
    int16_t ax2, ay2, az2, gx2, gy2, gz2;

    readSensors(MPU_ADDR_BACK, ax1, ay1, az1, gx1, gy1, gz1);
    readSensors(MPU_ADDR_NECK, ax2, ay2, az2, gx2, gy2, gz2);

    // 1. Calculate Accelerometer Angle (steady but noisy)
    float accelPitch1 = computePitchAccel(ax1, ay1, az1);
    float accelPitch2 = computePitchAccel(ax2, ay2, az2);

    // 2. Calculate Gyro Rate in deg/s
    // Default scale +/- 250deg/s => 131 LSB/(deg/s)
    float gyroRateY1 = gy1 / 131.0f; 
    float gyroRateY2 = gy2 / 131.0f;

    // 3. Complementary Filter
    if (_firstRun) {
        _backAngleDeg = accelPitch1;
        _neckAngleDeg = accelPitch2;
        _firstRun = false;
    } else {
        // angle = alpha * (angle + gyro * dt) + (1-alpha) * accel
        _backAngleDeg = ALPHA * (_backAngleDeg + gyroRateY1 * dt) + (1.0f - ALPHA) * accelPitch1;
        _neckAngleDeg = ALPHA * (_neckAngleDeg + gyroRateY2 * dt) + (1.0f - ALPHA) * accelPitch2;
    }

    _neckRelAngleDeg = _neckAngleDeg - _backAngleDeg;
}

PostureAngles Sensors::getAngles() const {
    PostureAngles a;
    a.backAngleDeg = _backAngleDeg;
    a.neckRelAngleDeg = _neckRelAngleDeg;
    return a;
}

void Sensors::readSensors(uint8_t addr, int16_t& ax, int16_t& ay, int16_t& az,
                          int16_t& gx, int16_t& gy, int16_t& gz) {
    Wire.beginTransmission(addr);
    Wire.write(REG_ACCEL_XOUT_H);
    Wire.endTransmission(false);

    // Read 14 bytes: AccelX,Y,Z (6), Temp(2), GyroX,Y,Z (6)
    Wire.requestFrom(addr, (uint8_t)14);
    
    // Timeout check
    unsigned long start = millis();
    while (Wire.available() < 14) { 
        if (millis() - start > 10) { // 10ms timeout for I2C
             // Timeout occurred, return zero or previous values to avoid hang
             // For now, let's just break and maybe we'll read garbage/zeros 
             // which is better than infinite loop
             return;
        }
    }

    ax = (Wire.read() << 8) | Wire.read();
    ay = (Wire.read() << 8) | Wire.read();
    az = (Wire.read() << 8) | Wire.read();
    
    // Ignore temp
    Wire.read(); Wire.read();

    gx = (Wire.read() << 8) | Wire.read();
    gy = (Wire.read() << 8) | Wire.read();
    gz = (Wire.read() << 8) | Wire.read();
}

// Tilt estimation from accelerometer
float Sensors::computePitchAccel(int16_t ax, int16_t ay, int16_t az) const {
    /* 
       Depending on mounting:
       Pitch is often rotation around Y.
       atan2(-x, sqrt(y*y + z*z))
    */
    // Convert to float (scale doesn't matter for atan2, just ratios)
    float fax = (float)ax;
    float fay = (float)ay;
    float faz = (float)az;

    float pitchRad = atan2f(-fax, sqrtf(fay * fay + faz * faz));
    return pitchRad * 180.0f / (float)M_PI;
}
