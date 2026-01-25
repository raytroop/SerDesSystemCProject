/**
 * @file test_cdr_edge_falling.cpp
 * @brief Unit test for RxCdrTdf module - Falling Edge Detection
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrEdgeTest, FallingEdgeDetection) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    // Falling edge: 1.0 -> -1.0
    std::vector<double> pattern = {1.0, -1.0, -1.0, -1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    sc_core::sc_start(5, sc_core::SC_NS);

    // After falling edge, phase error should be -1 (negative adjustment)
    double phase = tb->get_phase_output();
    EXPECT_LE(phase, 0.0) << "Falling edge should produce negative phase adjustment";

    sc_core::sc_stop();
}
