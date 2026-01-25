/**
 * @file test_clock_gen_extreme_freq.cpp
 * @brief Unit test for ClockGenerationTdf module - Extreme Frequency Validation
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationValidationTest, ExtremeFrequencyValidation) {
    ClockParams params;
    params.type = ClockType::IDEAL;
    
    // Test too high frequency (> 1 THz)
    params.frequency = 2e12;
    EXPECT_THROW(
        ClockGenerationTdf* clk = new ClockGenerationTdf("clk", params),
        std::invalid_argument
    ) << "Frequency above 1 THz should throw exception";
    
    // Test too low frequency (< 1 Hz)
    params.frequency = 0.5;
    EXPECT_THROW(
        ClockGenerationTdf* clk = new ClockGenerationTdf("clk", params),
        std::invalid_argument
    ) << "Frequency below 1 Hz should throw exception";
}
