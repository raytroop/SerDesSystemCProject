/**
 * @file test_cdr_edge_threshold_low.cpp
 * @brief Unit test for RxCdrTdf module - Low Edge Threshold Configuration
 * 
 * @note Independent test file to avoid SystemC E529 error.
 *       Each threshold configuration requires a separate executable due to
 *       SystemC's limitation that modules cannot be created after sc_start().
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

/**
 * @brief Test low threshold - small transitions SHOULD trigger edge detection
 * 
 * With edge_threshold=0.3, transitions of amplitude 0.4 should be detected
 * as edges and cause phase adjustment.
 */
TEST(CdrEdgeThresholdTest, LowThreshold_EdgeDetection) {
    std::vector<double> pattern = {0.0, 0.4, 0.0, 0.4};  // Small transitions (amplitude 0.4)

    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pi.edge_threshold = 0.3;  // Lower than transition amplitude (0.4)
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);
    sc_core::sc_start(10, sc_core::SC_NS);
    
    double phase = tb->get_phase_output();
    // Edges detected, phase should be non-zero (within range limits)
    // Note: We check for non-zero OR at range limit (which also indicates edge detection)
    bool edge_detected = (phase != 0.0) || 
                         (std::abs(phase) >= params.pai.range - params.pai.resolution);
    EXPECT_TRUE(edge_detected) 
        << "Low threshold should detect edges for transitions > threshold";

    sc_core::sc_stop();
}
