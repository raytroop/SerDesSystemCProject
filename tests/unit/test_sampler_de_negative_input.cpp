/**
 * @file test_sampler_de_negative_input.cpp
 * @brief Unit test for RxSamplerTdf module - Negative Input DE Output
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerDeTest, NegativeInputDEOutput) {
    // Configure parameters
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Test with negative input
    double input_amp = -0.2;  // -200mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: DE output should be false for negative input
    bool de_output = tb->get_output_de();
    
    EXPECT_FALSE(de_output) << "DE output should be false for negative input";
    
    sc_core::sc_stop();
}
