/**
 * @file test_tx_driver_common_mode.cpp
 * @brief Unit test for TX Driver module - Common Mode Test
 */

#include "tx_driver_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(TxDriverBasicTest, CommonModeTest) {
    TxDriverParams params;
    params.dc_gain = 1.0;
    params.vswing = 0.8;
    params.vcm_out = 0.6;
    params.output_impedance = 50.0;
    params.sat_mode = "none";
    params.poles.clear();
    
    TxDriverTestbench tb(params, TxDriverDifferentialSource::DC, 0.2);
    
    sc_core::sc_start(100, sc_core::SC_NS);
    
    // Common mode should be vcm_out * voltage_division
    double expected_cm = params.vcm_out * 0.5;  // 0.6 * 0.5 = 0.3
    double actual_cm = tb.monitor->get_dc_cm();
    
    EXPECT_NEAR(actual_cm, expected_cm, 0.02);
    
    sc_core::sc_stop();
}
