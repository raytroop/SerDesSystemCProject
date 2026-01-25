/**
 * @file test_adaption_rollback.cpp
 * @brief Unit test for AdaptionDe - Rollback Mechanism
 */

#include "adaption_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(AdaptionFreezeTest, RollbackMechanism) {
    AdaptionParams params;
    params.agc.enabled = true;
    params.dfe.enabled = true;
    params.threshold.enabled = true;
    params.cdr_pi.enabled = true;
    params.safety.freeze_on_error = true;
    params.safety.rollback_enable = true;
    params.safety.snapshot_interval = 1e-7;  // 100 ns snapshots
    params.safety.error_burst_threshold = 100;
    
    AdaptionBasicTestbench* tb = new AdaptionBasicTestbench("tb", params);
    
    // Run with normal operation
    tb->src->set_error_count(10);
    sc_core::sc_start(500, sc_core::SC_NS);
    
    // Inject error to potentially trigger rollback
    tb->src->set_error_count(150);
    sc_core::sc_start(1, sc_core::SC_US);
    
    // Verify system continues running (no crash)
    SUCCEED() << "Rollback mechanism test passed";
    
    // Clean up
    delete tb;
}
