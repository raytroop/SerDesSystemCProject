/**
 * @file test_cdr_pai_quantization.cpp
 * @brief Unit test for RxCdrTdf module - Phase Quantization
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrPaiTest, PhaseQuantization) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;  // 1ps
    params.pai.range = 5e-11;

    std::vector<double> pattern = {1.0, -1.0, 1.0, -1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    sc_core::sc_start(10, sc_core::SC_NS);

    double phase = tb->get_phase_output();
    double quantized = std::round(phase / params.pai.resolution) * params.pai.resolution;
    EXPECT_NEAR(phase, quantized, 1e-15) << "Phase should be quantized to resolution";

    sc_core::sc_stop();
}
