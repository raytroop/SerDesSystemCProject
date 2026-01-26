/**
 * @file test_tx_driver_gain_mismatch.cpp
 * @brief Unit test for TX Driver module - Gain Mismatch
 */

#include "tx_driver_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(TxDriverPsrrTest, GainMismatchTest) {
    TxDriverParams params;
    params.dc_gain = 1.0;
    params.vswing = 0.8;
    params.vcm_out = 0.6;
    params.output_impedance = 50.0;
    params.sat_mode = "none";
    params.poles.clear();
    params.imbalance.gain_mismatch = 10.0;  // 10% mismatch
    
    double input_diff = 0.4;
    
    TxDriverTestbench tb(params, TxDriverDifferentialSource::DC, input_diff);
    
    sc_core::sc_start(100, sc_core::SC_NS);
    
    // With 10% mismatch:
    // gain_p = 1.0 + 10/200 = 1.05
    // gain_n = 1.0 - 10/200 = 0.95
    
    // Get steady-state values (skip first 10%)
    size_t start = tb.monitor->samples_p.size() / 10;
    double avg_p = 0.0, avg_n = 0.0;
    for (size_t i = start; i < tb.monitor->samples_p.size(); ++i) {
        avg_p += tb.monitor->samples_p[i];
        avg_n += tb.monitor->samples_n[i];
    }
    avg_p /= (tb.monitor->samples_p.size() - start);
    avg_n /= (tb.monitor->samples_n.size() - start);
    
    // Compare the swings relative to the baseline (zero input) to detect gain mismatch
    // For positive input: with gain mismatch, P output increases more than N decreases
    // Calculate the relative changes from the baseline common mode
    double baseline_p = params.vcm_out * 0.5;  // Due to voltage division
    double baseline_n = params.vcm_out * 0.5;  // Due to voltage division
    
    // Measure deviation from baseline for each output
    double dev_p = avg_p - baseline_p;  // Should be positive for positive input
    double dev_n = avg_n - baseline_n;  // Should be negative for positive input
    
    // Calculate swings as absolute deviations from baseline
    double swing_p = std::abs(dev_p);  // Positive value
    double swing_n = std::abs(dev_n);  // Positive value
    
    // With gain mismatch, swing_p should be larger than swing_n
    double mismatch_ratio = swing_p / swing_n;
    EXPECT_NEAR(mismatch_ratio, 1.05 / 0.95, 0.05);
    
    sc_core::sc_stop();
}
