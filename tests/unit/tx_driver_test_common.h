/**
 * @file tx_driver_test_common.h
 * @brief Common test infrastructure for TxDriverTdf unit tests
 */

#ifndef SERDES_TESTS_TX_DRIVER_TEST_COMMON_H
#define SERDES_TESTS_TX_DRIVER_TEST_COMMON_H

#include <gtest/gtest.h>
#include <systemc-ams>
#include "ams/tx_driver.h"
#include <cmath>
#include <vector>

namespace serdes {
namespace test {

// ============================================================================
// Differential signal source for testing
// ============================================================================

class TxDriverDifferentialSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out_p;
    sca_tdf::sca_out<double> out_n;
    
    enum WaveformType { DC, SINE, STEP };
    
    TxDriverDifferentialSource(sc_core::sc_module_name nm,
                       WaveformType type = DC,
                       double amplitude = 0.5,
                       double frequency = 1e9,
                       double vcm = 0.0,
                       double step_time = 1e-9)
        : sca_tdf::sca_module(nm)
        , out_p("out_p")
        , out_n("out_n")
        , m_type(type)
        , m_amplitude(amplitude)
        , m_frequency(frequency)
        , m_vcm(vcm)
        , m_step_time(step_time)
    {}
    
    void set_attributes() override {
        out_p.set_rate(1);
        out_n.set_rate(1);
        set_timestep(1.0 / 100e9, sc_core::SC_SEC);
    }
    
    void processing() override {
        double t = get_time().to_seconds();
        double v_diff = 0.0;
        
        switch (m_type) {
            case DC:
                v_diff = m_amplitude;
                break;
            case SINE:
                v_diff = m_amplitude * std::sin(2.0 * M_PI * m_frequency * t);
                break;
            case STEP:
                v_diff = (t >= m_step_time) ? m_amplitude : 0.0;
                break;
        }
        
        out_p.write(m_vcm + 0.5 * v_diff);
        out_n.write(m_vcm - 0.5 * v_diff);
    }
    
private:
    WaveformType m_type;
    double m_amplitude;
    double m_frequency;
    double m_vcm;
    double m_step_time;
};

// ============================================================================
// Constant VDD source
// ============================================================================

class ConstantVddSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out;
    
    ConstantVddSource(sc_core::sc_module_name nm, double voltage = 1.0)
        : sca_tdf::sca_module(nm)
        , out("out")
        , m_voltage(voltage)
    {}
    
    void set_attributes() override {
        out.set_rate(1);
        set_timestep(1.0 / 100e9, sc_core::SC_SEC);
    }
    
    void processing() override {
        out.write(m_voltage);
    }
    
private:
    double m_voltage;
};

// ============================================================================
// VDD source with sinusoidal ripple
// ============================================================================

class VddWithRipple : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out;
    
    VddWithRipple(sc_core::sc_module_name nm, 
                  double nominal = 1.0,
                  double ripple_amplitude = 0.01,
                  double ripple_frequency = 100e6)
        : sca_tdf::sca_module(nm)
        , out("out")
        , m_nominal(nominal)
        , m_ripple_amp(ripple_amplitude)
        , m_ripple_freq(ripple_frequency)
    {}
    
    void set_attributes() override {
        out.set_rate(1);
        set_timestep(1.0 / 100e9, sc_core::SC_SEC);
    }
    
    void processing() override {
        double t = get_time().to_seconds();
        double ripple = m_ripple_amp * std::sin(2.0 * M_PI * m_ripple_freq * t);
        out.write(m_nominal + ripple);
    }
    
private:
    double m_nominal;
    double m_ripple_amp;
    double m_ripple_freq;
};

// ============================================================================
// Signal monitor/sink for capturing output
// ============================================================================

class SignalMonitor : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> in_p;
    sca_tdf::sca_in<double> in_n;
    
    std::vector<double> samples_p;
    std::vector<double> samples_n;
    std::vector<double> samples_diff;
    std::vector<double> samples_cm;
    std::vector<double> time_stamps;
    
    SignalMonitor(sc_core::sc_module_name nm)
        : sca_tdf::sca_module(nm)
        , in_p("in_p")
        , in_n("in_n")
    {}
    
    void set_attributes() override {
        in_p.set_rate(1);
        in_n.set_rate(1);
        set_timestep(1.0 / 100e9, sc_core::SC_SEC);
    }
    
    void processing() override {
        double vp = in_p.read();
        double vn = in_n.read();
        
        samples_p.push_back(vp);
        samples_n.push_back(vn);
        samples_diff.push_back(vp - vn);
        samples_cm.push_back(0.5 * (vp + vn));
        time_stamps.push_back(get_time().to_seconds());
    }
    
    void clear() {
        samples_p.clear();
        samples_n.clear();
        samples_diff.clear();
        samples_cm.clear();
        time_stamps.clear();
    }
    
    double get_dc_diff() const {
        if (samples_diff.empty()) return 0.0;
        size_t start = samples_diff.size() / 10;
        double sum = 0.0;
        for (size_t i = start; i < samples_diff.size(); ++i) {
            sum += samples_diff[i];
        }
        return sum / (samples_diff.size() - start);
    }
    
    double get_dc_cm() const {
        if (samples_cm.empty()) return 0.0;
        size_t start = samples_cm.size() / 10;
        double sum = 0.0;
        for (size_t i = start; i < samples_cm.size(); ++i) {
            sum += samples_cm[i];
        }
        return sum / (samples_cm.size() - start);
    }
    
    double get_rms_diff() const {
        if (samples_diff.empty()) return 0.0;
        size_t start = samples_diff.size() / 10;
        double sum_sq = 0.0;
        for (size_t i = start; i < samples_diff.size(); ++i) {
            sum_sq += samples_diff[i] * samples_diff[i];
        }
        return std::sqrt(sum_sq / (samples_diff.size() - start));
    }
};

// ============================================================================
// TX Driver Testbench
// ============================================================================

class TxDriverTestbench {
public:
    TxDriverDifferentialSource* src;
    sca_tdf::sca_module* vdd_src;
    TxDriverTdf* dut;
    SignalMonitor* monitor;
    
    sca_tdf::sca_signal<double> sig_in_p;
    sca_tdf::sca_signal<double> sig_in_n;
    sca_tdf::sca_signal<double> sig_vdd;
    sca_tdf::sca_signal<double> sig_out_p;
    sca_tdf::sca_signal<double> sig_out_n;
    
    TxDriverTestbench(const TxDriverParams& params,
                      TxDriverDifferentialSource::WaveformType src_type = TxDriverDifferentialSource::DC,
                      double src_amplitude = 0.5,
                      double src_frequency = 1e9,
                      double vdd_nominal = 1.0,
                      bool use_vdd_ripple = false,
                      double ripple_amp = 0.01,
                      double ripple_freq = 100e6)
        : sig_in_p("sig_in_p")
        , sig_in_n("sig_in_n")
        , sig_vdd("sig_vdd")
        , sig_out_p("sig_out_p")
        , sig_out_n("sig_out_n")
    {
        src = new TxDriverDifferentialSource("src", src_type, src_amplitude, src_frequency);
        
        if (use_vdd_ripple) {
            vdd_src = new VddWithRipple("vdd_src", vdd_nominal, ripple_amp, ripple_freq);
        } else {
            vdd_src = new ConstantVddSource("vdd_src", vdd_nominal);
        }
        
        dut = new TxDriverTdf("dut", params);
        monitor = new SignalMonitor("monitor");
        
        src->out_p(sig_in_p);
        src->out_n(sig_in_n);
        
        if (use_vdd_ripple) {
            static_cast<VddWithRipple*>(vdd_src)->out(sig_vdd);
        } else {
            static_cast<ConstantVddSource*>(vdd_src)->out(sig_vdd);
        }
        
        dut->in_p(sig_in_p);
        dut->in_n(sig_in_n);
        dut->vdd(sig_vdd);
        dut->out_p(sig_out_p);
        dut->out_n(sig_out_n);
        
        monitor->in_p(sig_out_p);
        monitor->in_n(sig_out_n);
    }
    
    ~TxDriverTestbench() {
        // SystemC modules are automatically managed by the simulator
    }
};

} // namespace test
} // namespace serdes

#endif // SERDES_TESTS_TX_DRIVER_TEST_COMMON_H
