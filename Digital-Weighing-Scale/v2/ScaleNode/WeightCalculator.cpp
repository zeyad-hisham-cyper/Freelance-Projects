#include "WeightCalculator.h"

WeightCalculator::WeightCalculator() : _zeroOffset(0), _calibrationFactor(1.0f) {
}

void WeightCalculator::setZeroOffset(int32_t offset) {
    _zeroOffset = offset;
}

int32_t WeightCalculator::getZeroOffset() const {
    return _zeroOffset;
}

void WeightCalculator::setCalibrationFactor(float factor) {
    // Avoid division by zero
    if (factor == 0.0f) factor = 1.0f;
    _calibrationFactor = factor;
}

float WeightCalculator::getCalibrationFactor() const {
    return _calibrationFactor;
}

float WeightCalculator::computeWeight(int32_t rawValue) const {
    return (float)(rawValue - _zeroOffset) / _calibrationFactor;
}
