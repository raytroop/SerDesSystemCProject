/**
 * @file rx_link_helpers.h
 * @brief Helper modules for RX link testbench
 * 
 * This file contains auxiliary modules for the RX link testbench:
 * - Signal sources (PRBS, constant, differential)
 * - Signal converters (single-to-diff, diff-to-single)
 * - Multi-point signal recorder for waveform capture
 * - CSV export utilities
 */

#ifndef SERDES_RX_LINK_HELPERS_H
#define SERDES_RX_LINK_HELPERS_H

#include <systemc-ams>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <cmath>

namespace serdes {

// ============================================================================
// Constants
// ============================================================================

constexpr double RX_LINK_DEFAULT_SAMPLE_RATE = 100e9;  // 100 GHz (10ps timestep)
constexpr double RX_LINK_DEFAULT_UI = 100e-12;         // 100 ps (10 Gbps)

// ============================================================================
// Single-to-Differential Converter
// ============================================================================

/**
 * @brief Converts single-ended signal to differential pair
 * 
 * out_p = vcm + 0.5 * in
 * out_n = vcm - 0.5 * in
 */
class SingleToDiffConverter : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> in;
    sca_tdf::sca_out<double> out_p;
    sca_tdf::sca_out<double> out_n;
    
    SingleToDiffConverter(sc_core::sc_module_name nm, double vcm = 0.0)
        : sca_tdf::sca_module(nm)
        , in("in")
        , out_p("out_p")
        , out_n("out_n")
        , m_vcm(vcm)
    {}
    
    void set_attributes() override {
        in.set_rate(1);
        out_p.set_rate(1);
        out_n.set_rate(1);
    }
    
    void processing() override {
        double val = in.read();
        out_p.write(m_vcm + 0.5 * val);
        out_n.write(m_vcm - 0.5 * val);
    }
    
private:
    double m_vcm;
};

// ============================================================================
// Constant VDD Source
// ============================================================================

/**
 * @brief Constant voltage source for VDD port
 */
class ConstVddSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out;
    
    ConstVddSource(sc_core::sc_module_name nm, double voltage = 1.0)
        : sca_tdf::sca_module(nm)
        , out("out")
        , m_voltage(voltage)
    {}
    
    void set_attributes() override {
        out.set_rate(1);
        out.set_timestep(1.0 / RX_LINK_DEFAULT_SAMPLE_RATE, sc_core::SC_SEC);
    }
    
    void processing() override {
        out.write(m_voltage);
    }
    
private:
    double m_voltage;
};

// ============================================================================
// Multi-Point Signal Recorder
// ============================================================================

/**
 * @brief Records signals from multiple points in the RX chain
 * 
 * Captures:
 * - Channel output (RX input): ch_out_p, ch_out_n
 * - CTLE output: ctle_out_p, ctle_out_n
 * - VGA output: vga_out_p, vga_out_n
 * - DFE output (Sampler input): dfe_out_p, dfe_out_n
 * - Sampler output: sampler_out
 * - Timestamps
 * 
 * Note: CDR phase is accessed via RxTopModule debug interface, not signal port.
 */
class MultiPointSignalRecorder : public sca_tdf::sca_module {
public:
    // Input ports - Channel output
    sca_tdf::sca_in<double> ch_out_p;
    sca_tdf::sca_in<double> ch_out_n;
    
    // Input ports - CTLE output
    sca_tdf::sca_in<double> ctle_out_p;
    sca_tdf::sca_in<double> ctle_out_n;
    
    // Input ports - VGA output
    sca_tdf::sca_in<double> vga_out_p;
    sca_tdf::sca_in<double> vga_out_n;
    
    // Input ports - DFE output (dual DFE)
    sca_tdf::sca_in<double> dfe_out_p;
    sca_tdf::sca_in<double> dfe_out_n;
    
    // Input ports - Sampler output
    sca_tdf::sca_in<double> sampler_out;
    
    MultiPointSignalRecorder(sc_core::sc_module_name nm)
        : sca_tdf::sca_module(nm)
        , ch_out_p("ch_out_p")
        , ch_out_n("ch_out_n")
        , ctle_out_p("ctle_out_p")
        , ctle_out_n("ctle_out_n")
        , vga_out_p("vga_out_p")
        , vga_out_n("vga_out_n")
        , dfe_out_p("dfe_out_p")
        , dfe_out_n("dfe_out_n")
        , sampler_out("sampler_out")
    {}
    
    void set_attributes() override {
        // All inputs at same rate
        ch_out_p.set_rate(1);
        ch_out_n.set_rate(1);
        ctle_out_p.set_rate(1);
        ctle_out_n.set_rate(1);
        vga_out_p.set_rate(1);
        vga_out_n.set_rate(1);
        dfe_out_p.set_rate(1);
        dfe_out_n.set_rate(1);
        sampler_out.set_rate(1);
    }
    
    void processing() override {
        // Record timestamp
        time_stamps.push_back(get_time().to_seconds());
        
        // Record Channel output
        double ch_p = ch_out_p.read();
        double ch_n = ch_out_n.read();
        ch_samples_p.push_back(ch_p);
        ch_samples_n.push_back(ch_n);
        ch_samples_diff.push_back(ch_p - ch_n);
        
        // Record CTLE output
        double ctle_p = ctle_out_p.read();
        double ctle_n = ctle_out_n.read();
        ctle_samples_p.push_back(ctle_p);
        ctle_samples_n.push_back(ctle_n);
        ctle_samples_diff.push_back(ctle_p - ctle_n);
        
        // Record VGA output
        double vga_p = vga_out_p.read();
        double vga_n = vga_out_n.read();
        vga_samples_p.push_back(vga_p);
        vga_samples_n.push_back(vga_n);
        vga_samples_diff.push_back(vga_p - vga_n);
        
        // Record DFE output
        double dfe_p = dfe_out_p.read();
        double dfe_n = dfe_out_n.read();
        dfe_samples_p.push_back(dfe_p);
        dfe_samples_n.push_back(dfe_n);
        dfe_samples_diff.push_back(dfe_p - dfe_n);
        
        // Record Sampler output
        sampler_samples.push_back(sampler_out.read());
    }
    
    /**
     * @brief Save complete waveform data to CSV file
     * @param filename Output file path
     */
    void save_waveform_csv(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            return;
        }
        
        // Write header
        file << "time_s,"
             << "ch_out_p_V,ch_out_n_V,ch_out_diff_V,"
             << "ctle_out_p_V,ctle_out_n_V,ctle_out_diff_V,"
             << "vga_out_p_V,vga_out_n_V,vga_out_diff_V,"
             << "dfe_out_p_V,dfe_out_n_V,dfe_out_diff_V,"
             << "sampler_out\n";
        
        file << std::scientific << std::setprecision(9);
        
        for (size_t i = 0; i < time_stamps.size(); ++i) {
            file << time_stamps[i] << ","
                 << ch_samples_p[i] << "," << ch_samples_n[i] << "," << ch_samples_diff[i] << ","
                 << ctle_samples_p[i] << "," << ctle_samples_n[i] << "," << ctle_samples_diff[i] << ","
                 << vga_samples_p[i] << "," << vga_samples_n[i] << "," << vga_samples_diff[i] << ","
                 << dfe_samples_p[i] << "," << dfe_samples_n[i] << "," << dfe_samples_diff[i] << ","
                 << sampler_samples[i] << "\n";
        }
        
        file.close();
        std::cout << "Saved " << time_stamps.size() << " samples to " << filename << std::endl;
    }
    
    /**
     * @brief Save eye diagram data to CSV file
     * @param filename Output file path
     * @param ui Unit interval in seconds (for time normalization)
     */
    void save_eye_data_csv(const std::string& filename, double ui = RX_LINK_DEFAULT_UI) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            return;
        }
        
        // Write header with explanation
        // All signals are module OUTPUTS (i.e., next module's INPUT)
        file << "time_in_ui,ch_diff_V,ctle_diff_V,vga_diff_V,dfe_diff_V\n";
        
        file << std::scientific << std::setprecision(9);
        
        for (size_t i = 0; i < time_stamps.size(); ++i) {
            // Normalize time to UI
            double time_in_ui = std::fmod(time_stamps[i], ui) / ui;
            
            file << time_in_ui << ","
                 << ch_samples_diff[i] << ","
                 << ctle_samples_diff[i] << ","
                 << vga_samples_diff[i] << ","
                 << dfe_samples_diff[i] << "\n";
        }
        
        file.close();
        std::cout << "Saved " << time_stamps.size() << " eye samples to " << filename << std::endl;
    }
    
    /**
     * @brief Print summary statistics
     */
    void print_summary() {
        if (time_stamps.empty()) {
            std::cout << "No samples recorded." << std::endl;
            return;
        }
        
        std::cout << "\n=== RX Link Signal Summary ===" << std::endl;
        std::cout << "Total samples: " << time_stamps.size() << std::endl;
        std::cout << "Time range: " << time_stamps.front() * 1e9 << " ns to " 
                  << time_stamps.back() * 1e9 << " ns" << std::endl;
        
        // Calculate statistics for DFE output (Sampler input)
        double dfe_min = dfe_samples_diff[0];
        double dfe_max = dfe_samples_diff[0];
        double dfe_sum = 0.0;
        
        for (const auto& v : dfe_samples_diff) {
            if (v < dfe_min) dfe_min = v;
            if (v > dfe_max) dfe_max = v;
            dfe_sum += v;
        }
        
        double dfe_mean = dfe_sum / dfe_samples_diff.size();
        double dfe_pp = dfe_max - dfe_min;
        
        std::cout << "\nDFE Output (Sampler Input):" << std::endl;
        std::cout << "  Peak-to-peak: " << dfe_pp * 1000 << " mV" << std::endl;
        std::cout << "  Max: " << dfe_max * 1000 << " mV" << std::endl;
        std::cout << "  Min: " << dfe_min * 1000 << " mV" << std::endl;
        std::cout << "  Mean: " << dfe_mean * 1000 << " mV" << std::endl;
    }
    
    // Public data storage for external access
    std::vector<double> time_stamps;
    std::vector<double> ch_samples_p, ch_samples_n, ch_samples_diff;
    std::vector<double> ctle_samples_p, ctle_samples_n, ctle_samples_diff;
    std::vector<double> vga_samples_p, vga_samples_n, vga_samples_diff;
    std::vector<double> dfe_samples_p, dfe_samples_n, dfe_samples_diff;
    std::vector<double> sampler_samples;
};

// ============================================================================
// Signal Statistics Calculator
// ============================================================================

/**
 * @brief Calculate basic statistics for a signal vector
 */
struct SignalStats {
    double mean = 0.0;
    double rms = 0.0;
    double min_val = 0.0;
    double max_val = 0.0;
    double peak_to_peak = 0.0;
    double std_dev = 0.0;
    
    static SignalStats compute(const std::vector<double>& samples, size_t skip_initial = 0) {
        SignalStats stats;
        
        if (samples.size() <= skip_initial) {
            return stats;
        }
        
        size_t start = skip_initial;
        size_t count = samples.size() - start;
        
        // Mean
        double sum = 0.0;
        for (size_t i = start; i < samples.size(); ++i) {
            sum += samples[i];
        }
        stats.mean = sum / count;
        
        // Min, Max, RMS, StdDev
        stats.min_val = samples[start];
        stats.max_val = samples[start];
        double sum_sq = 0.0;
        double sum_dev_sq = 0.0;
        
        for (size_t i = start; i < samples.size(); ++i) {
            double v = samples[i];
            if (v < stats.min_val) stats.min_val = v;
            if (v > stats.max_val) stats.max_val = v;
            sum_sq += v * v;
            double dev = v - stats.mean;
            sum_dev_sq += dev * dev;
        }
        
        stats.rms = std::sqrt(sum_sq / count);
        stats.std_dev = std::sqrt(sum_dev_sq / count);
        stats.peak_to_peak = stats.max_val - stats.min_val;
        
        return stats;
    }
};

} // namespace serdes

#endif // SERDES_RX_LINK_HELPERS_H
