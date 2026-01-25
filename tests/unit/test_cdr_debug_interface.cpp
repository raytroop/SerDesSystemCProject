/**
 * @file test_cdr_debug_interface.cpp
 * @brief Unit test for RxCdrTdf module - Debug Interface
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrBasicTest, DebugInterface) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    std::vector<double> pattern = {-1.0, 1.0, 1.0, 1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    sc_core::sc_start(5, sc_core::SC_NS);

    // Test debug interface methods
    double integral = tb->get_integral_state();
    double phase_error = tb->get_phase_error();
    
    // Integral can be positive or negative depending on accumulated phase errors
    // Just verify it's a finite value (accessible and valid)
    EXPECT_TRUE(std::isfinite(integral)) << "Integral state should be finite and accessible";
    
    // Phase error should be one of {-1, 0, 1}
    EXPECT_TRUE(phase_error == -1.0 || phase_error == 0.0 || phase_error == 1.0)
        << "Phase error should be -1, 0, or 1";

    sc_core::sc_stop();
}
