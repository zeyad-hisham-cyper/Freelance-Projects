#ifndef WEIGHT_CALCULATOR_H
#define WEIGHT_CALCULATOR_H

#include <stdint.h>

class WeightCalculator {
public:
    WeightCalculator();

    // Set the zero offset (raw ADC value at zero weight)
    void setZeroOffset(int32_t offset);
    int32_t getZeroOffset() const;

    // Set the calibration factor (raw ADC counts per kg)
    void setCalibrationFactor(float factor);
    float getCalibrationFactor() const;

    // Convert a raw ADC reading to physical weight
    float computeWeight(int32_t rawValue) const;

private:
    int32_t _zeroOffset;
    float _calibrationFactor;
};

#endif // WEIGHT_CALCULATOR_H
