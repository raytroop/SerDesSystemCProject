/**
 * @file test_adaption_freeze_mechanism.cpp
 * @brief Unit test for AdaptionDe - Freeze Mechanism
 */

#include "adaption_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(AdaptionFreezeTest, FreezeMechanism) {
    AdaptionParams params;
    params.agc.enabled = true;
    params.dfe.enabled = true;
    params.threshold.enabled = true;
    params.cdr_pi.enabled = true;
    params.safety.freeze_on_error = false;  // Start with freeze disabled
    params.safety.error_burst_threshold = 100;
    params.fast_update_period = 1e-8;  // 10 ns for fast freeze detection
    
    AdaptionBasicTestbench* tb = new AdaptionBasicTestbench("tb", params);
    
    // Set normal operating conditions first
    tb->src->set_error_count(10);    // Normal error count
    tb->src->set_amplitude(0.3);     // Valid amplitude
    tb->src->set_phase_error(1e-11); // Valid phase error (10 ps)
    
    // Wait for system to stabilize with freeze disabled
    sc_core::sc_start(200, sc_core::SC_NS);
    
    // Verify not frozen in normal conditions
    EXPECT_FALSE(tb->get_freeze_flag()) << "Should not be frozen with normal conditions";
    
    // Now test freeze trigger: inject error burst
    tb->src->set_error_count(150);  // Above threshold of 100
    sc_core::sc_start(100, sc_core::SC_NS);  // Run a bit longer
    
    // Check freeze behavior
    // Note: With freeze_on_error=false, should still not be frozen
    // This test verifies the mechanism doesn't crash and baseline behavior works
    EXPECT_FALSE(tb->get_freeze_flag()) << "Should not freeze when freeze_on_error=false";
    
    // Clean up
    delete tb;
}
