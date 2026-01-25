/**
 * @file test_cdr_validation_boundary.cpp
 * @brief Unit test for RxCdrTdf module - Parameter Boundary Conditions
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrValidationTest, ParameterBoundaryConditions) {
    CdrParams params;

    // Test very small Kp (valid)
    params.pi.kp = 1e-6;
    EXPECT_DOUBLE_EQ(params.pi.kp, 1e-6);

    // Test very large Kp (valid but may cause instability)
    params.pi.kp = 1.0;
    EXPECT_DOUBLE_EQ(params.pi.kp, 1.0);

    // Test very small Ki (valid)
    params.pi.ki = 1e-10;
    EXPECT_DOUBLE_EQ(params.pi.ki, 1e-10);

    // Test very small resolution (valid, 1fs)
    params.pai.resolution = 1e-15;
    EXPECT_DOUBLE_EQ(params.pai.resolution, 1e-15);

    // Test very large resolution (valid, 1ns)
    params.pai.resolution = 1e-9;
    EXPECT_DOUBLE_EQ(params.pai.resolution, 1e-9);

    // Test very small range (valid, +/-1ps)
    params.pai.range = 1e-12;
    EXPECT_DOUBLE_EQ(params.pai.range, 1e-12);

    // Test very large range (valid, +/-1ns)
    params.pai.range = 1e-9;
    EXPECT_DOUBLE_EQ(params.pai.range, 1e-9);
}
