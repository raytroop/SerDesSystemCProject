/**
 * @file test_tx_driver_slew_rate.cpp
 * @brief Unit test for TX Driver module - Slew Rate Limit
 */

#include "tx_driver_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(TxDriverPsrrTest, SlewRateLimitTest) {
    TxDriverParams params;
    params.dc_gain = 1.0;
    params.vswing = 0.8;
    params.vcm_out = 0.6;
    params.output_impedance = 50.0;
    params.sat_mode = "none";
    params.poles.clear();
    params.slew_rate.enable = true;
    params.slew_rate.max_slew_rate = 1e9;  // 1 V/ns, ensures rise_time >> timestep (10ps)
    
    // Step input
    double step_amp = 0.4;
    
    TxDriverTestbench tb(params, TxDriverDifferentialSource::STEP, step_amp, 1e9, 1.0);
    
    sc_core::sc_start(50, sc_core::SC_NS);
    
    // Find the rise time (10% to 90%)
    double final_value = step_amp * params.dc_gain * 0.5;  // After voltage division
    double v_10 = 0.1 * final_value;
    double v_90 = 0.9 * final_value;
    
    double t_10 = 0.0, t_90 = 0.0;
    bool found_10 = false, found_90 = false;
    
    for (size_t i = 0; i < tb.monitor->samples_diff.size(); ++i) {
        double v = tb.monitor->samples_diff[i];
        if (!found_10 && v >= v_10) {
            t_10 = tb.monitor->time_stamps[i];
            found_10 = true;
        }
        if (!found_90 && v >= v_90) {
            t_90 = tb.monitor->time_stamps[i];
            found_90 = true;
            break;
        }
    }
    
    if (found_10 && found_90) {
        double rise_time = t_90 - t_10;
        // Expected rise time: 0.8 * final_value / slew_rate
        double expected_rise = 0.8 * final_value / params.slew_rate.max_slew_rate;
        
        // Allow 50% tolerance due to discrete time steps
        EXPECT_NEAR(rise_time, expected_rise, expected_rise * 0.5);
    }
    
    sc_core::sc_stop();
}
