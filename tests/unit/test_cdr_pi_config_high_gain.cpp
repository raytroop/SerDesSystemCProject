/**
 * @file test_cdr_pi_config_high_gain.cpp
 * @brief Unit test for RxCdrTdf module - High Gain PI Configuration
 * 
 * @note Independent test file to avoid SystemC E529 error.
 *       Each PI configuration requires a separate executable due to
 *       SystemC's limitation that modules cannot be created after sc_start().
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

/**
 * @brief Test PI controller with high gain configuration
 * 
 * High gain configuration (Kp=0.02, Ki=2e-4) provides faster response
 * but may have more overshoot.
 */
TEST(CdrPiConfigTest, HighGain) {
    std::vector<double> pattern = {1.0, -1.0, 1.0, -1.0};

    CdrParams params;
    params.pi.kp = 0.02;   // 2x standard Kp
    params.pi.ki = 2e-4;   // 2x standard Ki
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);
    sc_core::sc_start(10, sc_core::SC_NS);
    
    double phase = tb->get_phase_output();
    EXPECT_GE(phase, -params.pai.range) << "Phase should be within negative range limit";
    EXPECT_LE(phase, params.pai.range) << "Phase should be within positive range limit";

    sc_core::sc_stop();
}
