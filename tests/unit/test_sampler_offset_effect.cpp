/**
 * @file test_sampler_offset_effect.cpp
 * @brief Unit test for RxSamplerTdf module - Offset Effect
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerNoiseTest, OffsetEffect) {
    // Configure parameters with offset enabled
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = true;
    params.offset_value = 0.15;  // 150mV offset
    params.noise_enable = false;
    
    // Create testbench with zero input
    double input_amp = 0.0;  // 0mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Zero input with positive offset should result in 1.0 output
    double output = tb->get_output();
    EXPECT_NEAR(output, 1.0, 0.1) << "Zero input with positive offset should result in 1.0 output";
    
    sc_core::sc_stop();
}
