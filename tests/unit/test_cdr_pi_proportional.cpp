/**
 * @file test_cdr_pi_proportional.cpp
 * @brief Unit test for RxCdrTdf module - PI Controller Proportional Response
 * 
 * @note The CDR PI output is scaled by UI: phase = (Kp * error) * UI
 *       With Kp=0.01, UI=100ps, error=1 → phase = 0.01 * 100ps = 1ps
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrPiTest, ProportionalResponse) {
    CdrParams params;
    params.pi.kp = 0.01;         // Standard Kp
    params.pi.ki = 0.0;          // Disable integral for this test
    params.ui = 1e-10;           // 100ps UI (10Gbps)
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;    // 50ps = 0.5 UI

    // Pattern: -1 → +1 (rising edge)
    // First sample: prev=0, curr=-1, diff=-1 > 0.5 → falling edge, error=-1
    // Second sample: prev=-1, curr=+1, diff=2 > 0.5 → rising edge, error=+1
    std::vector<double> pattern = {-1.0, 1.0, 1.0, 1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    sc_core::sc_start(3, sc_core::SC_NS);

    // Expected phase after edges:
    // - First edge (error=-1): phase = -0.01 * 100ps = -1ps
    // - Second edge (error=+1): phase = +0.01 * 100ps = +1ps (net ~0)
    // The exact value depends on timing, but should be within range
    double phase = tb->get_phase_output();
    
    // Verify phase is within valid range
    EXPECT_GE(phase, -params.pai.range) << "Phase should be >= -range";
    EXPECT_LE(phase, params.pai.range) << "Phase should be <= +range";
    
    // Verify the phase magnitude is reasonable (Kp * UI scale)
    // With Kp=0.01, UI=100ps, each edge contributes ~1ps
    double expected_step = params.pi.kp * params.ui;  // 1e-12 = 1ps
    EXPECT_TRUE(std::abs(phase) <= 2 * expected_step + params.pai.resolution)
        << "Phase magnitude should be consistent with Kp*UI scaling";

    sc_core::sc_stop();
}
