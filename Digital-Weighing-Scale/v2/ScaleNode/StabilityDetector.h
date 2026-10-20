#ifndef STABILITY_DETECTOR_H
#define STABILITY_DETECTOR_H

#include <stdint.h>
#include "Config.h"

/**
 * @brief Circular buffer-based Peak-to-Peak stability detection.
 * 
 * Keeps the last N raw ADC readings. If the difference between the
 * maximum and minimum readings in the buffer is less than a configured
 * threshold, the scale is considered stable.
 */
class StabilityDetector {
public:
    StabilityDetector();

    // Reset the buffer and stability state
    void reset();

    // Add a new raw ADC reading and update stability state
    void addSample(int32_t rawValue);

    // Returns true if the scale is currently stable
    bool isStable() const;

private:
    int32_t _buffer[STABILITY_WINDOW_SIZE];
    int _head;
    int _count;
    bool _isStable;
};

#endif // STABILITY_DETECTOR_H
