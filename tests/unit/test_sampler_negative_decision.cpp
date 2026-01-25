/**
 * @file test_sampler_negative_decision.cpp
 * @brief Unit test for RxSamplerTdf module - Negative Input Decision
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerBasicTest, NegativeInputDecision) {
    // Configure parameters
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Create testbench with negative input (by using negative amplitude)
    double input_amp = -0.2;  // -200mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Negative input should result in 0.0 output
    double output = tb->get_output();
    EXPECT_NEAR(output, 0.0, 0.1) << "Negative input should result in 0.0 output";
    
    sc_core::sc_stop();
}
