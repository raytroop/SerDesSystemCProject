/**
 * @file test_cdr_validation_kp.cpp
 * @brief Unit test for RxCdrTdf module - Parameter Validation Negative Kp
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrValidationTest, ParameterValidationNegativeKp) {
    CdrParams params;
    params.pi.kp = -0.01;  // Invalid: negative Kp
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    std::vector<double> pattern = {1.0, -1.0};
    
    EXPECT_THROW({
        CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);
        delete tb;
    }, std::invalid_argument) << "Should throw for negative Kp";
}
