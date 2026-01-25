/**
 * @file test_clock_gen_type_ideal.cpp
 * @brief Unit test for ClockGenerationTdf module - IDEAL Clock Type
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationTypeTest, IdealClockOutput) {
    ClockParams params;
    params.type = ClockType::IDEAL;
    params.frequency = 10e9;
    
    ClockGenTestbench* tb = new ClockGenTestbench("tb", params, 200);
    
    // Run for 2 clock periods
    sc_core::sc_start(2.0 / params.frequency, sc_core::SC_SEC);
    
    // IDEAL mode should produce valid phase output
    EXPECT_GT(tb->get_phase_samples().size(), 0u) 
        << "IDEAL clock type should produce output";
    
    // Phase should be in valid range
    EXPECT_GE(tb->get_min_phase(), 0.0);
    EXPECT_LT(tb->get_max_phase(), 2.0 * M_PI + 0.01);
    
    sc_core::sc_stop();
}
