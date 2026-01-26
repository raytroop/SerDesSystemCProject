/**
 * @file test_tx_driver_psrr_test.cpp
 * @brief Unit test for TX Driver module - PSRR (power supply rejection)
 */

#include "tx_driver_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(TxDriverPsrrTest, PSRRTest) {
    TxDriverParams params;
    params.dc_gain = 1.0;
    params.vswing = 0.8;
    params.vcm_out = 0.6;
    params.output_impedance = 50.0;
    params.sat_mode = "none";
    params.poles.clear();
    
    // Enable PSRR
    params.psrr.enable = true;
    params.psrr.gain = 0.01;  // -40dB PSRR
    params.psrr.poles = {1e9};
    params.psrr.vdd_nom = 1.0;
    
    // DC input, VDD with ripple
    double input_diff = 0.0;  // No signal input
    double vdd_ripple = 0.1;  // 100mV ripple
    double ripple_freq = 100e6;
    
    TxDriverTestbench tb(params, TxDriverDifferentialSource::DC, input_diff, 1e9,
                         1.0, true, vdd_ripple, ripple_freq);
    
    sc_core::sc_start(100, sc_core::SC_NS);
    
    // PSRR coupling should add ripple to differential output
    // Expected: ripple * psrr_gain * voltage_div = 0.1 * 0.01 * 0.5 = 0.0005V RMS
    double actual_rms = tb.monitor->get_rms_diff();
    double expected_rms = vdd_ripple * params.psrr.gain * 0.5 / std::sqrt(2.0);
    
    // Allow larger tolerance due to filter dynamics
    EXPECT_NEAR(actual_rms, expected_rms, expected_rms * 0.5);
    
    sc_core::sc_stop();
}
