/**
 * @file test_cdr_validation_range.cpp
 * @brief Unit test for RxCdrTdf module - Parameter Validation Invalid Range
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrValidationTest, ParameterValidationInvalidRange) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 0.0;  // Invalid: zero range

    std::vector<double> pattern = {1.0, -1.0};
    
    EXPECT_THROW({
        CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);
        delete tb;
    }, std::invalid_argument) << "Should throw for zero range";
}
