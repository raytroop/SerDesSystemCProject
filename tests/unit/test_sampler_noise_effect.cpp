/**
 * @file test_sampler_noise_effect.cpp
 * @brief Unit test for RxSamplerTdf module - Noise Effect
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerNoiseTest, NoiseEffect) {
    // Configure parameters with noise enabled
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = true;
    params.noise_sigma = 0.05;
    params.noise_seed = 12345;
    
    // Create testbench with input near threshold
    double input_amp = 0.05;  // 50mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Output should be either 0.0 or 1.0 due to noise
    double output = tb->get_output();
    EXPECT_TRUE(output == 0.0 || output == 1.0) << "Output should be digital (0.0 or 1.0)";
    
    sc_core::sc_stop();
}
