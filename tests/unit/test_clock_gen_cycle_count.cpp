/**
 * @file test_clock_gen_cycle_count.cpp
 * @brief Unit test for ClockGenerationTdf module - Cycle Count Verification
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationCycleTest, CycleCountVerification) {
    ClockParams params;
    params.type = ClockType::IDEAL;
    params.frequency = 10e9;  // 10 GHz
    
    int expected_cycles = 10;
    int samples_per_cycle = 100;
    int total_samples = expected_cycles * samples_per_cycle + 50;  // Extra samples
    
    ClockGenTestbench* tb = new ClockGenTestbench("tb", params, total_samples);
    
    // Run for expected number of cycles
    sc_core::sc_start(static_cast<double>(expected_cycles) / params.frequency, sc_core::SC_SEC);
    
    // Count phase wraps
    int actual_wraps = tb->count_phase_wraps();
    
    // Should have approximately expected_cycles wraps
    EXPECT_NEAR(actual_wraps, expected_cycles, 1) 
        << "Cycle count should match expected value";
    
    sc_core::sc_stop();
}
