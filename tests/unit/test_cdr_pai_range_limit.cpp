/**
 * @file test_cdr_pai_range_limit.cpp
 * @brief Unit test for RxCdrTdf module - Phase Range Limiting
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrPaiTest, PhaseRangeLimit) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    // Constant input (no edges) - phase should remain stable
    std::vector<double> pattern = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    sc_core::sc_start(10, sc_core::SC_NS);

    double phase = tb->get_phase_output();
    EXPECT_GE(phase, -params.pai.range) << "Phase should not exceed negative limit";
    EXPECT_LE(phase, params.pai.range) << "Phase should not exceed positive limit";

    sc_core::sc_stop();
}
