/**
 * @file test_sampler_validation_params.cpp
 * @brief Unit test for RxSamplerTdf module - Parameter Validation
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerValidationTest, ParameterValidation) {
    // Configure invalid parameters (hysteresis >= resolution)
    RxSamplerParams params;
    params.hysteresis = 0.1;
    params.resolution = 0.05;  // Invalid: hysteresis > resolution
    
    // Test: Construction should throw exception
    EXPECT_THROW(
        RxSamplerTdf* sampler = new RxSamplerTdf("sampler", params),
        std::invalid_argument
    ) << "Invalid parameters should throw exception";
}
