/**
 * @file test_cdr_edge_none.cpp
 * @brief Unit test for RxCdrTdf module - No Edge Detection
 * 
 * @note CDR initializes with m_prev_bit=0. To avoid initial edge detection,
 *       the pattern should start from 0 (or a value that doesn't cause
 *       transition > threshold from the initial state).
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrEdgeTest, NoEdgeDetection) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    // Pattern starts at 0.0 to match initial m_prev_bit=0
    // Then stays constant - no edges should be detected
    // All transitions are 0.0 which is below threshold (0.5)
    std::vector<double> pattern = {0.0, 0.0, 0.0, 0.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    sc_core::sc_start(10, sc_core::SC_NS);

    // With no edges (constant 0.0 from initial state), phase should remain at zero
    double phase = tb->get_phase_output();
    EXPECT_NEAR(phase, 0.0, params.pai.resolution * 2) 
        << "No edge should produce zero phase change";

    sc_core::sc_stop();
}
