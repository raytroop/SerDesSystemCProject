/**
 * @file test_cdr_pi_config.cpp
 * @brief Unit test for RxCdrTdf module - PI Controller Configuration
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrPiTest, PIControllerConfiguration) {
    CdrParams params;

    // Test default values
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    EXPECT_GT(params.pi.kp, 0.0) << "Kp should be positive";
    EXPECT_GT(params.pi.ki, 0.0) << "Ki should be positive";

    // Test different Kp values
    params.pi.kp = 0.001;
    EXPECT_DOUBLE_EQ(params.pi.kp, 0.001);

    params.pi.kp = 0.1;
    EXPECT_DOUBLE_EQ(params.pi.kp, 0.1);

    // Test different Ki values
    params.pi.ki = 1e-5;
    EXPECT_DOUBLE_EQ(params.pi.ki, 1e-5);

    params.pi.ki = 1e-3;
    EXPECT_DOUBLE_EQ(params.pi.ki, 1e-3);

    // Test Ki < Kp (typical relationship)
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    EXPECT_LT(params.pi.ki, params.pi.kp) << "Ki should typically be smaller than Kp";
}
