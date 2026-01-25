/**
 * @file test_adaption_dfe_lms.cpp
 * @brief Unit test for AdaptionDe - DFE Algorithm LMS
 */

#include "adaption_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(AdaptionDfeTest, DfeLms) {
    AdaptionParams params;
    params.agc.enabled = false;
    params.dfe.enabled = true;
    params.dfe.num_taps = 5;
    params.dfe.algorithm = "lms";
    params.dfe.mu = 1e-3;
    params.threshold.enabled = false;
    params.cdr_pi.enabled = false;
    
    AdaptionBasicTestbench* tb = new AdaptionBasicTestbench("tb", params);
    
    // Run simulation
    sc_core::sc_start(1, sc_core::SC_US);
    
    // Verify taps are within range
    for (int i = 0; i < params.dfe.num_taps; ++i) {
        double tap = tb->get_dfe_tap(i);
        EXPECT_GE(tap, params.dfe.tap_min);
        EXPECT_LE(tap, params.dfe.tap_max);
    }
    
    // Clean up
    delete tb;
}
