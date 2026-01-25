/**
 * @file test_cdr_pi_config_low_gain.cpp
 * @brief Unit test for RxCdrTdf module - Low Gain PI Configuration
 * 
 * @note Independent test file to avoid SystemC E529 error.
 *       Each PI configuration requires a separate executable due to
 *       SystemC's limitation that modules cannot be created after sc_start().
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

/**
 * @brief Test PI controller with low gain configuration
 * 
 * Low gain configuration (Kp=0.005, Ki=5e-5) provides slower but more
 * stable response with less overshoot.
 */
TEST(CdrPiConfigTest, LowGain) {
    std::vector<double> pattern = {1.0, -1.0, 1.0, -1.0};

    CdrParams params;
    params.pi.kp = 0.005;   // 0.5x standard Kp
    params.pi.ki = 5e-5;    // 0.5x standard Ki
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);
    sc_core::sc_start(10, sc_core::SC_NS);
    
    double phase = tb->get_phase_output();
    EXPECT_GE(phase, -params.pai.range) << "Phase should be within negative range limit";
    EXPECT_LE(phase, params.pai.range) << "Phase should be within positive range limit";

    sc_core::sc_stop();
}
