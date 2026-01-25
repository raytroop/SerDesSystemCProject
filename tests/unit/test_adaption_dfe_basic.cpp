/**
 * @file test_adaption_dfe_basic.cpp
 * @brief Unit test for AdaptionDe - DFE Basic Function
 */

#include "adaption_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(AdaptionDfeTest, DfeBasicFunction) {
    AdaptionParams params;
    params.agc.enabled = false;
    params.dfe.enabled = true;
    params.dfe.num_taps = 5;
    params.dfe.algorithm = "sign-lms";
    params.dfe.initial_taps = {-0.05, -0.02, 0.01, 0.005, 0.002};
    params.threshold.enabled = false;
    params.cdr_pi.enabled = false;
    
    AdaptionBasicTestbench* tb = new AdaptionBasicTestbench("tb", params);
    
    // Run simulation
    sc_core::sc_start(1, sc_core::SC_US);
    
    // Verify DFE taps are within range
    for (int i = 0; i < params.dfe.num_taps; ++i) {
        double tap = tb->get_dfe_tap(i);
        EXPECT_GE(tap, params.dfe.tap_min) << "Tap " << i << " should be >= tap_min";
        EXPECT_LE(tap, params.dfe.tap_max) << "Tap " << i << " should be <= tap_max";
    }
    
    // Clean up
    delete tb;
}
