/**
 * @file clock_generation_test_common.h
 * @brief Common test infrastructure for ClockGenerationTdf unit tests
 */

#ifndef SERDES_TESTS_CLOCK_GENERATION_TEST_COMMON_H
#define SERDES_TESTS_CLOCK_GENERATION_TEST_COMMON_H

#include <gtest/gtest.h>
#include <systemc-ams>
#include <cmath>
#include <vector>
#include "ams/clock_generation.h"
#include "common/parameters.h"

namespace serdes {
namespace test {

// ============================================================================
// Phase Monitor Module (for testing)
// ============================================================================

class PhaseMonitor : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> phase_in;
    
    std::vector<double> m_phase_samples;
    std::vector<double> m_time_samples;
    int m_sample_limit;
    
    PhaseMonitor(sc_core::sc_module_name nm, int sample_limit = 1000)
        : sca_tdf::sca_module(nm)
        , phase_in("phase_in")
        , m_sample_limit(sample_limit)
    {}
    
    void set_attributes() {
        phase_in.set_rate(1);
    }
    
    void processing() {
        if (static_cast<int>(m_phase_samples.size()) < m_sample_limit) {
            m_phase_samples.push_back(phase_in.read());
            m_time_samples.push_back(sc_core::sc_time_stamp().to_seconds());
        }
    }
    
    // Analysis methods
    double get_mean_phase() const {
        if (m_phase_samples.empty()) return 0.0;
        double sum = 0.0;
        for (double p : m_phase_samples) sum += p;
        return sum / m_phase_samples.size();
    }
    
    double get_max_phase() const {
        if (m_phase_samples.empty()) return 0.0;
        double max_val = m_phase_samples[0];
        for (double p : m_phase_samples) {
            if (p > max_val) max_val = p;
        }
        return max_val;
    }
    
    double get_min_phase() const {
        if (m_phase_samples.empty()) return 0.0;
        double min_val = m_phase_samples[0];
        for (double p : m_phase_samples) {
            if (p < min_val) min_val = p;
        }
        return min_val;
    }
    
    std::vector<double> get_phase_increments() const {
        std::vector<double> increments;
        for (size_t i = 1; i < m_phase_samples.size(); ++i) {
            double delta = m_phase_samples[i] - m_phase_samples[i-1];
            // Handle phase wrap-around
            if (delta < -M_PI) delta += 2.0 * M_PI;
            increments.push_back(delta);
        }
        return increments;
    }
    
    int count_phase_wraps() const {
        int wraps = 0;
        for (size_t i = 1; i < m_phase_samples.size(); ++i) {
            if (m_phase_samples[i] < m_phase_samples[i-1] - M_PI) {
                wraps++;
            }
        }
        return wraps;
    }
};

// ============================================================================
// Clock Generation Testbench
// ============================================================================

SC_MODULE(ClockGenTestbench) {
    ClockGenerationTdf* clk_gen;
    PhaseMonitor* monitor;
    
    sca_tdf::sca_signal<double> sig_phase;
    
    ClockParams params;
    int sample_limit;
    
    ClockGenTestbench(sc_core::sc_module_name nm, const ClockParams& p, int samples = 1000)
        : sc_core::sc_module(nm)
        , params(p)
        , sample_limit(samples)
    {
        clk_gen = new ClockGenerationTdf("clk_gen", params);
        monitor = new PhaseMonitor("monitor", sample_limit);
        
        clk_gen->clk_phase(sig_phase);
        monitor->phase_in(sig_phase);
    }
    
    ~ClockGenTestbench() {
        // SystemC modules are automatically managed by the simulator
    }
    
    // Access monitor data
    const std::vector<double>& get_phase_samples() const {
        return monitor->m_phase_samples;
    }
    
    const std::vector<double>& get_time_samples() const {
        return monitor->m_time_samples;
    }
    
    double get_mean_phase() const { return monitor->get_mean_phase(); }
    double get_max_phase() const { return monitor->get_max_phase(); }
    double get_min_phase() const { return monitor->get_min_phase(); }
    std::vector<double> get_phase_increments() const { return monitor->get_phase_increments(); }
    int count_phase_wraps() const { return monitor->count_phase_wraps(); }
};

} // namespace test
} // namespace serdes

#endif // SERDES_TESTS_CLOCK_GENERATION_TEST_COMMON_H
