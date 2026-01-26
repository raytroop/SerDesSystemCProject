/**
 * @file test_tx_driver_dc_gain.cpp
 * @brief Unit test for TX Driver module - DC Gain Test
 */

#include "tx_driver_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(TxDriverBasicTest, DCGainTest) {
    TxDriverParams params;
    params.dc_gain = 0.5;
    params.vswing = 1.0;
    params.vcm_out = 0.6;
    params.output_impedance = 50.0;
    params.sat_mode = "none";  // Disable saturation for linear test
    params.poles.clear();      // Disable bandwidth filtering
    
    double input_diff = 0.4;  // 400mV differential input
    
    TxDriverTestbench tb(params, TxDriverDifferentialSource::DC, input_diff);
    
    sc_core::sc_start(100, sc_core::SC_NS);
    
    // Expected: output_diff = input_diff * dc_gain * voltage_division
    // voltage_division = Z0 / (Zout + Z0) = 50 / (50 + 50) = 0.5
    double expected_diff = input_diff * params.dc_gain * 0.5;
    double actual_diff = tb.monitor->get_dc_diff();
    
    EXPECT_NEAR(actual_diff, expected_diff, 0.01);
    
    sc_core::sc_stop();
}
