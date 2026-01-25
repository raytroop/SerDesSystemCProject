/**
 * @file test_clock_gen_freq_1ghz.cpp
 * @brief Unit test for ClockGenerationTdf module - 1GHz Frequency Configuration
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(ClockGenerationFrequencyTest, Frequency1GHz) {
    double freq = 1e9;  // 1 GHz
    
    ClockParams params;
    params.type = ClockType::IDEAL;
    params.frequency = freq;
    
    ClockGenTestbench* tb = new ClockGenTestbench("tb", params, 200);
    
    // Run for 2 clock periods
    sc_core::sc_start(2.0 / freq, sc_core::SC_SEC);
    
    // Verify phase wraps occurred (at least 1 for 2 periods)
    int wraps = tb->count_phase_wraps();
    EXPECT_GE(wraps, 1) << "Should have at least 1 phase wrap for frequency " << freq;
    
    sc_core::sc_stop();
}
