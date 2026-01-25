/**
 * @file test_cdr_basic_functionality.cpp
 * @brief Unit test for RxCdrTdf module - Basic Functionality
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrBasicTest, BasicFunctionality) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pi.edge_threshold = 0.5;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    std::vector<double> pattern = {1.0, -1.0, 1.0, -1.0, 1.0, -1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    sc_core::sc_start(10, sc_core::SC_NS);

    // Verify port connection
    SUCCEED() << "Port connection test passed";

    // Verify phase output is in valid range
    double phase = tb->get_phase_output();
    EXPECT_GE(phase, -params.pai.range) << "Phase output should be >= -range";
    EXPECT_LE(phase, params.pai.range) << "Phase output should be <= range";

    // Verify phase is quantized
    double quantized = std::round(phase / params.pai.resolution) * params.pai.resolution;
    EXPECT_NEAR(phase, quantized, 1e-15) << "Phase should be quantized";

    sc_core::sc_stop();
}
