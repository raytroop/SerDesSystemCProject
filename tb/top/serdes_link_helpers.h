#ifndef SERDES_LINK_HELPERS_H
#define SERDES_LINK_HELPERS_H

#include <systemc-ams>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace serdes {

// ============================================================================
// Constant VDD Source
// ============================================================================

/**
 * @brief Constant voltage source for VDD supply
 */
class ConstVddSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out;
    
    ConstVddSource(sc_core::sc_module_name nm, double voltage = 1.0)
        : sca_tdf::sca_module(nm)
        , out("out")
        , m_voltage(voltage) {}
    
    void set_attributes() override {
        set_timestep(10.0, sc_core::SC_PS);
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
 * @brief Signal statistics structure
 */
struct SignalStats {
    double min_val;
    double max_val;
    double mean_val;
    double rms_val;
    double peak_to_peak;
    size_t sample_count;
    
    SignalStats()
        : min_val(0.0), max_val(0.0), mean_val(0.0)
        , rms_val(0.0), peak_to_peak(0.0), sample_count(0) {}
};

/**
 * @brief Multi-point signal recorder for SerDes link testbench
 * 
 * Records signals at multiple points in the link:
 * - TX output (differential)
 * - Channel output
 * - RX internal stages (CTLE, VGA, DFE)
 * - RX data output
 */
class MultiPointSignalRecorder : public sca_tdf::sca_module {
public:
    // TX output signals
    sca_tdf::sca_in<double> tx_out_p;
    sca_tdf::sca_in<double> tx_out_n;
    
    // Channel output signal
    sca_tdf::sca_in<double> channel_out;
    
    // RX input signals (after SingleToDiff)
    sca_tdf::sca_in<double> rx_in_p;
    sca_tdf::sca_in<double> rx_in_n;
    
    // RX internal signals
    sca_tdf::sca_in<double> ctle_out_p;
    sca_tdf::sca_in<double> ctle_out_n;
    sca_tdf::sca_in<double> vga_out_p;
    sca_tdf::sca_in<double> vga_out_n;
    sca_tdf::sca_in<double> dfe_out_p;
    sca_tdf::sca_in<double> dfe_out_n;
    
    // RX data output
    sca_tdf::sca_in<double> data_out;
    
    MultiPointSignalRecorder(sc_core::sc_module_name nm)
        : sca_tdf::sca_module(nm)
        , tx_out_p("tx_out_p")
        , tx_out_n("tx_out_n")
        , channel_out("channel_out")
        , rx_in_p("rx_in_p")
        , rx_in_n("rx_in_n")
        , ctle_out_p("ctle_out_p")
        , ctle_out_n("ctle_out_n")
        , vga_out_p("vga_out_p")
        , vga_out_n("vga_out_n")
        , dfe_out_p("dfe_out_p")
        , dfe_out_n("dfe_out_n")
        , data_out("data_out")
    {}
    
    void set_attributes() override {
        set_timestep(10.0, sc_core::SC_PS);
    }
    
    void processing() override {
        double time = get_time().to_seconds();
        
        m_time_stamps.push_back(time);
        m_tx_out_p.push_back(tx_out_p.read());
        m_tx_out_n.push_back(tx_out_n.read());
        m_channel_out.push_back(channel_out.read());
        m_rx_in_p.push_back(rx_in_p.read());
        m_rx_in_n.push_back(rx_in_n.read());
        m_ctle_out_p.push_back(ctle_out_p.read());
        m_ctle_out_n.push_back(ctle_out_n.read());
        m_vga_out_p.push_back(vga_out_p.read());
        m_vga_out_n.push_back(vga_out_n.read());
        m_dfe_out_p.push_back(dfe_out_p.read());
        m_dfe_out_n.push_back(dfe_out_n.read());
        m_data_out.push_back(data_out.read());
    }
    
    /**
     * @brief Save waveform data to CSV file
     */
    void save_waveform_csv(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            return;
        }
        
        file << "time_s,tx_out_p,tx_out_n,tx_out_diff,channel_out,"
             << "rx_in_p,rx_in_n,ctle_out_p,ctle_out_n,ctle_diff,"
             << "vga_out_p,vga_out_n,vga_diff,dfe_out_p,dfe_out_n,dfe_diff,data_out\n";
        
        file << std::scientific << std::setprecision(9);
        
        for (size_t i = 0; i < m_time_stamps.size(); ++i) {
            double tx_diff = m_tx_out_p[i] - m_tx_out_n[i];
            double ctle_diff = m_ctle_out_p[i] - m_ctle_out_n[i];
            double vga_diff = m_vga_out_p[i] - m_vga_out_n[i];
            double dfe_diff = m_dfe_out_p[i] - m_dfe_out_n[i];
            
            file << m_time_stamps[i] << ","
                 << m_tx_out_p[i] << "," << m_tx_out_n[i] << "," << tx_diff << ","
                 << m_channel_out[i] << ","
                 << m_rx_in_p[i] << "," << m_rx_in_n[i] << ","
                 << m_ctle_out_p[i] << "," << m_ctle_out_n[i] << "," << ctle_diff << ","
                 << m_vga_out_p[i] << "," << m_vga_out_n[i] << "," << vga_diff << ","
                 << m_dfe_out_p[i] << "," << m_dfe_out_n[i] << "," << dfe_diff << ","
                 << m_data_out[i] << "\n";
        }
        
        file.close();
        std::cout << "Saved " << m_time_stamps.size() << " samples to " << filename << std::endl;
    }
    
    /**
     * @brief Save eye diagram data to CSV file
     * @param filename Output filename
     * @param ui Unit interval in seconds
     */
    void save_eye_data_csv(const std::string& filename, double ui) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            return;
        }
        
        file << "phase,tx_diff,channel,dfe_diff,data_out\n";
        file << std::scientific << std::setprecision(9);
        
        // Skip initial transient (first 10%)
        size_t start_idx = m_time_stamps.size() / 10;
        
        for (size_t i = start_idx; i < m_time_stamps.size(); ++i) {
            // Calculate phase within UI (0 to 1)
            double phase = std::fmod(m_time_stamps[i], ui) / ui;
            
            double tx_diff = m_tx_out_p[i] - m_tx_out_n[i];
            double dfe_diff = m_dfe_out_p[i] - m_dfe_out_n[i];
            
            file << phase << ","
                 << tx_diff << ","
                 << m_channel_out[i] << ","
                 << dfe_diff << ","
                 << m_data_out[i] << "\n";
        }
        
        file.close();
        std::cout << "Saved eye diagram data to " << filename << std::endl;
    }
    
    /**
     * @brief Calculate signal statistics
     */
    SignalStats calculate_stats(const std::vector<double>& samples, size_t skip_start = 0) const {
        SignalStats stats;
        
        if (samples.empty() || skip_start >= samples.size()) {
            return stats;
        }
        
        size_t start = std::max(skip_start, samples.size() / 10);
        
        stats.min_val = samples[start];
        stats.max_val = samples[start];
        double sum = 0.0;
        double sum_sq = 0.0;
        
        for (size_t i = start; i < samples.size(); ++i) {
            double val = samples[i];
            if (val < stats.min_val) stats.min_val = val;
            if (val > stats.max_val) stats.max_val = val;
            sum += val;
            sum_sq += val * val;
        }
        
        stats.sample_count = samples.size() - start;
        stats.mean_val = sum / stats.sample_count;
        stats.rms_val = std::sqrt(sum_sq / stats.sample_count);
        stats.peak_to_peak = stats.max_val - stats.min_val;
        
        return stats;
    }
    
    /**
     * @brief Print summary statistics
     */
    void print_summary() {
        std::cout << "\n=== SerDes Link Signal Summary ===" << std::endl;
        
        // TX output statistics
        std::vector<double> tx_diff(m_tx_out_p.size());
        for (size_t i = 0; i < m_tx_out_p.size(); ++i) {
            tx_diff[i] = m_tx_out_p[i] - m_tx_out_n[i];
        }
        SignalStats tx_stats = calculate_stats(tx_diff);
        std::cout << "\nTX Output (differential):" << std::endl;
        std::cout << "  Peak-to-peak: " << tx_stats.peak_to_peak * 1000 << " mV" << std::endl;
        std::cout << "  RMS: " << tx_stats.rms_val * 1000 << " mV" << std::endl;
        
        // Channel output statistics
        SignalStats ch_stats = calculate_stats(m_channel_out);
        std::cout << "\nChannel Output:" << std::endl;
        std::cout << "  Peak-to-peak: " << ch_stats.peak_to_peak * 1000 << " mV" << std::endl;
        std::cout << "  Attenuation: " << 20 * std::log10(ch_stats.peak_to_peak / tx_stats.peak_to_peak) << " dB" << std::endl;
        
        // DFE output statistics
        std::vector<double> dfe_diff(m_dfe_out_p.size());
        for (size_t i = 0; i < m_dfe_out_p.size(); ++i) {
            dfe_diff[i] = m_dfe_out_p[i] - m_dfe_out_n[i];
        }
        SignalStats dfe_stats = calculate_stats(dfe_diff);
        std::cout << "\nDFE Output (differential):" << std::endl;
        std::cout << "  Peak-to-peak: " << dfe_stats.peak_to_peak * 1000 << " mV" << std::endl;
        
        // Data output
        SignalStats data_stats = calculate_stats(m_data_out);
        std::cout << "\nData Output:" << std::endl;
        std::cout << "  Min: " << data_stats.min_val << std::endl;
        std::cout << "  Max: " << data_stats.max_val << std::endl;
        std::cout << "  Samples: " << m_time_stamps.size() << std::endl;
    }
    
    // Data accessors
    const std::vector<double>& get_time_stamps() const { return m_time_stamps; }
    const std::vector<double>& get_tx_out_p() const { return m_tx_out_p; }
    const std::vector<double>& get_tx_out_n() const { return m_tx_out_n; }
    const std::vector<double>& get_channel_out() const { return m_channel_out; }
    const std::vector<double>& get_data_out() const { return m_data_out; }
    
private:
    std::vector<double> m_time_stamps;
    std::vector<double> m_tx_out_p;
    std::vector<double> m_tx_out_n;
    std::vector<double> m_channel_out;
    std::vector<double> m_rx_in_p;
    std::vector<double> m_rx_in_n;
    std::vector<double> m_ctle_out_p;
    std::vector<double> m_ctle_out_n;
    std::vector<double> m_vga_out_p;
    std::vector<double> m_vga_out_n;
    std::vector<double> m_dfe_out_p;
    std::vector<double> m_dfe_out_n;
    std::vector<double> m_data_out;
};

// ============================================================================
// BER Calculator
// ============================================================================

/**
 * @brief Simple BER (Bit Error Rate) calculator
 * 
 * Compares transmitted and received data to calculate error rate.
 * Note: Requires proper alignment between TX and RX data.
 */
class BerCalculator {
public:
    BerCalculator() : m_total_bits(0), m_error_bits(0) {}
    
    /**
     * @brief Add comparison result
     * @param tx_bit Transmitted bit (0 or 1)
     * @param rx_bit Received bit (0 or 1)
     */
    void add_comparison(int tx_bit, int rx_bit) {
        m_total_bits++;
        if (tx_bit != rx_bit) {
            m_error_bits++;
        }
    }
    
    /**
     * @brief Get bit error rate
     */
    double get_ber() const {
        if (m_total_bits == 0) return 0.0;
        return static_cast<double>(m_error_bits) / m_total_bits;
    }
    
    /**
     * @brief Get total bit count
     */
    size_t get_total_bits() const { return m_total_bits; }
    
    /**
     * @brief Get error bit count
     */
    size_t get_error_bits() const { return m_error_bits; }
    
    /**
     * @brief Reset counters
     */
    void reset() {
        m_total_bits = 0;
        m_error_bits = 0;
    }
    
private:
    size_t m_total_bits;
    size_t m_error_bits;
};

} // namespace serdes

#endif // SERDES_LINK_HELPERS_H
