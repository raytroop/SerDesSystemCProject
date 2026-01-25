/**
 * @file test_clock_gen_long_stability.cpp
 * @brief Unit test for ClockGenerationTdf module - Long Simulation Stability
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationCycleTest, LongSimulationStability) {
    ClockParams params;
    params.type = ClockType::IDEAL;
    params.frequency = 10e9;  // 10 GHz
    
    // Run for 1000 cycles to check numerical stability
    int num_cycles = 1000;
    int samples_per_cycle = 100;
    
    ClockGenTestbench* tb = new ClockGenTestbench("tb", params, num_cycles * samples_per_cycle + 100);
    
    sc_core::sc_start(static_cast<double>(num_cycles) / params.frequency, sc_core::SC_SEC);
    
    // All phase values should still be in valid range after long simulation
    for (double phase : tb->get_phase_samples()) {
        EXPECT_GE(phase, 0.0) << "Phase should remain non-negative";
        EXPECT_LT(phase, 2.0 * M_PI + 1e-9) << "Phase should remain less than 2*pi";
    }
    
    // Phase increments should still be consistent
    std::vector<double> increments = tb->get_phase_increments();
    double expected_increment = 2.0 * M_PI / 100.0;
    
    // Check last 100 increments
    size_t start_idx = increments.size() > 100 ? increments.size() - 100 : 0;
    for (size_t i = start_idx; i < increments.size(); ++i) {
        EXPECT_NEAR(increments[i], expected_increment, 1e-9) 
            << "Phase increment should remain stable after long simulation";
    }
    
    sc_core::sc_stop();
}
