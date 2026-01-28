/**
 * @file test_rx_top_basic.cpp
 * @brief Basic functionality tests for RxTopModule
 * 
 * Tests:
 * - Port connection verification
 * - Signal propagation through chain
 * - Output data validity
 */

#include <gtest/gtest.h>
#include <systemc-ams>
#include "rx_top_test_common.h"

using namespace serdes;
using namespace serdes::test;

// ============================================================================
// Basic Port Connection Test
// ============================================================================

TEST(RxTopBasicTest, PortConnectionAndSignalFlow) {
    // Setup
    RxParams params = get_default_rx_params();
    AdaptionParams adaption_params = get_default_adaption_params();
    RxTopTestbench tb(params, adaption_params, RxDifferentialSource::SQUARE, 0.5, 5e9);
    
    // Run simulation
    sc_core::sc_start(500, sc_core::SC_NS);
    
    // Verify samples were collected
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    
    // Verify output has valid data (both 0s and 1s)
    EXPECT_TRUE(tb.monitor->is_valid_data()) 
        << "Output should contain both 0s and 1s for square wave input";
    
    // Verify transitions occurred
    EXPECT_GT(tb.monitor->count_transitions(), 0) 
        << "Output should have transitions for square wave input";
    
    sc_core::sc_stop();
}

// ============================================================================
// DC Input Test
// ============================================================================

TEST(RxTopBasicTest, DcInputResponse) {
    // Setup with positive DC input
    RxParams params = get_default_rx_params();
    AdaptionParams adaption_params = get_default_adaption_params();
    RxTopTestbench tb(params, adaption_params, RxDifferentialSource::DC, 0.5, 10e9);
    
    // Run simulation
    sc_core::sc_start(200, sc_core::SC_NS);
    
    // Verify samples were collected
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    
    // For positive DC input, output should be mostly 1s after settling
    size_t ones = tb.monitor->count_ones();
    size_t total = tb.get_sample_count();
    
    // Allow some initial settling time (first 10% may be unstable)
    EXPECT_GT(ones, total / 2) 
        << "Positive DC input should produce mostly 1s after settling";
    
    sc_core::sc_stop();
}

// ============================================================================
// Negative DC Input Test
// ============================================================================

TEST(RxTopBasicTest, NegativeDcInputResponse) {
    // Setup with negative DC input
    RxParams params = get_default_rx_params();
    AdaptionParams adaption_params = get_default_adaption_params();
    RxTopTestbench tb(params, adaption_params, RxDifferentialSource::DC, -0.5, 10e9);
    
    // Run simulation
    sc_core::sc_start(200, sc_core::SC_NS);
    
    // Verify samples were collected
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    
    // For negative DC input, output should be mostly 0s after settling
    size_t zeros = tb.monitor->count_zeros();
    size_t total = tb.get_sample_count();
    
    EXPECT_GT(zeros, total / 2) 
        << "Negative DC input should produce mostly 0s after settling";
    
    sc_core::sc_stop();
}

// ============================================================================
// Debug Interface Test
// ============================================================================

TEST(RxTopBasicTest, DebugInterfaceAccessible) {
    // Setup
    RxParams params = get_default_rx_params();
    AdaptionParams adaption_params = get_default_adaption_params();
    RxTopTestbench tb(params, adaption_params, RxDifferentialSource::SQUARE, 0.5, 5e9);
    
    // Run brief simulation
    sc_core::sc_start(100, sc_core::SC_NS);
    
    // Verify debug interfaces are accessible (no crash)
    double cdr_phase = tb.get_cdr_phase();
    double cdr_integral = tb.get_cdr_integral_state();
    
    // Values should be finite
    EXPECT_TRUE(std::isfinite(cdr_phase)) << "CDR phase should be finite";
    EXPECT_TRUE(std::isfinite(cdr_integral)) << "CDR integral should be finite";
    
    // Access internal signals via DUT
    const auto& ctle_out_p = tb.dut->get_ctle_out_p_signal();
    const auto& vga_out_p = tb.dut->get_vga_out_p_signal();
    const auto& dfe_out_p = tb.dut->get_dfe_out_p_signal();
    
    // Just verify they exist (no crash)
    (void)ctle_out_p;
    (void)vga_out_p;
    (void)dfe_out_p;
    
    sc_core::sc_stop();
}

// ============================================================================
// PRBS Input Test
// ============================================================================

TEST(RxTopBasicTest, PrbsInputProcessing) {
    // Setup with PRBS input
    RxParams params = get_default_rx_params();
    AdaptionParams adaption_params = get_default_adaption_params();
    RxTopTestbench tb(params, adaption_params, RxDifferentialSource::PRBS, 0.5, 10e9);
    
    // Run longer simulation for PRBS
    sc_core::sc_start(1000, sc_core::SC_NS);
    
    // Verify samples were collected
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    
    // For PRBS input, expect roughly equal 0s and 1s
    size_t ones = tb.monitor->count_ones();
    size_t zeros = tb.monitor->count_zeros();
    size_t total = tb.get_sample_count();
    
    // Allow 30% tolerance for PRBS balance
    double one_ratio = static_cast<double>(ones) / total;
    EXPECT_GT(one_ratio, 0.3) << "PRBS should have significant number of 1s";
    EXPECT_LT(one_ratio, 0.7) << "PRBS should have significant number of 0s";
    
    // Verify transitions
    size_t transitions = tb.monitor->count_transitions();
    EXPECT_GT(transitions, total / 10) << "PRBS should have many transitions";
    
    sc_core::sc_stop();
}
