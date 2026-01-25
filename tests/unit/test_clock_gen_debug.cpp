/**
 * @file test_clock_gen_debug.cpp
 * @brief Unit test for ClockGenerationTdf module - Debug Interface
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationTypeTest, DebugInterface) {
    ClockParams params;
    params.type = ClockType::IDEAL;
    params.frequency = 40e9;
    
    ClockGenerationTdf* clk_gen = new ClockGenerationTdf("clk_gen", params);
    
    // Verify debug interface returns correct values
    EXPECT_EQ(clk_gen->get_frequency(), 40e9) << "Frequency should match";
    EXPECT_EQ(clk_gen->get_type(), ClockType::IDEAL) << "Type should match";
    EXPECT_NEAR(clk_gen->get_expected_timestep(), 0.25e-12, 1e-15) << "Timestep should match";
    
    delete clk_gen;
}
