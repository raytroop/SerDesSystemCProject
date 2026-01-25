/**
 * @file adaption_test_common.h
 * @brief Common test infrastructure for AdaptionDe unit tests
 */

#ifndef SERDES_TESTS_ADAPTION_TEST_COMMON_H
#define SERDES_TESTS_ADAPTION_TEST_COMMON_H

#include <gtest/gtest.h>
#include <systemc>
#include <cmath>
#include "ams/adaption.h"
#include "common/parameters.h"

namespace serdes {
namespace test {

// ============================================================================
// Simple signal source for unit testing
// ============================================================================
class SimpleAdaptionSource : public sc_core::sc_module {
public:
    sc_core::sc_out<double> phase_error;
    sc_core::sc_out<double> amplitude_rms;
    sc_core::sc_out<int> error_count;
    sc_core::sc_out<double> isi_metric;
    sc_core::sc_out<int> mode;
    sc_core::sc_out<bool> reset;
    sc_core::sc_out<double> scenario_switch;
    
    double m_phase_error;
    double m_amplitude;
    int m_error_count;
    int m_mode;
    
    SC_HAS_PROCESS(SimpleAdaptionSource);
    
    SimpleAdaptionSource(sc_core::sc_module_name nm)
        : sc_core::sc_module(nm)
        , phase_error("phase_error")
        , amplitude_rms("amplitude_rms")
        , error_count("error_count")
        , isi_metric("isi_metric")
        , mode("mode")
        , reset("reset")
        , scenario_switch("scenario_switch")
        , m_phase_error(0.5e-11)
        , m_amplitude(0.3)
        , m_error_count(0)
        , m_mode(2)
    {
        SC_THREAD(source_process);
    }
    
    void source_process() {
        // Initial reset
        reset.write(true);
        wait(sc_core::sc_time(10, sc_core::SC_NS));
        reset.write(false);
        
        while (true) {
            phase_error.write(m_phase_error);
            amplitude_rms.write(m_amplitude);
            error_count.write(m_error_count);
            isi_metric.write(0.1);
            mode.write(m_mode);
            scenario_switch.write(0.0);
            
            wait(sc_core::sc_time(1, sc_core::SC_NS));
        }
    }
    
    void set_phase_error(double val) { m_phase_error = val; }
    void set_amplitude(double val) { m_amplitude = val; }
    void set_error_count(int val) { m_error_count = val; }
    void set_mode(int val) { m_mode = val; }
};

// ============================================================================
// Test fixture for Adaption unit tests
// ============================================================================
class AdaptionBasicTestbench : public sc_core::sc_module {
public:
    SimpleAdaptionSource* src;
    AdaptionDe* adaption;
    
    // Signals
    sc_core::sc_signal<double> sig_phase_error;
    sc_core::sc_signal<double> sig_amplitude_rms;
    sc_core::sc_signal<int> sig_error_count;
    sc_core::sc_signal<double> sig_isi_metric;
    sc_core::sc_signal<int> sig_mode;
    sc_core::sc_signal<bool> sig_reset;
    sc_core::sc_signal<double> sig_scenario_switch;
    
    sc_core::sc_signal<double> sig_vga_gain;
    sc_core::sc_signal<double> sig_ctle_zero;
    sc_core::sc_signal<double> sig_ctle_pole;
    sc_core::sc_signal<double> sig_ctle_dc_gain;
    sc_core::sc_signal<double> sig_dfe_tap1;
    sc_core::sc_signal<double> sig_dfe_tap2;
    sc_core::sc_signal<double> sig_dfe_tap3;
    sc_core::sc_signal<double> sig_dfe_tap4;
    sc_core::sc_signal<double> sig_dfe_tap5;
    sc_core::sc_signal<double> sig_dfe_tap6;
    sc_core::sc_signal<double> sig_dfe_tap7;
    sc_core::sc_signal<double> sig_dfe_tap8;
    sc_core::sc_signal<double> sig_sampler_threshold;
    sc_core::sc_signal<double> sig_sampler_hysteresis;
    sc_core::sc_signal<double> sig_phase_cmd;
    sc_core::sc_signal<int> sig_update_count;
    sc_core::sc_signal<bool> sig_freeze_flag;
    
    AdaptionParams params;
    
    AdaptionBasicTestbench(sc_core::sc_module_name nm, const AdaptionParams& p)
        : sc_core::sc_module(nm)
        , params(p)
    {
        src = new SimpleAdaptionSource("src");
        adaption = new AdaptionDe("adaption", params);
        
        // Connect source
        src->phase_error(sig_phase_error);
        src->amplitude_rms(sig_amplitude_rms);
        src->error_count(sig_error_count);
        src->isi_metric(sig_isi_metric);
        src->mode(sig_mode);
        src->reset(sig_reset);
        src->scenario_switch(sig_scenario_switch);
        
        // Connect adaption inputs
        adaption->phase_error(sig_phase_error);
        adaption->amplitude_rms(sig_amplitude_rms);
        adaption->error_count(sig_error_count);
        adaption->isi_metric(sig_isi_metric);
        adaption->mode(sig_mode);
        adaption->reset(sig_reset);
        adaption->scenario_switch(sig_scenario_switch);
        
        // Connect adaption outputs
        adaption->vga_gain(sig_vga_gain);
        adaption->ctle_zero(sig_ctle_zero);
        adaption->ctle_pole(sig_ctle_pole);
        adaption->ctle_dc_gain(sig_ctle_dc_gain);
        adaption->dfe_tap1(sig_dfe_tap1);
        adaption->dfe_tap2(sig_dfe_tap2);
        adaption->dfe_tap3(sig_dfe_tap3);
        adaption->dfe_tap4(sig_dfe_tap4);
        adaption->dfe_tap5(sig_dfe_tap5);
        adaption->dfe_tap6(sig_dfe_tap6);
        adaption->dfe_tap7(sig_dfe_tap7);
        adaption->dfe_tap8(sig_dfe_tap8);
        adaption->sampler_threshold(sig_sampler_threshold);
        adaption->sampler_hysteresis(sig_sampler_hysteresis);
        adaption->phase_cmd(sig_phase_cmd);
        adaption->update_count(sig_update_count);
        adaption->freeze_flag(sig_freeze_flag);
    }
    
    ~AdaptionBasicTestbench() {
        // SystemC modules are automatically managed by the simulator
        // Do not delete them manually
    }
    
    double get_vga_gain() { return sig_vga_gain.read(); }
    double get_phase_cmd() { return sig_phase_cmd.read(); }
    double get_threshold() { return sig_sampler_threshold.read(); }
    int get_update_count() { return sig_update_count.read(); }
    bool get_freeze_flag() { return sig_freeze_flag.read(); }
    double get_dfe_tap(int idx) {
        switch(idx) {
            case 0: return sig_dfe_tap1.read();
            case 1: return sig_dfe_tap2.read();
            case 2: return sig_dfe_tap3.read();
            case 3: return sig_dfe_tap4.read();
            case 4: return sig_dfe_tap5.read();
            case 5: return sig_dfe_tap6.read();
            case 6: return sig_dfe_tap7.read();
            case 7: return sig_dfe_tap8.read();
            default: return 0.0;
        }
    }
};

} // namespace test
} // namespace serdes

#endif // SERDES_TESTS_ADAPTION_TEST_COMMON_H
