/**
 * @file test_adaption_threshold_drift.cpp
 * @brief Unit test for AdaptionDe - Threshold Drift Limiting
 */

#include "adaption_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(AdaptionThresholdTest, ThresholdDriftLimiting) {
    AdaptionParams params;
    params.agc.enabled = false;
    params.dfe.enabled = false;
    params.threshold.enabled = true;
    params.threshold.initial = 0.0;
    params.threshold.drift_threshold = 0.03;  // Tight limit
    params.threshold.adapt_step = 0.001;
    params.cdr_pi.enabled = false;
    
    AdaptionBasicTestbench* tb = new AdaptionBasicTestbench("tb", params);
    
    // Set high error count to trigger adaptation
    tb->src->set_error_count(50);
    
    // Run simulation
    sc_core::sc_start(5, sc_core::SC_US);
    
    // Verify threshold drift is limited
    double threshold = tb->get_threshold();
    double drift = std::abs(threshold - params.threshold.initial);
    EXPECT_LE(drift, params.threshold.drift_threshold);
    
    // Clean up
    delete tb;
}
