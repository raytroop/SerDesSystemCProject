/**
 * @file serdes_link_tb.cpp
 * @brief System-level testbench for complete SerDes link
 * 
 * This testbench performs complete SerDes link simulation using
 * SerdesLinkTopModule which encapsulates:
 *   WaveGen -> TX -> Channel -> RX
 * 
 * Key features:
 * - Uses SerdesLinkTopModule for complete link simulation
 * - Multi-point signal recording for analysis
 * - Multiple test scenarios via command line
 * - CSV export for waveform and eye diagram data
 * 
 * Test scenarios:
 * 0. BASIC_LINK     - Basic link verification (short simulation)
 * 1. CHANNEL_SWEEP  - Test different channel configurations
 * 2. EYE_DIAGRAM    - Eye diagram data collection (long simulation)
 * 3. BER_TEST       - Bit error rate measurement
 * 
 * Usage:
 *   ./serdes_link_tb [scenario]
 * 
 * Examples:
 *   ./serdes_link_tb basic
 *   ./serdes_link_tb eye
 *   ./serdes_link_tb ber
 */

#include <systemc-ams>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <iomanip>

// Project headers
#include "ams/serdes_link_top.h"
#include "common/parameters.h"

// Local helpers
#include "serdes_link_helpers.h"

using namespace serdes;

// ============================================================================
// Test Scenarios
// ============================================================================

enum TestScenario {
    BASIC_LINK = 0,
    CHANNEL_SWEEP,
    EYE_DIAGRAM,
    BER_TEST
};

std::map<std::string, TestScenario> scenario_map = {
    {"basic", BASIC_LINK},
    {"channel", CHANNEL_SWEEP},
    {"eye", EYE_DIAGRAM},
    {"ber", BER_TEST},
    {"0", BASIC_LINK},
    {"1", CHANNEL_SWEEP},
    {"2", EYE_DIAGRAM},
    {"3", BER_TEST}
};

std::string get_scenario_name(TestScenario scenario) {
    switch (scenario) {
        case BASIC_LINK: return "basic";
        case CHANNEL_SWEEP: return "channel";
        case EYE_DIAGRAM: return "eye";
        case BER_TEST: return "ber";
        default: return "unknown";
    }
}

// ============================================================================
// SerDes Link Testbench
// ============================================================================

SC_MODULE(SerdesLinkTestbench) {
    // ========================================================================
    // Sub-modules
    // ========================================================================
    
    // VDD source
    ConstVddSource* vdd_src;
    
    // SerDes Link Top Module (encapsulates entire link)
    SerdesLinkTopModule* link;
    
    // Signal recorder
    MultiPointSignalRecorder* recorder;
    
    // ========================================================================
    // Signals
    // ========================================================================
    
    // VDD signal
    sca_tdf::sca_signal<double> sig_vdd;
    
    // Data output
    sca_tdf::sca_signal<double> sig_data_out;
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    SerdesLinkParams m_params;
    double m_sim_duration;
    double m_ui;
    std::string m_output_prefix;
    TestScenario m_scenario;
    
    // ========================================================================
    // Constructor
    // ========================================================================
    
    SC_CTOR(SerdesLinkTestbench)
        : sig_vdd("sig_vdd")
        , sig_data_out("sig_data_out")
        , m_sim_duration(2000e-9)
        , m_ui(100e-12)
        , m_output_prefix("serdes_link")
        , m_scenario(BASIC_LINK)
    {
    }
    
    // ========================================================================
    // Configuration Methods
    // ========================================================================
    
    void configure(TestScenario scenario) {
        m_scenario = scenario;
        
        // Default parameters
        configure_defaults();
        
        std::string scenario_name = get_scenario_name(scenario);
        m_output_prefix = "serdes_link_" + scenario_name;
        
        switch (scenario) {
            case BASIC_LINK:
                configure_basic();
                break;
            case CHANNEL_SWEEP:
                configure_channel_sweep();
                break;
            case EYE_DIAGRAM:
                configure_eye_diagram();
                break;
            case BER_TEST:
                configure_ber_test();
                break;
            default:
                configure_basic();
                break;
        }
    }
    
    void configure_defaults() {
        // Global timing parameters
        m_params.sample_rate = 100e9;  // 100 GS/s
        m_params.seed = 12345;
        m_ui = 100e-12;  // 100 ps (10 Gbps)
        
        // Wave generation
        m_params.wave = WaveGenParams();
        m_params.wave.type = PRBSType::PRBS31;
        m_params.wave.single_pulse = 0.0;
        
        // TX parameters
        m_params.tx = TxParams();
        m_params.tx.ffe.taps = {0.0, 1.0, -0.25};  // De-emphasis
        m_params.tx.mux_lane = 0;
        m_params.tx.driver.dc_gain = 0.8;
        m_params.tx.driver.vswing = 0.8;
        m_params.tx.driver.vcm_out = 0.6;
        m_params.tx.driver.sat_mode = "soft";
        m_params.tx.driver.vlin = 0.5;
        m_params.tx.driver.poles = {50e9};
        
        // Channel parameters (simple model)
        m_params.channel = ChannelParams();
        m_params.channel.attenuation_db = 8.0;
        m_params.channel.bandwidth_hz = 15e9;
        
        // RX parameters
        m_params.rx = RxParams();
        
        // CTLE
        m_params.rx.ctle.zeros = {2e9};
        m_params.rx.ctle.poles = {30e9};
        m_params.rx.ctle.dc_gain = 1.5;
        m_params.rx.ctle.vcm_out = 0.0;
        
        // VGA
        m_params.rx.vga.zeros = {1e9};
        m_params.rx.vga.poles = {20e9};
        m_params.rx.vga.dc_gain = 2.0;
        m_params.rx.vga.vcm_out = 0.0;
        
        // DFE
        m_params.rx.dfe.taps = {-0.05, -0.02, 0.01};
        
        // Sampler (phase-driven mode is forced by RxTopModule)
        m_params.rx.sampler.phase_source = "phase";
        m_params.rx.sampler.threshold = 0.0;
        m_params.rx.sampler.hysteresis = 0.02;
        m_params.rx.sampler.resolution = 0.02;
        
        // CDR
        m_params.rx.cdr.pi.kp = 0.01;
        m_params.rx.cdr.pi.ki = 1e-4;
        m_params.rx.cdr.pi.edge_threshold = 0.5;
        m_params.rx.cdr.pai.resolution = 1e-12;
        m_params.rx.cdr.pai.range = 5e-11;
        m_params.rx.cdr.ui = m_ui;
    }
    
    void configure_basic() {
        std::cout << "Configuring BASIC_LINK test..." << std::endl;
        m_sim_duration = 2000e-9;  // 2 us
    }
    
    void configure_channel_sweep() {
        std::cout << "Configuring CHANNEL_SWEEP test..." << std::endl;
        // Higher channel loss
        m_params.channel.attenuation_db = 12.0;
        m_params.channel.bandwidth_hz = 12e9;
        
        // Increase CTLE gain to compensate
        m_params.rx.ctle.dc_gain = 2.0;
        
        m_sim_duration = 3000e-9;  // 3 us
    }
    
    void configure_eye_diagram() {
        std::cout << "Configuring EYE_DIAGRAM test..." << std::endl;
        // Longer simulation for good eye statistics
        m_sim_duration = 10000e-9;  // 10 us
    }
    
    void configure_ber_test() {
        std::cout << "Configuring BER_TEST..." << std::endl;
        // Longer simulation for BER measurement
        m_sim_duration = 20000e-9;  // 20 us
        
        // Stronger CDR for better tracking
        m_params.rx.cdr.pi.kp = 0.02;
        m_params.rx.cdr.pi.ki = 2e-4;
    }
    
    // ========================================================================
    // Build Method
    // ========================================================================
    
    void build() {
        std::cout << "Building SerDes link testbench..." << std::endl;
        
        // Create VDD source
        vdd_src = new ConstVddSource("vdd_src", 1.0);
        
        // Create SerDes Link Top Module
        link = new SerdesLinkTopModule("link", m_params);
        
        // Create Signal Recorder
        recorder = new MultiPointSignalRecorder("recorder");
        
        // ====================================================================
        // Connect Signals
        // ====================================================================
        
        // VDD source
        vdd_src->out(sig_vdd);
        
        // Link connections
        link->vdd(sig_vdd);
        link->data_out(sig_data_out);
        
        // Recorder connections - using Link debug signals
        recorder->tx_out_p(link->get_tx_out_p_signal());
        recorder->tx_out_n(link->get_tx_out_n_signal());
        recorder->channel_out(link->get_channel_out_signal());
        recorder->rx_in_p(link->get_rx_in_p_signal());
        recorder->rx_in_n(link->get_rx_in_n_signal());
        recorder->ctle_out_p(link->get_rx_ctle_out_p_signal());
        recorder->ctle_out_n(link->get_rx_ctle_out_n_signal());
        recorder->vga_out_p(link->get_rx_vga_out_p_signal());
        recorder->vga_out_n(link->get_rx_vga_out_n_signal());
        recorder->dfe_out_p(link->get_rx_dfe_out_p_signal());
        recorder->dfe_out_n(link->get_rx_dfe_out_n_signal());
        recorder->data_out(sig_data_out);
        
        std::cout << "SerDes link testbench built successfully." << std::endl;
    }
    
    // ========================================================================
    // Run Method
    // ========================================================================
    
    void run() {
        std::cout << "\nRunning SerDes link simulation..." << std::endl;
        std::cout << "  Simulation time: " << m_sim_duration * 1e9 << " ns" << std::endl;
        std::cout << "  Sample rate: " << m_params.sample_rate / 1e9 << " GS/s" << std::endl;
        std::cout << "  Data rate: " << 1.0 / m_ui / 1e9 << " Gbps" << std::endl;
        std::cout << "  UI: " << m_ui * 1e12 << " ps" << std::endl;
        std::cout << "  Channel attenuation: " << m_params.channel.attenuation_db << " dB" << std::endl;
        std::cout << "  Channel bandwidth: " << m_params.channel.bandwidth_hz / 1e9 << " GHz" << std::endl;
        
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
        
        // Simulation parameters
        file << "  \"simulation\": {\n";
        file << "    \"scenario\": \"" << get_scenario_name(m_scenario) << "\",\n";
        file << "    \"duration_s\": " << m_sim_duration << ",\n";
        file << "    \"sample_rate_hz\": " << m_params.sample_rate << ",\n";
        file << "    \"ui_s\": " << m_ui << ",\n";
        file << "    \"seed\": " << m_params.seed << "\n";
        file << "  },\n";
        
        // Wave parameters
        file << "  \"wave\": {\n";
        file << "    \"type\": \"PRBS31\",\n";
        file << "    \"single_pulse\": " << m_params.wave.single_pulse << "\n";
        file << "  },\n";
        
        // TX parameters
        file << "  \"tx\": {\n";
        file << "    \"ffe_taps\": [";
        for (size_t i = 0; i < m_params.tx.ffe.taps.size(); ++i) {
            file << m_params.tx.ffe.taps[i];
            if (i < m_params.tx.ffe.taps.size() - 1) file << ", ";
        }
        file << "],\n";
        file << "    \"driver_dc_gain\": " << m_params.tx.driver.dc_gain << ",\n";
        file << "    \"driver_vswing\": " << m_params.tx.driver.vswing << "\n";
        file << "  },\n";
        
        // Channel parameters
        file << "  \"channel\": {\n";
        file << "    \"attenuation_db\": " << m_params.channel.attenuation_db << ",\n";
        file << "    \"bandwidth_hz\": " << m_params.channel.bandwidth_hz << "\n";
        file << "  },\n";
        
        // RX parameters
        file << "  \"rx\": {\n";
        file << "    \"ctle_dc_gain\": " << m_params.rx.ctle.dc_gain << ",\n";
        file << "    \"vga_dc_gain\": " << m_params.rx.vga.dc_gain << ",\n";
        file << "    \"dfe_taps\": [";
        for (size_t i = 0; i < m_params.rx.dfe.taps.size(); ++i) {
            file << m_params.rx.dfe.taps[i];
            if (i < m_params.rx.dfe.taps.size() - 1) file << ", ";
        }
        file << "],\n";
        file << "    \"cdr_kp\": " << m_params.rx.cdr.pi.kp << ",\n";
        file << "    \"cdr_ki\": " << m_params.rx.cdr.pi.ki << "\n";
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
        std::cout << "  Final phase: " << link->get_cdr_phase() * 1e12 << " ps" << std::endl;
        std::cout << "  Integral state: " << link->get_cdr_integral_state() << std::endl;
    }
    
    // ========================================================================
    // Destructor
    // ========================================================================
    
    ~SerdesLinkTestbench() {
        delete vdd_src;
        delete link;
        delete recorder;
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
            std::cout << "Available scenarios:" << std::endl;
            std::cout << "  basic   (0) - Basic link verification" << std::endl;
            std::cout << "  channel (1) - Channel parameter sweep" << std::endl;
            std::cout << "  eye     (2) - Eye diagram data collection" << std::endl;
            std::cout << "  ber     (3) - BER measurement" << std::endl;
            return 1;
        }
    }
    
    std::cout << "=== SerDes Link System Testbench ===" << std::endl;
    std::cout << "Scenario: " << get_scenario_name(scenario) << std::endl;
    
    // Create and run testbench
    SerdesLinkTestbench tb("tb");
    tb.configure(scenario);
    tb.build();
    tb.run();
    tb.save_results();
    tb.print_summary();
    
    sc_core::sc_stop();
    
    std::cout << "\n=== Testbench completed successfully ===" << std::endl;
    return 0;
}
