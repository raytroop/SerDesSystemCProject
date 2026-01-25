/**
 * @file test_sampler_basic_decision.cpp
 * @brief Unit test for RxSamplerTdf module - Basic Decision
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerBasicTest, BasicDecision) {
    // Configure parameters
    RxSamplerParams params;
    params.resolution = 0.1;  // Larger resolution for clear decision
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Create testbench with positive input
    double input_amp = 0.2;  // 200mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Positive input should result in 1.0 output
    double output = tb->get_output();
    EXPECT_NEAR(output, 1.0, 0.1) << "Positive input should result in 1.0 output";
    
    sc_core::sc_stop();
}
