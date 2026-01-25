/**
 * @file test_clock_gen_initial_phase.cpp
 * @brief Unit test for ClockGenerationTdf module - Initial Phase Zero
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationBasicTest, InitialPhaseZero) {
    ClockParams params;
    params.type = ClockType::IDEAL;
    params.frequency = 10e9;
    
    ClockGenTestbench* tb = new ClockGenTestbench("tb", params, 100);
    
    // Run for just a short time
    sc_core::sc_start(1e-12, sc_core::SC_SEC);
    
    // First phase sample should be zero (or very close to it)
    if (!tb->get_phase_samples().empty()) {
        EXPECT_NEAR(tb->get_phase_samples()[0], 0.0, 1e-10) 
            << "Initial phase should be zero";
    }
    
    sc_core::sc_stop();
}
