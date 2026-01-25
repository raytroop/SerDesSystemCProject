/**
 * @file test_cdr_pi_configurations.cpp
 * @brief Unit test for RxCdrTdf module - Different PI Configurations
 * 
 * @note Each configuration is tested in a separate TEST case to avoid
 *       SystemC E529 error (cannot create modules after simulation starts).
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

/**
 * @brief Test PI controller with standard gain configuration
 */
TEST(CdrPiConfigTest, StandardGain) {
    std::vector<double> pattern = {1.0, -1.0, 1.0, -1.0};

    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);
    sc_core::sc_start(10, sc_core::SC_NS);
    
    double phase = tb->get_phase_output();
    EXPECT_GE(phase, -params.pai.range) << "Phase should be within negative range limit";
    EXPECT_LE(phase, params.pai.range) << "Phase should be within positive range limit";

    sc_core::sc_stop();
}
