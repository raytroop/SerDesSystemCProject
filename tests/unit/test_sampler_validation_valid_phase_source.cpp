/**
 * @file test_sampler_validation_valid_phase_source.cpp
 * @brief Unit test for RxSamplerTdf module - Valid Phase Source
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerValidationTest, ValidPhaseSource) {
    // Test valid phase sources
    std::vector<std::string> valid_sources = {"clock", "phase"};
    
    for (const std::string& source : valid_sources) {
        RxSamplerParams params;
        params.phase_source = source;
        params.resolution = 0.1;  // Ensure resolution > hysteresis(0.02)
        
        // Test: Construction should succeed
        EXPECT_NO_THROW(
            RxSamplerTdf* sampler = new RxSamplerTdf("sampler", params);
            delete sampler;
        ) << "Valid phase source '" << source << "' should not throw exception";
    }
}
