/**
 * @file test_clock_gen_timestep.cpp
 * @brief Unit test for ClockGenerationTdf module - Time Step Adaptation
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationFrequencyTest, TimeStepAdaptation) {
    std::vector<double> test_frequencies = {10e9, 40e9, 80e9};
    std::vector<double> expected_timesteps = {1e-12, 0.25e-12, 0.125e-12};
    
    for (size_t i = 0; i < test_frequencies.size(); ++i) {
        ClockParams params;
        params.type = ClockType::IDEAL;
        params.frequency = test_frequencies[i];
        
        ClockGenerationTdf* clk_gen = new ClockGenerationTdf("clk_gen", params);
        
        // Verify expected timestep calculation
        double expected_ts = 1.0 / (test_frequencies[i] * 100.0);
        EXPECT_NEAR(clk_gen->get_expected_timestep(), expected_timesteps[i], 1e-15)
            << "Time step should adapt to frequency " << test_frequencies[i];
        
        delete clk_gen;
    }
}
