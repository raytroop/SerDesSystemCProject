/**
 * @file test_sampler_output_range_pos01.cpp
 * @brief Unit test for RxSamplerTdf module - Output Range with 0.1V input
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerValidationTest, OutputRangePos01) {
    // Configure parameters
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    double amp = 0.1;
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, amp);
    
    sc_core::sc_start(10, sc_core::SC_NS);
    
    double output = tb->get_output();
    
    // Test: Output should be either 0.0 or 1.0
    EXPECT_TRUE(output == 0.0 || output == 1.0) 
        << "Output should be digital (0.0 or 1.0) for input amplitude " << amp;
    
    sc_core::sc_stop();
}
