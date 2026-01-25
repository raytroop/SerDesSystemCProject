/**
 * @file test_clock_gen_freq_40ghz.cpp
 * @brief Unit test for ClockGenerationTdf module - 40GHz Frequency Configuration
 */

#include "clock_generation_test_common.h"

using namespace serdes;
using namespace serdes::test;

// Set higher time resolution for high frequency tests
// Must be called before any SystemC object is created
static bool time_resolution_set = []() {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);
    return true;
}();

TEST(ClockGenerationFrequencyTest, Frequency40GHz) {
    (void)time_resolution_set;  // Ensure static initializer runs
    
    double freq = 40e9;  // 40 GHz
    
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
