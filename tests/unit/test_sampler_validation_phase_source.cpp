/**
 * @file test_sampler_validation_phase_source.cpp
 * @brief Unit test for RxSamplerTdf module - Phase Source Validation
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerValidationTest, PhaseSourceValidation) {
    // Configure invalid phase source
    RxSamplerParams params;
    params.phase_source = "invalid";
    
    // Test: Construction should throw exception
    EXPECT_THROW(
        RxSamplerTdf* sampler = new RxSamplerTdf("sampler", params),
        std::invalid_argument
    ) << "Invalid phase source should throw exception";
}
