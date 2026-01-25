/**
 * @file test_cdr_pi_integral.cpp
 * @brief Unit test for RxCdrTdf module - PI Controller Integral Accumulation
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrPiTest, IntegralAccumulation) {
    CdrParams params;
    params.pi.kp = 0.0;  // Disable proportional
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    // Multiple rising edges to accumulate integral
    std::vector<double> pattern = {-1.0, 1.0, -1.0, 1.0, -1.0, 1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    sc_core::sc_start(10, sc_core::SC_NS);

    // Integral should accumulate over multiple edges
    double integral = tb->get_integral_state();
    // Multiple rising edges should accumulate positive integral
    EXPECT_GT(integral, 0.0) << "Integral should accumulate with rising edges";

    sc_core::sc_stop();
}
