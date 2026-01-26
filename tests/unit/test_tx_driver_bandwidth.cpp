/**
 * @file test_tx_driver_bandwidth.cpp
 * @brief Unit tests for TX Driver module - Bandwidth limiting
 */

#include "tx_driver_test_common.h"

using namespace serdes;
using namespace serdes::test;

// ============================================================================
// Test Case: Bandwidth Test (pole filtering)
// ============================================================================

TEST(TxDriverBandwidthTest, BandwidthTest) {
    TxDriverParams params;
    params.dc_gain = 1.0;
    params.vswing = 1.0;
    params.vcm_out = 0.6;
    params.output_impedance = 50.0;
    params.sat_mode = "none";
    params.poles = {10e9};  // 10 GHz pole
    
    // Test with sine wave at pole frequency (should be -3dB)
    double input_amp = 0.2;
    double test_freq = 10e9;
    
    TxDriverTestbench tb(params, TxDriverDifferentialSource::SINE, input_amp, test_freq);
    
    // Run for several cycles
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // At pole frequency, gain should be reduced by ~3dB (factor of ~0.707)
    // Expected RMS = input_amp * dc_gain * voltage_div * 0.707 / sqrt(2)
    double expected_rms = input_amp * params.dc_gain * 0.5 * 0.707 / std::sqrt(2.0);
    double actual_rms = tb.monitor->get_rms_diff();
    
    // Allow 20% tolerance for numerical accuracy
    EXPECT_NEAR(actual_rms, expected_rms, expected_rms * 0.2);
    
    sc_core::sc_stop();
}
