/**
 * @file test_adaption_threshold_basic.cpp
 * @brief Unit test for AdaptionDe - Threshold Basic Function
 */

#include "adaption_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(AdaptionThresholdTest, ThresholdBasicFunction) {
    AdaptionParams params;
    params.agc.enabled = false;
    params.dfe.enabled = false;
    params.threshold.enabled = true;
    params.threshold.initial = 0.0;
    params.threshold.drift_threshold = 0.05;
    params.cdr_pi.enabled = false;
    
    AdaptionBasicTestbench* tb = new AdaptionBasicTestbench("tb", params);
    
    // Run simulation
    sc_core::sc_start(1, sc_core::SC_US);
    
    // Verify threshold is within drift limit
    double threshold = tb->get_threshold();
    double drift = std::abs(threshold - params.threshold.initial);
    EXPECT_LE(drift, params.threshold.drift_threshold) 
        << "Threshold drift should be within limit";
    
    // Clean up
    delete tb;
}
