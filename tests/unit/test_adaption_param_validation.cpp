/**
 * @file test_adaption_param_validation.cpp
 * @brief Unit test for AdaptionDe - Parameter Validation
 */

#include "adaption_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(AdaptionBasicTest, ParameterValidation) {
    AdaptionParams params;
    
    // Test default values
    EXPECT_DOUBLE_EQ(params.Fs, 80e9);
    EXPECT_DOUBLE_EQ(params.UI, 2.5e-11);
    EXPECT_EQ(params.update_mode, "multi-rate");
    
    // Test AGC defaults
    EXPECT_TRUE(params.agc.enabled);
    EXPECT_DOUBLE_EQ(params.agc.target_amplitude, 0.4);
    EXPECT_GT(params.agc.kp, 0.0);
    EXPECT_GT(params.agc.ki, 0.0);
    EXPECT_LT(params.agc.gain_min, params.agc.gain_max);
    
    // Test DFE defaults
    EXPECT_TRUE(params.dfe.enabled);
    EXPECT_GT(params.dfe.num_taps, 0);
    EXPECT_LE(params.dfe.num_taps, 8);
    EXPECT_GT(params.dfe.mu, 0.0);
    
    // Test CDR PI defaults
    EXPECT_TRUE(params.cdr_pi.enabled);
    EXPECT_GT(params.cdr_pi.kp, 0.0);
    EXPECT_GT(params.cdr_pi.ki, 0.0);
    EXPECT_GT(params.cdr_pi.phase_range, 0.0);
    
    // Test safety defaults
    EXPECT_TRUE(params.safety.freeze_on_error);
    EXPECT_GT(params.safety.error_burst_threshold, 0);
}
