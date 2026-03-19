/**
 * @file channel_sparam_v2_tb.cpp
 * @brief Testbench for ChannelSParamV2 (C++ core + AMS wrapper)
 * 
 * This testbench validates the V2 channel implementation which uses
 * a pure C++ PoleResidueFilter for numerical stability.
 * 
 * Usage:
 *   ./channel_sparam_v2_tb <config_file.json> [duration_ns]
 */

#include <systemc-ams>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <cmath>

#include "ams/channel_sparam_v2.h"
#include "common/parameters.h"

using namespace serdes;

// ============================================================================
// Simple PRBS Generator (TDF module)
// ============================================================================
SCA_TDF_MODULE(PrbsGenerator) {
    sca_tdf::sca_out<double> out;
    
    PrbsGenerator(sc_core::sc_module_name nm, double sample_rate, int prbs_order = 7)
        : sca_tdf::sca_module(nm)
        , out("out")
        , m_sample_rate(sample_rate)
        , m_prbs_order(prbs_order)
        , m_lfsr(1)
    {}
    
    void set_attributes() override {
        set_timestep(1.0 / m_sample_rate, sc_core::SC_SEC);
        out.set_rate(1);
    }
    
    void processing() override {
        // Simple PRBS-7: x^7 + x^6 + 1
        int bit = ((m_lfsr >> 6) ^ (m_lfsr >> 5)) & 1;
        m_lfsr = ((m_lfsr << 1) | bit) & 0x7F;
        
        // Output NRZ: -1 or +1
        double val = (m_lfsr & 1) ? 1.0 : -1.0;
        out.write(val);
    }
    
private:
    double m_sample_rate;
    int m_prbs_order;
    int m_lfsr;
};

// ============================================================================
// Signal Recorder
// ============================================================================
SCA_TDF_MODULE(SignalRecorder) {
    sca_tdf::sca_in<double> in_signal;
    sca_tdf::sca_in<double> out_signal;
    
    SignalRecorder(sc_core::sc_module_name nm, double sample_rate)
        : sca_tdf::sca_module(nm)
        , in_signal("in_signal")
        , out_signal("out_signal")
        , m_sample_rate(sample_rate)
    {}
    
    void set_attributes() override {
        set_timestep(1.0 / m_sample_rate, sc_core::SC_SEC);
        in_signal.set_rate(1);
        out_signal.set_rate(1);
    }
    
    void processing() override {
        m_time.push_back(sc_core::sc_time_stamp().to_seconds());
        m_input.push_back(in_signal.read());
        m_output.push_back(out_signal.read());
    }
    
    void save_csv(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            return;
        }
        
        file << "time_s,input_V,output_V\n";
        file << std::scientific << std::setprecision(12);
        
        for (size_t i = 0; i < m_time.size(); ++i) {
            file << m_time[i] << "," 
                 << m_input[i] << ","
                 << m_output[i] << "\n";
        }
        
        file.close();
        std::cout << "Saved " << m_time.size() << " samples to " << filename << std::endl;
    }
    
    double get_input_rms() const {
        double sum = 0.0;
        for (double v : m_input) sum += v * v;
        return std::sqrt(sum / m_input.size());
    }
    
    double get_output_rms() const {
        double sum = 0.0;
        for (double v : m_output) sum += v * v;
        return std::sqrt(sum / m_output.size());
    }

private:
    double m_sample_rate;
    std::vector<double> m_time;
    std::vector<double> m_input;
    std::vector<double> m_output;
};

// ============================================================================
// Top-level Testbench
// ============================================================================
SC_MODULE(ChannelV2Testbench) {
    // Signals
    sca_tdf::sca_signal<double> prbs_signal;
    sca_tdf::sca_signal<double> channel_output;
    
    // Modules
    PrbsGenerator* prbs_gen;
    ChannelSParamV2* channel;
    SignalRecorder* recorder;
    
    // Parameters
    double m_sample_rate;
    std::string m_config_file;
    sc_core::sc_time m_duration;
    
    SC_CTOR(ChannelV2Testbench) {
        m_sample_rate = 100e9;  // 100 GHz default
        m_config_file = "";
        m_duration = sc_core::sc_time(1000, sc_core::SC_NS);  // 1 us default
    }
    
    void set_params(const std::string& config_file, double duration_ns) {
        m_config_file = config_file;
        m_duration = sc_core::sc_time(duration_ns, sc_core::SC_NS);
    }
    
    void build() {
        std::cout << "\n========================================\n";
        std::cout << "Channel SParam V2 Testbench\n";
        std::cout << "========================================\n";
        
        // Create PRBS generator
        prbs_gen = new PrbsGenerator("prbs_gen", m_sample_rate, 7);
        prbs_gen->out(prbs_signal);
        
        // Create channel
        ChannelExtendedParams params;
        params.fs = m_sample_rate;
        params.config_file = m_config_file;
        
        std::cout << "Creating Channel (V2) with config: " 
                  << (m_config_file.empty() ? "(none)" : m_config_file) << "\n";
        
        channel = new ChannelSParamV2("channel", params);
        channel->in(prbs_signal);
        channel->out(channel_output);
        
        // Create recorder
        recorder = new SignalRecorder("recorder", m_sample_rate);
        recorder->in_signal(prbs_signal);
        recorder->out_signal(channel_output);
        
        std::cout << "System built successfully\n";
        std::cout << "  Sample rate: " << m_sample_rate/1e9 << " GHz\n";
        std::cout << "  Duration: " << m_duration.to_double() * 1e9 << " ns\n";
    }
    
    void run() {
        std::cout << "\nStarting simulation...\n";
        sc_core::sc_start(m_duration);
        std::cout << "Simulation complete\n";
    }
    
    void report() {
        std::cout << "\n========================================\n";
        std::cout << "Test Results\n";
        std::cout << "========================================\n";
        
        double in_rms = recorder->get_input_rms();
        double out_rms = recorder->get_output_rms();
        double gain_db = 20.0 * std::log10(out_rms / in_rms);
        
        std::cout << "Input RMS:  " << in_rms << " V\n";
        std::cout << "Output RMS: " << out_rms << " V\n";
        std::cout << "Gain:       " << gain_db << " dB\n";
        std::cout << "DC Gain:    " << channel->get_dc_gain() << "\n";
        
        // Save waveform
        recorder->save_csv("channel_v2_waveform.csv");
        
        std::cout << "\n========================================\n";
    }
};

// ============================================================================
// Main
// ============================================================================
void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <config_file.json> [duration_ns]\n";
    std::cout << "\nArguments:\n";
    std::cout << "  config_file   Path to pole-residue JSON config (required)\n";
    std::cout << "  duration_ns   Simulation duration in nanoseconds (default: 1000)\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << prog << " channel_pole_residue.json 500\n";
}

int sc_main(int argc, char* argv[]) {
    // Disable SystemC deprecation warnings
    sc_core::sc_report_handler::set_actions(
        "/IEEE_Std_1666/deprecated",
        sc_core::SC_DO_NOTHING
    );
    
    // Parse arguments
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string config_file = argv[1];
    double duration_ns = 1000.0;  // Default 1 us
    
    if (argc > 2) {
        duration_ns = std::stod(argv[2]);
    }
    
    // Create and run testbench
    ChannelV2Testbench tb("tb");
    tb.set_params(config_file, duration_ns);
    tb.build();
    tb.run();
    tb.report();
    
    return 0;
}
