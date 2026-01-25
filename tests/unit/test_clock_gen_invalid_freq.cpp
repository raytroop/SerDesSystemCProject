/**
 * @file test_clock_gen_invalid_freq.cpp
 * @brief Unit test for ClockGenerationTdf module - Invalid Frequency Validation
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationValidationTest, InvalidFrequencyValidation) {
    ClockParams params;
    params.type = ClockType::IDEAL;
    
    // Test zero frequency
    params.frequency = 0.0;
    EXPECT_THROW(
        ClockGenerationTdf* clk = new ClockGenerationTdf("clk", params),
        std::invalid_argument
    ) << "Zero frequency should throw exception";
    
    // Test negative frequency
    params.frequency = -10e9;
    EXPECT_THROW(
        ClockGenerationTdf* clk = new ClockGenerationTdf("clk", params),
        std::invalid_argument
    ) << "Negative frequency should throw exception";
}
