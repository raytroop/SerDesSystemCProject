/**
 * @file test_cdr_edge_threshold_effect.cpp
 * @brief Unit test for RxCdrTdf module - Edge Threshold Effect
 * 
 * @note Each threshold configuration is tested in a separate TEST case to avoid
 *       SystemC E529 error (cannot create modules after simulation starts).
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

/**
 * @brief Test high threshold - small transitions should NOT trigger edge detection
 */
TEST(CdrEdgeThresholdTest, HighThreshold_NoEdgeDetection) {
    std::vector<double> pattern = {0.0, 0.4, 0.0, 0.4};  // Small transitions (amplitude 0.4)

    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pi.edge_threshold = 0.5;  // Higher than transition amplitude
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);
    sc_core::sc_start(10, sc_core::SC_NS);
    
    double phase = tb->get_phase_output();
    // No edges detected, phase should be near zero
    EXPECT_NEAR(phase, 0.0, params.pai.resolution * 2) 
        << "High threshold should suppress edge detection for small transitions";

    sc_core::sc_stop();
}
