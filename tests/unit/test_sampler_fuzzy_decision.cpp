/**
 * @file test_sampler_fuzzy_decision.cpp
 * @brief Unit test for RxSamplerTdf module - Fuzzy Decision
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerHysteresisTest, FuzzyDecision) {
    // Configure parameters for fuzzy decision
    RxSamplerParams params;
    params.threshold = 0.0;
    params.resolution = 0.1;  // 100mV resolution region
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Create testbench with input in fuzzy region
    double input_amp = 0.05;  // 50mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Output should be either 0.0 or 1.0 (random in fuzzy region)
    double output = tb->get_output();
    EXPECT_TRUE(output == 0.0 || output == 1.0) << "Output should be digital (0.0 or 1.0)";
    
    sc_core::sc_stop();
}
