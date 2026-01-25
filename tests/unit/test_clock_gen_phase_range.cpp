/**
 * @file test_clock_gen_phase_range.cpp
 * @brief Unit test for ClockGenerationTdf module - Phase Range Verification
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

// Set higher time resolution (40GHz requires sub-picosecond timestep)
static bool time_resolution_set = []() {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);
    return true;
}();

TEST(ClockGenerationBasicTest, PhaseRangeVerification) {
    (void)time_resolution_set;
    
    ClockParams params;
    params.type = ClockType::IDEAL;
    params.frequency = 40e9;  // 40 GHz
    
    ClockGenTestbench* tb = new ClockGenTestbench("tb", params, 1000);
    
    // Run for 10 clock periods
    sc_core::sc_start(10.0 / params.frequency, sc_core::SC_SEC);
    
    // All phase values should be in [0, 2*pi) range
    for (double phase : tb->get_phase_samples()) {
        EXPECT_GE(phase, 0.0) << "Phase should be non-negative";
        EXPECT_LT(phase, 2.0 * M_PI + 1e-10) << "Phase should be less than 2*pi";
    }
    
    sc_core::sc_stop();
}
