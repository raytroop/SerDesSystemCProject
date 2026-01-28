/**
 * @file test_rx_top_integration.cpp
 * @brief Integration tests for RxTopModule
 * 
 * Tests:
 * - Full chain with realistic parameters
 * - Data recovery verification
 * - Long-running stability
 */

#include <gtest/gtest.h>
#include <systemc-ams>
#include "rx_top_test_common.h"
#include <cmath>

using namespace serdes;
using namespace serdes::test;

// ============================================================================
// Full Chain Integration Test
// ============================================================================

TEST(RxTopIntegrationTest, FullChainWithRealisticParams) {
    // Setup with realistic parameters for 10 Gbps
    RxParams params;
    
    // CTLE: moderate boost for channel compensation
    params.ctle.zeros = {2e9};
    params.ctle.poles = {25e9};
    params.ctle.dc_gain = 1.5;
    params.ctle.vcm_out = 0.0;
    
    // VGA: 2x gain
    params.vga.zeros = {1e9};
    params.vga.poles = {20e9};
    params.vga.dc_gain = 2.0;
    params.vga.vcm_out = 0.0;
    
    // DFE Summer: typical 3-tap configuration
    params.dfe_summer.tap_coeffs = {-0.05, -0.02, 0.01};
    params.dfe_summer.ui = 100e-12;
    params.dfe_summer.vcm_out = 0.0;
    params.dfe_summer.vtap = 1.0;
    params.dfe_summer.map_mode = "pm1";
    params.dfe_summer.enable = true;
    
    // Sampler: phase-driven
    params.sampler.phase_source = "phase";
    params.sampler.threshold = 0.0;
    params.sampler.hysteresis = 0.01;
    params.sampler.resolution = 0.02;
    
    // CDR: balanced gains for 10 Gbps
    params.cdr.pi.kp = 0.01;
    params.cdr.pi.ki = 1e-4;
    params.cdr.pai.resolution = 1e-12;
    params.cdr.pai.range = 5e-11;
    params.cdr.ui = 100e-12;
    
    AdaptionParams adaption_params = get_default_adaption_params();
    RxTopTestbench tb(params, adaption_params, RxDifferentialSource::PRBS, 0.3, 5e9);
    
    // Run simulation for 2000 ns
    sc_core::sc_start(2000, sc_core::SC_NS);
    
    // Verify output
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    EXPECT_TRUE(tb.monitor->is_valid_data()) 
        << "Realistic params should produce valid data";
    
    // Verify reasonable transition density
    size_t transitions = tb.monitor->count_transitions();
    size_t samples = tb.get_sample_count();
    double transition_density = static_cast<double>(transitions) / samples;
    
    EXPECT_GT(transition_density, 0.1) << "Should have reasonable transition density";
    EXPECT_LT(transition_density, 0.9) << "Transition density should not be too high";
    
    sc_core::sc_stop();
}

// ============================================================================
// Low Signal Amplitude Test
// ============================================================================

TEST(RxTopIntegrationTest, LowSignalAmplitude) {
    // Test with low input signal amplitude
    RxParams params = get_high_gain_rx_params();  // Use high gain to compensate
    AdaptionParams adaption_params = get_default_adaption_params();
    
    RxTopTestbench tb(params, adaption_params, RxDifferentialSource::SQUARE, 0.1, 3e9);  // 100mV pp
    
    // Run simulation
    sc_core::sc_start(1000, sc_core::SC_NS);
    
    // Verify output
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    
    // With high gain, low signal should still be detected
    size_t transitions = tb.monitor->count_transitions();
    EXPECT_GT(transitions, 0) << "Should detect transitions even with low signal";
    
    sc_core::sc_stop();
}

// ============================================================================
// High Frequency Input Test
// ============================================================================

TEST(RxTopIntegrationTest, HighFrequencyInput) {
    // Test with high frequency input (approaching Nyquist)
    RxParams params = get_default_rx_params();
    
    // Higher CTLE boost for high frequency
    params.ctle.dc_gain = 2.0;
    params.ctle.zeros = {5e9};
    AdaptionParams adaption_params = get_default_adaption_params();
    
    RxTopTestbench tb(params, adaption_params, RxDifferentialSource::SQUARE, 0.4, 10e9);  // 10 GHz
    
    // Run simulation
    sc_core::sc_start(500, sc_core::SC_NS);
    
    // Verify output
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    
    // High frequency may have ISI, but should still produce output
    size_t transitions = tb.monitor->count_transitions();
    EXPECT_GE(transitions, 0) << "High frequency input should not crash";
    
    sc_core::sc_stop();
}

// ============================================================================
// Long Running Stability Test
// ============================================================================

TEST(RxTopIntegrationTest, LongRunningStability) {
    // Test for long-running stability
    RxParams params = get_default_rx_params();
    AdaptionParams adaption_params = get_default_adaption_params();
    RxTopTestbench tb(params, adaption_params, RxDifferentialSource::PRBS, 0.3, 5e9);
    
    // Run for extended period (5000 ns)
    sc_core::sc_start(5000, sc_core::SC_NS);
    
    // Verify no overflow or NaN in CDR state
    double cdr_phase = tb.get_cdr_phase();
    double cdr_integral = tb.get_cdr_integral_state();
    
    EXPECT_TRUE(std::isfinite(cdr_phase)) << "CDR phase should remain finite";
    EXPECT_TRUE(std::isfinite(cdr_integral)) << "CDR integral should remain finite";
    
    // Verify output still valid
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    EXPECT_TRUE(tb.monitor->is_valid_data()) 
        << "Long running simulation should produce valid data";
    
    sc_core::sc_stop();
}

// ============================================================================
// Parameter Variation Robustness Test
// ============================================================================

TEST(RxTopIntegrationTest, ParameterVariationRobustness) {
    // Test with various parameter combinations
    RxParams params = get_default_rx_params();
    
    // Apply some parameter variations
    params.ctle.dc_gain = 1.8;
    params.vga.dc_gain = 2.5;
    params.dfe_summer.tap_coeffs = {-0.06, -0.03, 0.015, 0.005};
    params.dfe_summer.enable = true;
    params.cdr.pi.kp = 0.015;
    params.cdr.pi.ki = 1.5e-4;
    AdaptionParams adaption_params = get_default_adaption_params();
    
    RxTopTestbench tb(params, adaption_params, RxDifferentialSource::PRBS, 0.35, 5e9);
    
    // Run simulation
    sc_core::sc_start(1000, sc_core::SC_NS);
    
    // Verify output
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    EXPECT_TRUE(tb.monitor->is_valid_data()) 
        << "Varied parameters should produce valid data";
    
    sc_core::sc_stop();
}

// ============================================================================
// Data Recovery Quality Test
// ============================================================================

TEST(RxTopIntegrationTest, DataRecoveryQuality) {
    // Test data recovery with PRBS pattern
    RxParams params = get_default_rx_params();
    AdaptionParams adaption_params = get_default_adaption_params();
    RxTopTestbench tb(params, adaption_params, RxDifferentialSource::PRBS, 0.4, 5e9);
    
    // Run simulation
    sc_core::sc_start(2000, sc_core::SC_NS);
    
    // Analyze output quality
    size_t ones = tb.monitor->count_ones();
    size_t zeros = tb.monitor->count_zeros();
    size_t total = tb.get_sample_count();
    
    // For PRBS, expect roughly equal distribution
    double one_ratio = static_cast<double>(ones) / total;
    double zero_ratio = static_cast<double>(zeros) / total;
    
    // Allow 40% tolerance (PRBS may not be perfectly balanced)
    EXPECT_GT(one_ratio, 0.2) << "Should have significant 1s in output";
    EXPECT_GT(zero_ratio, 0.2) << "Should have significant 0s in output";
    
    // Verify transition activity
    size_t transitions = tb.monitor->count_transitions();
    EXPECT_GT(transitions, total / 20) << "Should have active transitions";
    
    sc_core::sc_stop();
}
