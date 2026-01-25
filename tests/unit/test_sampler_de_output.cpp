/**
 * @file test_sampler_de_output.cpp
 * @brief Unit test for RxSamplerTdf module - DE Output Verification
 */

#include "sampler_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(SamplerDeTest, DEOutputVerification) {
    // Configure parameters
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Test with positive input
    double input_amp = 0.2;  // 200mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Both TDF and DE outputs should match
    double tdf_output = tb->get_output();
    bool de_output = tb->get_output_de();
    
    // Convert TDF output to bool for comparison
    bool tdf_output_bool = (tdf_output != 0.0);
    
    EXPECT_EQ(tdf_output_bool, de_output) 
        << "TDF and DE outputs should match: TDF=" << tdf_output_bool << ", DE=" << de_output;
    
    // Test: DE output should be boolean (true for positive input)
    EXPECT_TRUE(de_output) << "DE output should be true for positive input";
    
    sc_core::sc_stop();
}
