/**
 * @file test_clock_gen_pll_validation.cpp
 * @brief Unit test for ClockGenerationTdf module - PLL Parameter Validation
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationValidationTest, PllParameterValidation) {
    ClockParams params;
    params.type = ClockType::PLL;
    params.frequency = 40e9;
    
    // Test invalid charge pump current
    params.pll.cp_current = -1e-5;
    EXPECT_THROW(
        ClockGenerationTdf* clk = new ClockGenerationTdf("clk", params),
        std::invalid_argument
    ) << "Negative CP current should throw exception";
    
    // Reset and test invalid loop filter resistance
    params.pll.cp_current = 5e-5;
    params.pll.lf_R = 0.0;
    EXPECT_THROW(
        ClockGenerationTdf* clk = new ClockGenerationTdf("clk", params),
        std::invalid_argument
    ) << "Zero LF resistance should throw exception";
    
    // Reset and test invalid divider
    params.pll.lf_R = 10000;
    params.pll.divider = -1;
    EXPECT_THROW(
        ClockGenerationTdf* clk = new ClockGenerationTdf("clk", params),
        std::invalid_argument
    ) << "Negative divider should throw exception";
}
