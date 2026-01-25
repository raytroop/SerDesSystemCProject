/**
 * @file test_adaption_output_range.cpp
 * @brief Unit test for AdaptionDe - Output Range Validation
 */

#include "adaption_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(AdaptionBasicTest, OutputRangeValidation) {
    AdaptionParams params;
    
    AdaptionBasicTestbench* tb = new AdaptionBasicTestbench("tb", params);
    
    // Run simulation
    sc_core::sc_start(1, sc_core::SC_US);
    
    // Verify all outputs are in reasonable ranges
    double gain = tb->get_vga_gain();
    EXPECT_GE(gain, 0.0) << "VGA gain should be >= 0";
    EXPECT_LE(gain, 100.0) << "VGA gain should be <= 100";
    
    double threshold = tb->get_threshold();
    EXPECT_GE(threshold, -1.0) << "Threshold should be >= -1V";
    EXPECT_LE(threshold, 1.0) << "Threshold should be <= 1V";
    
    double phase_cmd = tb->get_phase_cmd();
    EXPECT_GE(phase_cmd, -1e-9) << "Phase cmd should be >= -1ns";
    EXPECT_LE(phase_cmd, 1e-9) << "Phase cmd should be <= 1ns";
    
    // Clean up
    delete tb;
}
