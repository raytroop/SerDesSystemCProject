/**
 * @file test_sampler_hysteresis_behavior.cpp
 * @brief Unit test for RxSamplerTdf module - Hysteresis Behavior
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerHysteresisTest, HysteresisBehavior) {
    // Configure parameters with hysteresis
    // Note: Current validation requires hysteresis < resolution, which means
    // hysteresis region is always inside fuzzy region. This test verifies
    // the deterministic boundary behavior outside fuzzy region.
    RxSamplerParams params;
    params.threshold = 0.0;
    params.hysteresis = 0.02;
    params.resolution = 0.05;  // Fuzzy region: |v| < 0.05
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Create testbench with input at boundary of deterministic region
    // Input 0.02 is outside fuzzy region (|0.02| < 0.05 is false? No, 0.02 < 0.05)
    // Actually 0.02 < 0.05, so it's in fuzzy region - use larger input
    // Use 0.06: outside fuzzy region (0.06 > 0.05), above hysteresis/2 (0.01)
    // Expected: deterministic true (v_diff > threshold + hysteresis/2)
    double input_amp = 0.06;  // 60mV: outside fuzzy, deterministic high
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Input above threshold + hysteresis/2 should result in deterministic 1.0
    double output1 = tb->get_output();
    EXPECT_NEAR(output1, 1.0, 0.1) << "Input outside fuzzy region, above threshold should be 1.0";
    
    sc_core::sc_stop();
}
