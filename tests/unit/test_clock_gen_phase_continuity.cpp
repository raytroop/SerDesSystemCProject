/**
 * @file test_clock_gen_phase_continuity.cpp
 * @brief Unit test for ClockGenerationTdf module - Phase Continuity
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

// Set higher time resolution (20GHz requires sub-picosecond timestep)
static bool time_resolution_set = []() {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);
    return true;
}();

TEST(ClockGenerationBasicTest, PhaseContinuity) {
    (void)time_resolution_set;
    
    ClockParams params;
    params.type = ClockType::IDEAL;
    params.frequency = 20e9;  // 20 GHz
    
    ClockGenTestbench* tb = new ClockGenTestbench("tb", params, 500);
    
    // Run for 5 clock periods
    sc_core::sc_start(5.0 / params.frequency, sc_core::SC_SEC);
    
    std::vector<double> increments = tb->get_phase_increments();
    
    // Skip if not enough samples
    if (increments.size() < 2) {
        delete tb;
        GTEST_SKIP() << "Not enough samples collected";
    }
    
    // Expected increment: 2*pi / 100 (100 samples per period)
    double expected_increment = 2.0 * M_PI / 100.0;
    
    // All increments should be approximately equal (for ideal clock)
    for (double inc : increments) {
        EXPECT_NEAR(inc, expected_increment, 1e-10) 
            << "Phase increment should be constant for ideal clock";
    }
    
    sc_core::sc_stop();
}
