/**
 * @file test_cdr_edge_rising.cpp
 * @brief Unit test for RxCdrTdf module - Rising Edge Detection
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrEdgeTest, RisingEdgeDetection) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    // Rising edge: -1.0 -> 1.0
    std::vector<double> pattern = {-1.0, 1.0, 1.0, 1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    sc_core::sc_start(5, sc_core::SC_NS);

    // After rising edge, phase error should be +1 (positive adjustment)
    double phase = tb->get_phase_output();
    EXPECT_GE(phase, 0.0) << "Rising edge should produce positive phase adjustment";

    sc_core::sc_stop();
}
