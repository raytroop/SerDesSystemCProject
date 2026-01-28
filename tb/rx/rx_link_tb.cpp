/**
 * @file rx_link_tb.cpp
 * @brief Transient testbench for RX Link (complete RX chain)
 * 
 * This testbench performs complete RX chain simulation:
 * WaveGen → Channel → CTLE → VGA → DFE_P/DFE_N → Sampler → CDR
 * 
 * Key features:
 * - Dual DFE Summer architecture for differential processing
 * - CDR closed-loop mode (phase-driven sampling)
 * - Multi-point signal recording for eye diagram analysis
 * - CSV export with timestamps for all key nodes
 * 
 * Test scenarios:
 * 0. BASIC_LINK     - Basic link establishment
 * 1. CTLE_SWEEP     - CTLE parameter sweep
 * 2. CDR_LOCK_TEST  - CDR locking behavior
 * 3. EYE_DIAGRAM    - Eye diagram data collection
 * 
 * Usage:
 *   ./rx_link_tb [scenario]
 * 
 * Examples:
 *   ./rx_link_tb basic
 *   ./rx_link_tb eye
 */

#include <systemc-ams>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <iomanip>
#include <cstring>

// Project headers
#include "ams/wave_generation.h"
#include "ams/channel_sparam.h"
#include "ams/rx_ctle.h"
#include "ams/rx_vga.h"
#include "ams/rx_dfe.h"
#include "ams/rx_sampler.h"
#include "ams/rx_cdr.h"
#include "common/parameters.h"

// Local helpers
#include "rx_link_helpers.h"

using namespace serdes;

// ============================================================================
// Test Scenarios
// ============================================================================

enum TestScenario {
    BASIC_LINK = 0,
    CTLE_SWEEP,
    CDR_LOCK_TEST,
    EYE_DIAGRAM
};

std::map<std::string, TestScenario> scenario_map = {
    {"basic", BASIC_LINK},
    {"ctle_sweep", CTLE_SWEEP},
    {"cdr_lock", CDR_LOCK_TEST},
    {"eye", EYE_DIAGRAM},
    {"0", BASIC_LINK},
    {"1", CTLE_SWEEP},
    {"2", CDR_LOCK_TEST},
    {"3", EYE_DIAGRAM}
};

std::string get_scenario_name(TestScenario scenario) {
    switch (scenario) {
        case BASIC_LINK: return "basic";
        case CTLE_SWEEP: return "ctle_sweep";
        case CDR_LOCK_TEST: return "cdr_lock";
        case EYE_DIAGRAM: return "eye";
        default: return "unknown";
    }
}

// ============================================================================
// RX Link Testbench
// ============================================================================

SC_MODULE(RxLinkTestbench) {
    // ========================================================================
    // Sub-modules
    // ========================================================================
    
    // Signal sources
    WaveGenerationTdf* wavegen;
    ConstVddSource* vdd_src;
    
    // Channel
    ChannelSParamTdf* channel;
    
    // Single-to-Differential converter (Channel output is single-ended)
    SingleToDiffConverter* s2d;
    
    // RX Front-end (differential)
    RxCtleTdf* ctle;
    RxVgaTdf* vga;
    
    // DFE Summer (dual instances for differential)
    RxDfeTdf* dfe_p;    // P path, normal taps
    RxDfeTdf* dfe_n;    // N path, negated taps
    
    // Differential-to-Single converters for DFE input
    DiffToSingleConverter* d2s_p;
    DiffToSingleConverter* d2s_n;
    
    // RX Back-end
    RxSamplerTdf* sampler;
    RxCdrTdf* cdr;
    
    // Auxiliary sources for Sampler ports
    ConstClockSource* clk_src;
    
    // Signal recorder
    MultiPointSignalRecorder* recorder;
    
    // ========================================================================
    // Signals
    // ========================================================================
    
    // WaveGen → Channel
    sca_tdf::sca_signal<double> sig_wavegen_out;
    sca_tdf::sca_signal<double> sig_channel_out;
    
    // Channel → CTLE (differential via converter)
    sca_tdf::sca_signal<double> sig_ch_out_p;
    sca_tdf::sca_signal<double> sig_ch_out_n;
    
    // CTLE → VGA
    sca_tdf::sca_signal<double> sig_ctle_out_p;
    sca_tdf::sca_signal<double> sig_ctle_out_n;
    
    // VGA → DFE (need to convert diff to single for each DFE)
    sca_tdf::sca_signal<double> sig_vga_out_p;
    sca_tdf::sca_signal<double> sig_vga_out_n;
    
    // VGA output split for DFE inputs
    sca_tdf::sca_signal<double> sig_vga_single_p;  // For DFE_P: vga_out_p only
    sca_tdf::sca_signal<double> sig_vga_single_n;  // For DFE_N: vga_out_n only (negated)
    
    // DFE → Sampler
    sca_tdf::sca_signal<double> sig_dfe_out_p;
    sca_tdf::sca_signal<double> sig_dfe_out_n;
    
    // Sampler → CDR → Sampler (closed loop)
    sca_tdf::sca_signal<double> sig_sampler_out;
    sca_tdf::sca_signal<double> sig_cdr_phase;
    
    // VDD
    sca_tdf::sca_signal<double> sig_vdd;
    
    // Clock (unused but required for port connection)
    sca_tdf::sca_signal<double> sig_clk;
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    WaveGenParams m_wave_params;
    ChannelParams m_channel_params;
    RxCtleParams m_ctle_params;
    RxVgaParams m_vga_params;
    RxDfeParams m_dfe_params_p;
    RxDfeParams m_dfe_params_n;  // Negated taps
    RxSamplerParams m_sampler_params;
    CdrParams m_cdr_params;
    
    double m_sim_duration;
    double m_sample_rate;
    double m_ui;
    std::string m_output_prefix;
    
    // ========================================================================
    // Constructor
    // ========================================================================
    
    SC_CTOR(RxLinkTestbench)
        : sig_wavegen_out("sig_wavegen_out")
        , sig_channel_out("sig_channel_out")
        , sig_ch_out_p("sig_ch_out_p")
        , sig_ch_out_n("sig_ch_out_n")
        , sig_ctle_out_p("sig_ctle_out_p")
        , sig_ctle_out_n("sig_ctle_out_n")
        , sig_vga_out_p("sig_vga_out_p")
        , sig_vga_out_n("sig_vga_out_n")
        , sig_vga_single_p("sig_vga_single_p")
        , sig_vga_single_n("sig_vga_single_n")
        , sig_dfe_out_p("sig_dfe_out_p")
        , sig_dfe_out_n("sig_dfe_out_n")
        , sig_sampler_out("sig_sampler_out")
        , sig_cdr_phase("sig_cdr_phase")
        , sig_vdd("sig_vdd")
        , sig_clk("sig_clk")
        , m_sim_duration(2000e-9)
        , m_sample_rate(100e9)
        , m_ui(100e-12)
        , m_output_prefix("rx_link")
    {
    }
    
    // ========================================================================
    // Configuration Methods
    // ========================================================================
    
    void configure(TestScenario scenario) {
        // Default parameters
        configure_defaults();
        
        std::string scenario_name = get_scenario_name(scenario);
        m_output_prefix = "rx_link_" + scenario_name;
        
        switch (scenario) {
            case BASIC_LINK:
                configure_basic();
                break;
            case CTLE_SWEEP:
                configure_ctle_sweep();
                break;
            case CDR_LOCK_TEST:
                configure_cdr_lock();
                break;
            case EYE_DIAGRAM:
                configure_eye_diagram();
                break;
            default:
                configure_basic();
                break;
        }
        
        // Setup negated taps for DFE_N
        setup_dfe_negated_taps();
    }
    
    void configure_defaults() {
        // Wave generation
        m_wave_params = WaveGenParams();
        m_wave_params.type = PRBSType::PRBS31;
        m_wave_params.single_pulse = 0.0;
        
        // Channel - simple model
        m_channel_params = ChannelParams();
        m_channel_params.attenuation_db = 6.0;
        m_channel_params.bandwidth_hz = 15e9;
        
        // CTLE
        m_ctle_params = RxCtleParams();
        m_ctle_params.zeros = {2e9};
        m_ctle_params.poles = {30e9};
        m_ctle_params.dc_gain = 1.5;
        m_ctle_params.vcm_out = 0.0;
        
        // VGA
        m_vga_params = RxVgaParams();
        m_vga_params.zeros = {1e9};
        m_vga_params.poles = {20e9};
        m_vga_params.dc_gain = 2.0;
        m_vga_params.vcm_out = 0.0;
        
        // DFE (P path - normal taps)
        m_dfe_params_p = RxDfeParams();
        m_dfe_params_p.taps = {-0.05, -0.02, 0.01};
        
        // Sampler - phase-driven mode (CDR closed loop)
        m_sampler_params = RxSamplerParams();
        m_sampler_params.phase_source = "phase";
        m_sampler_params.threshold = 0.0;
        m_sampler_params.hysteresis = 0.02;
        m_sampler_params.resolution = 0.02;
        
        // CDR
        m_cdr_params = CdrParams();
        m_cdr_params.pi.kp = 0.01;
        m_cdr_params.pi.ki = 1e-4;
        m_cdr_params.pi.edge_threshold = 0.5;
        m_cdr_params.pai.resolution = 1e-12;
        m_cdr_params.pai.range = 5e-11;
        m_cdr_params.ui = 100e-12;
        
        // Timing
        m_sample_rate = 100e9;  // 100 GS/s
        m_ui = 100e-12;         // 100 ps (10 Gbps)
        m_sim_duration = 2000e-9;  // 2 us
    }
    
    void setup_dfe_negated_taps() {
        // N path: negated taps for differential symmetry
        m_dfe_params_n = m_dfe_params_p;
        m_dfe_params_n.taps.clear();
        for (double t : m_dfe_params_p.taps) {
            m_dfe_params_n.taps.push_back(-t);
        }
    }
    
    void configure_basic() {
        std::cout << "Configuring BASIC_LINK test..." << std::endl;
        m_sim_duration = 2000e-9;  // 2 us
    }
    
    void configure_ctle_sweep() {
        std::cout << "Configuring CTLE_SWEEP test..." << std::endl;
        // Higher CTLE gain
        m_ctle_params.dc_gain = 2.0;
        m_ctle_params.zeros = {3e9};
        m_sim_duration = 2000e-9;
    }
    
    void configure_cdr_lock() {
        std::cout << "Configuring CDR_LOCK_TEST..." << std::endl;
        // Longer simulation for CDR to lock
        m_sim_duration = 5000e-9;  // 5 us
        // Stronger CDR gains
        m_cdr_params.pi.kp = 0.02;
        m_cdr_params.pi.ki = 2e-4;
    }
    
    void configure_eye_diagram() {
        std::cout << "Configuring EYE_DIAGRAM test..." << std::endl;
        // Collect enough data for eye diagram
        m_sim_duration = 10000e-9;  // 10 us for good statistics
    }
    
    // ========================================================================
    // Build Method
    // ========================================================================
    
    void build() {
        std::cout << "Building RX link testbench..." << std::endl;
        
        // Create WaveGen
        wavegen = new WaveGenerationTdf("wavegen", m_wave_params, m_sample_rate, 12345);
        
        // Create Channel
        channel = new ChannelSParamTdf("channel", m_channel_params);
        
        // Create Single-to-Diff converter (Channel output → CTLE input)
        s2d = new SingleToDiffConverter("s2d", 0.0);
        
        // Create VDD source
        vdd_src = new ConstVddSource("vdd_src", 1.0);
        
        // Create CTLE
        ctle = new RxCtleTdf("ctle", m_ctle_params);
        
        // Create VGA
        vga = new RxVgaTdf("vga", m_vga_params);
        
        // Create Diff-to-Single converters for DFE inputs
        // DFE_P receives: vga_out_p (single-ended extraction)
        // DFE_N receives: vga_out_n (single-ended extraction, will be negated in tap)
        d2s_p = new DiffToSingleConverter("d2s_p");  // For splitting purposes
        d2s_n = new DiffToSingleConverter("d2s_n");  // Actually just pass-through
        
        // Create DFE instances (dual)
        dfe_p = new RxDfeTdf("dfe_p", m_dfe_params_p);
        dfe_n = new RxDfeTdf("dfe_n", m_dfe_params_n);
        
        // Create Sampler
        sampler = new RxSamplerTdf("sampler", m_sampler_params);
        
        // Create CDR
        cdr = new RxCdrTdf("cdr", m_cdr_params);
        
        // Create Clock source (required but not used in phase-driven mode)
        clk_src = new ConstClockSource("clk_src", 0.0);
        
        // Create Recorder
        recorder = new MultiPointSignalRecorder("recorder");
        
        // ====================================================================
        // Connect Signals
        // ====================================================================
        
        // WaveGen → Channel
        wavegen->out(sig_wavegen_out);
        channel->in(sig_wavegen_out);
        channel->out(sig_channel_out);
        
        // Channel → S2D → CTLE
        s2d->in(sig_channel_out);
        s2d->out_p(sig_ch_out_p);
        s2d->out_n(sig_ch_out_n);
        
        // VDD source
        vdd_src->out(sig_vdd);
        
        // CTLE connections
        ctle->in_p(sig_ch_out_p);
        ctle->in_n(sig_ch_out_n);
        ctle->vdd(sig_vdd);
        ctle->out_p(sig_ctle_out_p);
        ctle->out_n(sig_ctle_out_n);
        
        // VGA connections
        vga->in_p(sig_ctle_out_p);
        vga->in_n(sig_ctle_out_n);
        vga->vdd(sig_vdd);
        vga->out_p(sig_vga_out_p);
        vga->out_n(sig_vga_out_n);
        
        // DFE_P: input = VGA out_p (need single-ended)
        // DFE_N: input = VGA out_n (need single-ended, taps already negated)
        // Since DFE takes single-ended input, we directly connect VGA outputs
        dfe_p->in(sig_vga_out_p);
        dfe_p->out(sig_dfe_out_p);
        
        dfe_n->in(sig_vga_out_n);
        dfe_n->out(sig_dfe_out_n);
        
        // Sampler connections
        sampler->in_p(sig_dfe_out_p);
        sampler->in_n(sig_dfe_out_n);
        sampler->phase_offset(sig_cdr_phase);  // CDR → Sampler (closed loop)
        sampler->clk_sample(sig_clk);          // Required but not used
        sampler->data_out(sig_sampler_out);
        // Note: data_out_de port is optional, not connected
        
        // CDR connections (closed loop)
        cdr->in(sig_sampler_out);
        cdr->phase_out(sig_cdr_phase);
        
        // Clock source (required for port)
        clk_src->out(sig_clk);
        
        // Recorder connections
        recorder->ch_out_p(sig_ch_out_p);
        recorder->ch_out_n(sig_ch_out_n);
        recorder->ctle_out_p(sig_ctle_out_p);
        recorder->ctle_out_n(sig_ctle_out_n);
        recorder->vga_out_p(sig_vga_out_p);
        recorder->vga_out_n(sig_vga_out_n);
        recorder->dfe_out_p(sig_dfe_out_p);
        recorder->dfe_out_n(sig_dfe_out_n);
        recorder->sampler_out(sig_sampler_out);
        recorder->cdr_phase(sig_cdr_phase);
        
        std::cout << "RX link testbench built successfully." << std::endl;
    }
    
    // ========================================================================
    // Run Method
    // ========================================================================
    
    void run() {
        std::cout << "Running RX link simulation for " << m_sim_duration * 1e9 << " ns..." << std::endl;
        std::cout << "  Sample rate: " << m_sample_rate / 1e9 << " GS/s" << std::endl;
        std::cout << "  Data rate: " << 1.0 / m_ui / 1e9 << " Gbps" << std::endl;
        std::cout << "  UI: " << m_ui * 1e12 << " ps" << std::endl;
        
        sc_core::sc_start(m_sim_duration, sc_core::SC_SEC);
        
        std::cout << "Simulation completed." << std::endl;
    }
    
    // ========================================================================
    // Save Results
    // ========================================================================
    
    void save_results() {
        // Save waveform data
        std::string waveform_file = m_output_prefix + "_waveform.csv";
        recorder->save_waveform_csv(waveform_file);
        
        // Save eye diagram data
        std::string eye_file = m_output_prefix + "_eye.csv";
        recorder->save_eye_data_csv(eye_file, m_ui);
        
        // Save configuration
        std::string config_file = m_output_prefix + "_config.json";
        save_config(config_file);
    }
    
    void save_config(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open config file " << filename << std::endl;
            return;
        }
        
        file << std::fixed << std::setprecision(6);
        
        file << "{\n";
        
        // Wave parameters
        file << "  \"wave\": {\n";
        file << "    \"type\": \"PRBS31\"\n";
        file << "  },\n";
        
        // Channel parameters
        file << "  \"channel\": {\n";
        file << "    \"attenuation_db\": " << m_channel_params.attenuation_db << ",\n";
        file << "    \"bandwidth_hz\": " << m_channel_params.bandwidth_hz << "\n";
        file << "  },\n";
        
        // CTLE parameters
        file << "  \"ctle\": {\n";
        file << "    \"dc_gain\": " << m_ctle_params.dc_gain << ",\n";
        file << "    \"zeros\": [";
        for (size_t i = 0; i < m_ctle_params.zeros.size(); ++i) {
            file << m_ctle_params.zeros[i];
            if (i < m_ctle_params.zeros.size() - 1) file << ", ";
        }
        file << "],\n";
        file << "    \"poles\": [";
        for (size_t i = 0; i < m_ctle_params.poles.size(); ++i) {
            file << m_ctle_params.poles[i];
            if (i < m_ctle_params.poles.size() - 1) file << ", ";
        }
        file << "]\n";
        file << "  },\n";
        
        // VGA parameters
        file << "  \"vga\": {\n";
        file << "    \"dc_gain\": " << m_vga_params.dc_gain << "\n";
        file << "  },\n";
        
        // DFE parameters
        file << "  \"dfe\": {\n";
        file << "    \"taps_p\": [";
        for (size_t i = 0; i < m_dfe_params_p.taps.size(); ++i) {
            file << m_dfe_params_p.taps[i];
            if (i < m_dfe_params_p.taps.size() - 1) file << ", ";
        }
        file << "],\n";
        file << "    \"taps_n\": [";
        for (size_t i = 0; i < m_dfe_params_n.taps.size(); ++i) {
            file << m_dfe_params_n.taps[i];
            if (i < m_dfe_params_n.taps.size() - 1) file << ", ";
        }
        file << "]\n";
        file << "  },\n";
        
        // CDR parameters
        file << "  \"cdr\": {\n";
        file << "    \"kp\": " << m_cdr_params.pi.kp << ",\n";
        file << "    \"ki\": " << m_cdr_params.pi.ki << ",\n";
        file << "    \"ui\": " << m_cdr_params.ui << "\n";
        file << "  },\n";
        
        // Simulation parameters
        file << "  \"simulation\": {\n";
        file << "    \"duration_s\": " << m_sim_duration << ",\n";
        file << "    \"sample_rate_hz\": " << m_sample_rate << ",\n";
        file << "    \"ui_s\": " << m_ui << "\n";
        file << "  }\n";
        
        file << "}\n";
        
        file.close();
        std::cout << "Saved configuration to " << filename << std::endl;
    }
    
    // ========================================================================
    // Print Summary
    // ========================================================================
    
    void print_summary() {
        recorder->print_summary();
        
        // Print CDR status
        std::cout << "\nCDR Status:" << std::endl;
        std::cout << "  Final phase: " << cdr->get_raw_phase() * 1e12 << " ps" << std::endl;
        std::cout << "  Integral state: " << cdr->get_integral_state() << std::endl;
    }
};

// ============================================================================
// Main Function
// ============================================================================

int sc_main(int argc, char* argv[]) {
    // Disable SystemC deprecation warnings
    sc_core::sc_report_handler::set_actions(
        "/IEEE_Std_1666/deprecated",
        sc_core::SC_DO_NOTHING
    );
    
    // Parse command line
    TestScenario scenario = BASIC_LINK;
    
    if (argc > 1) {
        std::string arg = argv[1];
        auto it = scenario_map.find(arg);
        if (it != scenario_map.end()) {
            scenario = it->second;
        } else {
            std::cout << "Unknown scenario: " << arg << std::endl;
            std::cout << "Available scenarios: basic, ctle_sweep, cdr_lock, eye" << std::endl;
            return 1;
        }
    }
    
    std::cout << "=== RX Link Transient Testbench ===" << std::endl;
    std::cout << "Scenario: " << get_scenario_name(scenario) << std::endl;
    
    // Create and run testbench
    RxLinkTestbench tb("tb");
    tb.configure(scenario);
    tb.build();
    tb.run();
    tb.save_results();
    tb.print_summary();
    
    sc_core::sc_stop();
    
    std::cout << "\nTestbench completed successfully." << std::endl;
    return 0;
}
