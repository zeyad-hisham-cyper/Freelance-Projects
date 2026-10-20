#include <Arduino.h>
#include <unity.h>
#include "postureLogic.h"

// Mock EEPROM behavior or ensure we reset before test
PostureLogic logic;

void setUp(void) {
    // Reset internal state if possible or rely on valid logic
    // We can't easily reset EEPROM here without mocking, 
    // but we can test logic given a state.
}

void tearDown(void) {
    // clean up
}

void test_calibration_flow() {
    unsigned long now = 1000;
    logic.startCalibration(now);
    
    // Check initial state
    TEST_ASSERT_FALSE(logic.isCalibrationDone(now));
    
    // Feed some data
    PostureAngles a1 = {10.0f, 5.0f};
    logic.updateCalibration(a1, now);
    
    // Advance time but not enough
    now += 60000; // 1 min
    TEST_ASSERT_FALSE(logic.isCalibrationDone(now));
    
    // Advance to finish (2 min total)
    now += 60001;
    TEST_ASSERT_TRUE(logic.isCalibrationDone(now));
    
    logic.finishCalibration();
    TEST_ASSERT_TRUE(logic.hasBaseline());
    
    // Verify baseline (should be avg of inputs, here just one)
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.0f, logic.getBackThreshold() + THRESHOLD_STEP_DEG*0); 
    // Note: getThreshold returns baseline - step*level. Level is 0.
    // So getThreshold == baseline.
}

void test_bad_posture_detection() {
    // Assume we have baseline from prev test
    // Baseline Back = 10.0, Neck = 5.0
    // Thresh (lvl 0) -> Back=10, Neck=5? 
    // Wait, getBackThreshold returns baseline - step*level (0).
    // so threshold is 10.
    
    // Good posture (below threshold)
    PostureAngles good = {5.0f, 2.0f};
    TEST_ASSERT_FALSE(logic.isPostureBad(good));
    
    // Bad posture (above threshold)
    PostureAngles bad = {15.0f, 2.0f}; // 15 > 10
    TEST_ASSERT_TRUE(logic.isPostureBad(bad));
}

void setup() {
    delay(2000);
    UnityBegin("PostureLogic Tests");
    
    RUN_TEST(test_calibration_flow);
    RUN_TEST(test_bad_posture_detection);
    
    UnityEnd();
}

void loop() {}
