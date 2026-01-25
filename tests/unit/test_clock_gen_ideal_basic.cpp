/**
 * @file test_clock_gen_ideal_basic.cpp
 * @brief Unit test for ClockGenerationTdf module - Ideal Clock Basic
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationBasicTest, IdealClockBasic) {
    ClockParams params;
    params.type = ClockType::IDEAL;
    params.frequency = 10e9;  // 10 GHz
    
    ClockGenTestbench* tb = new ClockGenTestbench("tb", params, 500);
    
    // Run for 50 clock periods (500 samples at 100 samples/period)
    sc_core::sc_start(50.0 / params.frequency, sc_core::SC_SEC);
    
    // Verify samples were collected
    EXPECT_GT(tb->get_phase_samples().size(), 0u);
    
    // Verify phase range is within [0, 2*pi)
    EXPECT_GE(tb->get_min_phase(), 0.0);
    EXPECT_LT(tb->get_max_phase(), 2.0 * M_PI + 0.01);  // Small tolerance
    
    sc_core::sc_stop();
}
