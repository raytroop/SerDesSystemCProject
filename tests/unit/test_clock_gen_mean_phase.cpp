/**
 * @file test_clock_gen_mean_phase.cpp
 * @brief Unit test for ClockGenerationTdf module - Mean Phase Distribution
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationCycleTest, MeanPhaseDistribution) {
    ClockParams params;
    params.type = ClockType::IDEAL;
    params.frequency = 10e9;
    
    // Run for many cycles to get good distribution
    int num_cycles = 100;
    
    ClockGenTestbench* tb = new ClockGenTestbench("tb", params, num_cycles * 100 + 50);
    
    sc_core::sc_start(static_cast<double>(num_cycles) / params.frequency, sc_core::SC_SEC);
    
    // Mean phase should be approximately pi (uniform distribution over [0, 2*pi))
    double mean_phase = tb->get_mean_phase();
    EXPECT_NEAR(mean_phase, M_PI, 0.1) 
        << "Mean phase should be approximately pi for uniform distribution";
    
    sc_core::sc_stop();
}
