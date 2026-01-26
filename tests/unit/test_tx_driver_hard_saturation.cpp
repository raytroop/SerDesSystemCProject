/**
 * @file test_tx_driver_hard_saturation.cpp
 * @brief Unit test for TX Driver module - Hard Saturation (clipping)
 */

#include "tx_driver_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(TxDriverSaturationTest, HardSaturationTest) {
    TxDriverParams params;
    params.dc_gain = 1.0;
    params.vswing = 0.8;  // Vsat = 0.4V
    params.vcm_out = 0.6;
    params.output_impedance = 50.0;
    params.sat_mode = "hard";
    params.poles.clear();
    
    // Large input that should clip
    double input_diff = 2.0;
    
    TxDriverTestbench tb(params, TxDriverDifferentialSource::DC, input_diff);
    
    sc_core::sc_start(100, sc_core::SC_NS);
    
    // With hard saturation, output should be exactly at Vsat * voltage_division
    double expected_diff = (params.vswing / 2.0) * 0.5;  // 0.2V
    double actual_diff = std::abs(tb.monitor->get_dc_diff());
    
    EXPECT_NEAR(actual_diff, expected_diff, 0.01);
    
    sc_core::sc_stop();
}
