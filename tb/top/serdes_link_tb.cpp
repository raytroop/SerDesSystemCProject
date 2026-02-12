/**
 * @file serdes_link_tb.cpp
 * @brief Full-link testbench with ClockGen integration and EyeAnalyzer output
 */

#include <systemc-ams>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "ams/serdes_link_top.h"
#include "ams/clock_generation.h"
#include "ams/diff_to_single.h"
#include "common/parameters.h"

using namespace serdes;

// Test Scenarios
enum TestScenario { BASIC_LINK = 0, EYE_DIAGRAM, S4P_CHANNEL, LONG_CHANNEL };

std::map<std::string, TestScenario> scenario_map = {
    {"basic", BASIC_LINK}, {"eye", EYE_DIAGRAM}, {"s4p", S4P_CHANNEL}, {"long_ch", LONG_CHANNEL}
};

std::string get_scenario_name(TestScenario s) {
    switch (s) {
        case BASIC_LINK: return "basic";
        case EYE_DIAGRAM: return "eye";
        case S4P_CHANNEL: return "s4p";
        case LONG_CHANNEL: return "long_ch";
        default: return "unknown";
    }
}

// EyeAnalyzer-compatible Signal Recorder
class EyeAnalyzerRecorder : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> time_in;
    sca_tdf::sca_in<double> voltage_in;
    
    EyeAnalyzerRecorder(sc_core::sc_module_name nm, const std::string& name)
        : sca_tdf::sca_module(nm), time_in("time_in"), voltage_in("voltage_in"), m_name(name) {}
    
    void set_attributes() override { set_timestep(2.0, sc_core::SC_PS); }  // 50 samples/UI with 2ps timestep
    
    void processing() override {
        m_time.push_back(time_in.read());
        m_voltage.push_back(voltage_in.read());
    }
    
    void save_csv(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) { std::cerr << "Error: Cannot open " << filename << std::endl; return; }
        file << std::scientific << std::setprecision(9);
        file << "time_s,voltage_v\n";
        for (size_t i = 0; i < m_time.size(); ++i) {
            file << m_time[i] << "," << m_voltage[i] << "\n";
        }
        file.close();
        std::cout << "Saved " << m_time.size() << " samples to " << filename << std::endl;
    }
    
    const std::vector<double>& get_time() const { return m_time; }
    const std::vector<double>& get_voltage() const { return m_voltage; }
    
private:
    std::string m_name;
    std::vector<double> m_time;
    std::vector<double> m_voltage;
};

// CDR Phase Monitor - Records CDR phase output over time
class CdrPhaseMonitor : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> time_in;
    sca_tdf::sca_in<double> phase_in;  // CDR phase output in seconds
    
    CdrPhaseMonitor(sc_core::sc_module_name nm, const std::string& name)
        : sca_tdf::sca_module(nm), time_in("time_in"), phase_in("phase_in"), m_name(name) {}
    
    void set_attributes() override { set_timestep(2.0, sc_core::SC_PS); }  // 50 samples/UI
    
    void processing() override {
        m_time.push_back(time_in.read());
        m_phase.push_back(phase_in.read());
    }
    
    void save_csv(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) { std::cerr << "Error: Cannot open " << filename << std::endl; return; }
        file << std::scientific << std::setprecision(12);
        file << "time_s,phase_s,phase_ps,phase_ui\n";
        for (size_t i = 0; i < m_time.size(); ++i) {
            double phase_s = m_phase[i];
            double phase_ps = phase_s * 1e12;
            double phase_ui = phase_s / 100e-12;  // Assuming 100ps UI
            file << m_time[i] << "," << phase_s << "," << phase_ps << "," << phase_ui << "\n";
        }
        file.close();
        std::cout << "Saved " << m_time.size() << " CDR phase samples to " << filename << std::endl;
    }
    
    const std::vector<double>& get_time() const { return m_time; }
    const std::vector<double>& get_phase() const { return m_phase; }
    
private:
    std::string m_name;
    std::vector<double> m_time;
    std::vector<double> m_phase;
};

// DFE Tap data recorder (simplified - records final values only)
struct DfeTapData {
    std::vector<double> time;
    std::vector<double> tap1, tap2, tap3, tap4, tap5;
    
    void add(double t, double t1, double t2, double t3, double t4, double t5) {
        time.push_back(t);
        tap1.push_back(t1);
        tap2.push_back(t2);
        tap3.push_back(t3);
        tap4.push_back(t4);
        tap5.push_back(t5);
    }
    
    void save_csv(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) { std::cerr << "Error: Cannot open " << filename << std::endl; return; }
        file << std::scientific << std::setprecision(12);
        file << "time_s,tap1,tap2,tap3,tap4,tap5\n";
        for (size_t i = 0; i < time.size(); ++i) {
            file << time[i] << "," << tap1[i] << "," << tap2[i] << ","
                 << tap3[i] << "," << tap4[i] << "," << tap5[i] << "\n";
        }
        file.close();
        std::cout << "Saved " << time.size() << " DFE tap samples to " << filename << std::endl;
    }
};

// Time Source Module
class TimeSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out;
    TimeSource(sc_core::sc_module_name nm) : sca_tdf::sca_module(nm), out("out") {}
    void set_attributes() override { set_timestep(2.0, sc_core::SC_PS); out.set_rate(1); }  // 50 samples/UI
    void processing() override { out.write(get_time().to_seconds()); }
};

// Constant VDD Source
class ConstVddSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out;
    ConstVddSource(sc_core::sc_module_name nm, double v = 1.0)
        : sca_tdf::sca_module(nm), out("out"), m_voltage(v) {}
    void set_attributes() override { set_timestep(1.5625, sc_core::SC_PS); }  // 64 samples/UI
    void processing() override { out.write(m_voltage); }
private:
    double m_voltage;
};

// SerDes Full Link Testbench
SC_MODULE(SerdesFullLinkTb) {
    ConstVddSource* vdd_src;
    ClockGenerationTdf* clk_gen;
    SerdesLinkTopModule* link;
    TimeSource* time_src;
    DiffToSingleTdf* d2s_tx;       // Differential to single for TX output
    DiffToSingleTdf* d2s_vga;      // Differential to single for VGA output (eye diagram)
    DiffToSingleTdf* d2s_sampler;  // Differential to single for Sampler input (DFE output)
    EyeAnalyzerRecorder* rec_tx_out;
    EyeAnalyzerRecorder* rec_channel_out;
    EyeAnalyzerRecorder* rec_vga_out;
    EyeAnalyzerRecorder* rec_sampler_in;  // Sampler-centric eye diagram
    CdrPhaseMonitor* rec_cdr_phase;
    DfeTapData dfe_tap_data;              // DFE tap coefficient data (recorded in summary)
    
    sca_tdf::sca_signal<double> sig_vdd;
    sca_tdf::sca_signal<double> sig_clk_phase;
    sca_tdf::sca_signal<double> sig_data_out;
    sca_tdf::sca_signal<double> sig_time;
    sca_tdf::sca_signal<double> sig_tx_out_p;   // TX output positive (analog, for eye analysis)
    sca_tdf::sca_signal<double> sig_tx_out_n;   // TX output negative (analog, for eye analysis)
    sca_tdf::sca_signal<double> sig_tx_diff;    // TX differential signal (Vp - Vn)
    sca_tdf::sca_signal<double> sig_vga_out_p;  // VGA output positive (for eye analysis)
    sca_tdf::sca_signal<double> sig_vga_out_n;  // VGA output negative (for eye analysis)
    sca_tdf::sca_signal<double> sig_vga_diff;   // VGA differential signal (Vp - Vn)
    sca_tdf::sca_signal<double> sig_cdr_phase;  // CDR phase output signal
    sca_tdf::sca_signal<double> sig_dfe_out_p;  // DFE output positive (Sampler input)
    sca_tdf::sca_signal<double> sig_dfe_out_n;  // DFE output negative (Sampler input)
    sca_tdf::sca_signal<double> sig_sampler_in; // Sampler input differential (DFE out)
    
    // DE signals for DFE tap monitoring (from Adaption)
    sc_core::sc_signal<double> sig_dfe_tap1;    // DFE tap 1
    sc_core::sc_signal<double> sig_dfe_tap2;    // DFE tap 2
    sc_core::sc_signal<double> sig_dfe_tap3;    // DFE tap 3
    sc_core::sc_signal<double> sig_dfe_tap4;    // DFE tap 4
    sc_core::sc_signal<double> sig_dfe_tap5;    // DFE tap 5
    
    SerdesLinkParams m_params;
    ClockParams m_clk_params;
    double m_sim_duration;
    double m_ui;
    double m_data_rate;
    std::string m_output_prefix;
    TestScenario m_scenario;
    
    SC_CTOR(SerdesFullLinkTb)
        : sig_vdd("sig_vdd"), sig_clk_phase("sig_clk_phase"),
          sig_data_out("sig_data_out"), sig_time("sig_time"),
          sig_tx_out_p("sig_tx_out_p"), sig_tx_out_n("sig_tx_out_n"),
          sig_tx_diff("sig_tx_diff"),
          sig_vga_out_p("sig_vga_out_p"), sig_vga_out_n("sig_vga_out_n"),
          sig_vga_diff("sig_vga_diff"),
          sig_cdr_phase("sig_cdr_phase"),
          sig_dfe_out_p("sig_dfe_out_p"), sig_dfe_out_n("sig_dfe_out_n"),
          sig_sampler_in("sig_sampler_in"),
          m_sim_duration(2000e-9), m_ui(100e-12), m_data_rate(10e9),
          m_output_prefix("serdes_link"), m_scenario(BASIC_LINK) {}
    
    void configure(TestScenario scenario) {
        m_scenario = scenario;
        configure_defaults();
        m_output_prefix = "serdes_link_" + get_scenario_name(scenario);
        switch (scenario) {
            case BASIC_LINK: configure_basic(); break;
            case EYE_DIAGRAM: configure_eye_diagram(); break;
            case S4P_CHANNEL: configure_s4p_channel(); break;
            case LONG_CHANNEL: configure_long_channel(); break;
            default: configure_basic(); break;
        }
    }
    
    void configure_defaults() {
        std::cout << "Configuring default parameters..." << std::endl;
        m_data_rate = 10e9;            // 10 Gbps
        m_ui = 1.0 / m_data_rate;      // 100 ps UI
        m_params.data_rate = m_data_rate;
        m_params.sample_rate = 500e9;  // 50x sampling (500 GHz) for 2ps timestep
        m_params.seed = 12345;
        
        // Clock: 10 GHz IDEAL
        m_clk_params.type = ClockType::IDEAL;
        m_clk_params.frequency = m_data_rate;
        
        // WaveGen: PRBS31
        m_params.wave = WaveGenParams();
        m_params.wave.type = PRBSType::PRBS31;
        
        // TX: Standard driver (no FFE, with bandwidth limit for proper eye)
        m_params.tx = TxParams();
        m_params.tx.ffe.taps = {1.0};  // Single tap, no pre-emphasis (FFE OFF)
        m_params.tx.mux_lane = 0;
        m_params.tx.driver.dc_gain = 0.8;
        m_params.tx.driver.vswing = 0.8;
        m_params.tx.driver.vcm_out = 0.6;
        m_params.tx.driver.sat_mode = "soft";
        m_params.tx.driver.vlin = 0.5;
        m_params.tx.driver.poles = {20e9};  // 20GHz bandwidth for visible transition
        
        // Channel: Simple model with moderate loss
        // For 10 Gbps, bandwidth should be > 10 GHz (ideally 2-3x fundamental freq)
        m_params.channel = ChannelParams();
        m_params.channel.touchstone = "";
        m_params.channel.ports = 2;
        m_params.channel.crosstalk = false;
        m_params.channel.bidirectional = false;
        m_params.channel.attenuation_db = 4.0;   // Reduced from 8dB
        m_params.channel.bandwidth_hz = 25e9;    // Increased from 15GHz
        
        // RX - Adjusted parameters for proper eye diagram
        // Key fix: CTLE needs zero < pole for high-frequency equalization
        // to compensate for channel high-frequency loss (ISI reduction)
        m_params.rx = RxParams();
        // CTLE: Two-pole equalizer with peak at 5GHz
        // H(s) = dc_gain * (1 + s/wz) / [(1 + s/wp1)(1 + s/wp2)]
        // Peak at 5GHz for 10Gbps Nyquist frequency
        m_params.rx.ctle.zeros = {0.8e9};        // 0.8 GHz zero
        m_params.rx.ctle.poles = {2.7e9, 10e9};  // 2.7 GHz and 10 GHz poles
        m_params.rx.ctle.dc_gain = 0.8;    // Lower DC gain
        m_params.rx.ctle.sat_min = -1.0;
        m_params.rx.ctle.sat_max = 1.0;
        // VGA: Variable gain amplifier with high-bandwidth pole
        m_params.rx.vga.zeros = {};        // No zeros - pure amplifier
        m_params.rx.vga.poles = {20e9};    // 20 GHz pole
        m_params.rx.vga.dc_gain = 1.2;     // Moderate gain
        m_params.rx.vga.sat_min = -1.0;
        m_params.rx.vga.sat_max = 1.0;
        m_params.rx.dfe_summer.tap_coeffs = {-0.05, -0.02, 0.01};
        m_params.rx.dfe_summer.ui = m_ui;
        m_params.rx.dfe_summer.enable = true;
        
        // CRITICAL FIX: hysteresis must be < resolution
        m_params.rx.sampler.phase_source = "phase";
        m_params.rx.sampler.threshold = 0.0;
        m_params.rx.sampler.hysteresis = 0.005;  // Must be < resolution
        m_params.rx.sampler.resolution = 0.02;
        
        m_params.rx.cdr.pi.kp = 0.01;
        m_params.rx.cdr.pi.ki = 1e-4;
        m_params.rx.cdr.pi.edge_threshold = 0.5;
        m_params.rx.cdr.pai.resolution = 1e-12;
        m_params.rx.cdr.pai.range = 5e-11;
        // Set CDR UI to match data UI for correct phase tracking
        m_params.rx.cdr.ui = m_ui;  // 100ps for 10Gbps
        
        // ENABLE all adaption
        m_params.adaption.agc.enabled = true;
        m_params.adaption.dfe.enabled = true;
        m_params.adaption.cdr_pi.enabled = true;
        m_params.adaption.threshold.enabled = true;
        
        // Configure adaption parameters
        m_params.adaption.agc.target_amplitude = 0.4;
        m_params.adaption.agc.kp = 0.1;
        m_params.adaption.agc.ki = 100.0;
        m_params.adaption.dfe.algorithm = "sign-lms";
        m_params.adaption.dfe.mu = 1e-4;
        m_params.adaption.cdr_pi.kp = 0.01;
        m_params.adaption.cdr_pi.ki = 1e-4;
        
        // CRITICAL FIX: Align update periods to integer UI for proper synchronization
        // Fast path (CDR PI + Threshold): update every 2 UI (200 ps @ 10Gbps)
        // This aligns with CDR's 2x oversampling (edge + data samples)
        m_params.adaption.fast_update_period = 2.0 * m_ui;  // 200 ps = 2 UI
        
        // Slow path (AGC + DFE): update every 2000 UI (200 ns @ 10Gbps)
        // Slower adaptation for stable convergence
        m_params.adaption.slow_update_period = 2000.0 * m_ui;  // 200 ns = 2000 UI
        
        // Ensure Adaption UI matches system UI
        m_params.adaption.UI = m_ui;
        
        std::cout << "  Data rate: " << m_data_rate/1e9 << " Gbps" << std::endl;
        std::cout << "  UI: " << m_ui*1e12 << " ps" << std::endl;
        std::cout << "  Sampler decision rate: " << (2.0 * m_data_rate)/1e9 << " GHz (2x oversampling)" << std::endl;
        std::cout << "  CDR UI: " << (m_ui/2.0)*1e12 << " ps (half UI for 2x sampling)" << std::endl;
        std::cout << "  Adaption: ENABLED (AGC, DFE, CDR_PI, Threshold)" << std::endl;
    }
    
    void configure_basic() {
        std::cout << "Scenario: BASIC_LINK" << std::endl;
        m_sim_duration = 2000e-9;
    }
    
    void configure_eye_diagram() {
        std::cout << "Scenario: EYE_DIAGRAM" << std::endl;
        m_sim_duration = 10000e-9;
    }
    
    void configure_s4p_channel() {
        std::cout << "Scenario: S4P_CHANNEL" << std::endl;
        m_params.channel.touchstone = "peters_01_0605_B12_thru.s4p";
        m_params.channel.ports = 4;
        m_params.channel.crosstalk = true;
        m_params.rx.ctle.dc_gain = 2.0;
        m_params.rx.vga.dc_gain = 3.0;
        m_sim_duration = 5000e-9;
    }
    
    void configure_long_channel() {
        std::cout << "Scenario: LONG_CHANNEL" << std::endl;
        m_params.channel.attenuation_db = 20.0;
        m_params.channel.bandwidth_hz = 10e9;
        m_params.tx.ffe.taps = {-0.1, 1.0, -0.3};
        m_params.rx.ctle.dc_gain = 3.0;
        m_params.rx.vga.dc_gain = 4.0;
        m_sim_duration = 5000e-9;
    }
    
    void build() {
        std::cout << "\nBuilding testbench..." << std::endl;
        
        vdd_src = new ConstVddSource("vdd_src", 1.0);
        clk_gen = new ClockGenerationTdf("clk_gen", m_clk_params);
        link = new SerdesLinkTopModule("link", m_params);
        time_src = new TimeSource("time_src");
        
        // Create differential-to-single converters
        d2s_tx = new DiffToSingleTdf("d2s_tx");
        d2s_vga = new DiffToSingleTdf("d2s_vga");
        d2s_sampler = new DiffToSingleTdf("d2s_sampler");  // For Sampler input
        
        rec_tx_out = new EyeAnalyzerRecorder("rec_tx_out", "tx_out");
        rec_channel_out = new EyeAnalyzerRecorder("rec_channel_out", "channel_out");
        rec_vga_out = new EyeAnalyzerRecorder("rec_vga_out", "vga_out");
        rec_sampler_in = new EyeAnalyzerRecorder("rec_sampler_in", "sampler_in");  // Sampler-centric
        rec_cdr_phase = new CdrPhaseMonitor("rec_cdr_phase", "cdr_phase");
        
        // Connect signals
        vdd_src->out(sig_vdd);
        link->vdd(sig_vdd);
        clk_gen->clk_phase(sig_clk_phase);
        link->data_out(sig_data_out);
        time_src->out(sig_time);
        
        // Connect monitoring ports (for analog eye diagram)
        link->mon_tx_out_p(sig_tx_out_p);
        link->mon_tx_out_n(sig_tx_out_n);
        // DFE output = Sampler input (for Sampler-centric eye diagram)
        link->mon_dfe_out_p(sig_dfe_out_p);
        link->mon_dfe_out_n(sig_dfe_out_n);
        // VGA output (before DFE, for traditional RX eye diagram)
        link->mon_vga_out_p(sig_vga_out_p);
        link->mon_vga_out_n(sig_vga_out_n);
        link->mon_cdr_phase(sig_cdr_phase);  // CDR phase output signal
        
        // Convert differential TX signals to single-ended
        d2s_tx->in_p(sig_tx_out_p);
        d2s_tx->in_n(sig_tx_out_n);
        d2s_tx->out(sig_tx_diff);
        
        // Convert differential VGA signals to single-ended
        d2s_vga->in_p(sig_vga_out_p);
        d2s_vga->in_n(sig_vga_out_n);
        d2s_vga->out(sig_vga_diff);
        
        // Convert differential DFE output (Sampler input) to single-ended
        d2s_sampler->in_p(sig_dfe_out_p);
        d2s_sampler->in_n(sig_dfe_out_n);
        d2s_sampler->out(sig_sampler_in);
        
        // Connect recorders - use DIFFERENTIAL signals for eye diagram
        rec_tx_out->time_in(sig_time);
        rec_tx_out->voltage_in(sig_tx_diff);            // TX differential output (Vp - Vn)
        rec_channel_out->time_in(sig_time);
        rec_channel_out->voltage_in(sig_tx_out_p);      // TX positive (for debugging)
        rec_vga_out->time_in(sig_time);
        rec_vga_out->voltage_in(sig_vga_diff);          // VGA differential output (Vp - Vn)
        rec_sampler_in->time_in(sig_time);
        rec_sampler_in->voltage_in(sig_sampler_in);     // Sampler input (DFE output) - Sampler-centric
        
        // Connect CDR phase monitor
        rec_cdr_phase->time_in(sig_time);
        rec_cdr_phase->phase_in(sig_cdr_phase);         // CDR phase output
        
        std::cout << "Testbench built successfully." << std::endl;
        std::cout << "  Note: VGA output = before DFE, Sampler input = after DFE" << std::endl;
    }
    
    // DFE tap coefficient sampling thread
    void dfe_tap_sampling_thread() {
        // Wait for initial transient to settle
        sc_core::wait(100.0, sc_core::SC_NS);
        
        // Sample DFE taps periodically during simulation
        // Use slow update period for sampling (200ns) to capture adaptation changes
        sc_core::sc_time sample_period(200.0, sc_core::SC_NS);
        
        while (sc_core::sc_time_stamp() < sc_core::sc_time(m_sim_duration, sc_core::SC_SEC)) {
            double t = sc_core::sc_time_stamp().to_seconds();
            double tap1 = link->get_dfe_tap(1);
            double tap2 = link->get_dfe_tap(2);
            double tap3 = link->get_dfe_tap(3);
            double tap4 = link->get_dfe_tap(4);
            double tap5 = link->get_dfe_tap(5);
            
            dfe_tap_data.add(t, tap1, tap2, tap3, tap4, tap5);
            
            sc_core::wait(sample_period);
        }
    }
    
    void run() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Running SerDes Link Simulation" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  Scenario: " << get_scenario_name(m_scenario) << std::endl;
        std::cout << "  Duration: " << m_sim_duration * 1e9 << " ns" << std::endl;
        std::cout << "  Data rate: " << m_data_rate / 1e9 << " Gbps" << std::endl;
        std::cout << "  UI: " << m_ui * 1e12 << " ps" << std::endl;
        std::cout << "  Channel: " << (m_params.channel.touchstone.empty() ? "simple" : "S4P") << std::endl;
        std::cout << "  Adaption: ENABLED (AGC, DFE, CDR_PI, Threshold)" << std::endl;
        std::cout << "========================================\n" << std::endl;
        
        // Register DFE tap sampling thread
        SC_THREAD(dfe_tap_sampling_thread);
        
        sc_core::sc_start(m_sim_duration, sc_core::SC_SEC);
        std::cout << "\nSimulation completed." << std::endl;
    }
    
    void save_results() {
        std::cout << "\nSaving results..." << std::endl;
        rec_tx_out->save_csv(m_output_prefix + "_tx.csv");
        rec_channel_out->save_csv(m_output_prefix + "_channel.csv");
        rec_vga_out->save_csv(m_output_prefix + "_vga.csv");
        rec_sampler_in->save_csv(m_output_prefix + "_sampler_in.csv");  // Sampler-centric
        rec_cdr_phase->save_csv(m_output_prefix + "_cdr_phase.csv");
        save_metadata_json(m_output_prefix + "_metadata.json");
    }
    
    void save_metadata_json(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) return;
        file << std::fixed << std::setprecision(12);
        file << "{\n";
        file << "  \"simulation\": {\n";
        file << "    \"scenario\": \"" << get_scenario_name(m_scenario) << "\",\n";
        file << "    \"duration_s\": " << m_sim_duration << ",\n";
        file << "    \"sample_rate_hz\": " << m_params.sample_rate << ",\n";
        file << "    \"ui_s\": " << m_ui << ",\n";
        file << "    \"data_rate_bps\": " << m_data_rate << "\n";
        file << "  },\n";
        file << "  \"clock\": {\"type\": \"IDEAL\", \"frequency_hz\": " << m_clk_params.frequency << "},\n";
        file << "  \"channel\": {\"type\": \"" << (m_params.channel.touchstone.empty() ? "simple" : "s4p") << "\"}\n";
        file << "}\n";
        file.close();
        std::cout << "Saved metadata to " << filename << std::endl;
    }
    
    void print_summary() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Simulation Summary" << std::endl;
        std::cout << "========================================" << std::endl;
        
        auto tx_v = rec_tx_out->get_voltage();
        if (!tx_v.empty()) {
            double tx_pp = *std::max_element(tx_v.begin(), tx_v.end()) - *std::min_element(tx_v.begin(), tx_v.end());
            std::cout << "  TX Peak-to-peak: " << tx_pp * 1000 << " mV" << std::endl;
        }
        
        // Check CDR lock status
        std::cout << "\n--- CDR Lock Status Check ---" << std::endl;
        double final_cdr_phase = link->get_cdr_phase();
        double final_cdr_integral = link->get_cdr_integral_state();
        double cdr_phase_ps = final_cdr_phase * 1e12;  // Convert to ps
        double phase_in_ui = final_cdr_phase / m_ui;
        
        std::cout << "  CDR Final Phase: " << cdr_phase_ps << " ps (" << phase_in_ui << " UI)" << std::endl;
        std::cout << "  CDR Integral State: " << final_cdr_integral << std::endl;
        
        // CDR lock detection criteria:
        // 1. Phase should be within reasonable range (typically ±0.5 UI)
        // 2. For a locked CDR, the phase should converge to a stable value
        bool phase_in_range = std::abs(phase_in_ui) < 0.5;
        bool phase_near_center = std::abs(phase_in_ui) < 0.3;  // Ideally near 0 or 0.5 UI
        
        std::cout << "\n  Lock Detection Analysis:" << std::endl;
        std::cout << "    Phase within ±0.5 UI range: " << (phase_in_range ? "YES ✓" : "NO ✗") << std::endl;
        std::cout << "    Phase near center (±0.3 UI): " << (phase_near_center ? "YES ✓" : "NO (may still be locked)") << std::endl;
        
        // Determine lock status
        bool cdr_locked = phase_in_range;
        std::cout << "\n  *** CDR LOCK STATUS: " << (cdr_locked ? "LOCKED ✓✓✓" : "NOT LOCKED ✗✗✗") << " ***" << std::endl;
        
        // DFE Tap values
        std::cout << "\n--- DFE Tap Coefficients (Final Values) ---" << std::endl;
        double dfe_tap1 = link->get_dfe_tap(1);
        double dfe_tap2 = link->get_dfe_tap(2);
        double dfe_tap3 = link->get_dfe_tap(3);
        double dfe_tap4 = link->get_dfe_tap(4);
        double dfe_tap5 = link->get_dfe_tap(5);
        std::cout << "  Tap 1: " << dfe_tap1 << std::endl;
        std::cout << "  Tap 2: " << dfe_tap2 << std::endl;
        std::cout << "  Tap 3: " << dfe_tap3 << std::endl;
        std::cout << "  Tap 4: " << dfe_tap4 << std::endl;
        std::cout << "  Tap 5: " << dfe_tap5 << std::endl;
        
        // Save DFE tap data (single point for now)
        dfe_tap_data.add(m_sim_duration, dfe_tap1, dfe_tap2, dfe_tap3, dfe_tap4, dfe_tap5);
        dfe_tap_data.save_csv(m_output_prefix + "_dfe_taps.csv");
        
        // Additional adaptive parameters status
        std::cout << "\n--- Adaptive Parameters Status ---" << std::endl;
        std::cout << "  AGC:    " << (m_params.adaption.agc.enabled ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "  DFE:    " << (m_params.adaption.dfe.enabled ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "  CDR_PI: " << (m_params.adaption.cdr_pi.enabled ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "  Threshold: " << (m_params.adaption.threshold.enabled ? "ENABLED" : "DISABLED") << std::endl;
        
        // Adaption update rate analysis
        std::cout << "\n--- Adaption Update Rate Analysis ---" << std::endl;
        double ui_ps = m_ui * 1e12;
        double fast_period_ps = m_params.adaption.fast_update_period * 1e12;
        double slow_period_ps = m_params.adaption.slow_update_period * 1e12;
        std::cout << "  UI: " << ui_ps << " ps" << std::endl;
        std::cout << "  Fast update period: " << fast_period_ps << " ps (" << fast_period_ps/ui_ps << " UI)" << std::endl;
        std::cout << "  Slow update period: " << slow_period_ps/1000 << " ns (" << slow_period_ps/ui_ps << " UI)" << std::endl;
        
        // Check alignment
        double fast_ui_ratio = fast_period_ps / ui_ps;
        double slow_ui_ratio = slow_period_ps / ui_ps;
        bool fast_aligned = std::abs(fast_ui_ratio - std::round(fast_ui_ratio)) < 0.01;
        bool slow_aligned = std::abs(slow_ui_ratio - std::round(slow_ui_ratio)) < 0.01;
        std::cout << "  Fast path UI-aligned: " << (fast_aligned ? "YES ✓" : "NO ⚠ (recommend fix)") << std::endl;
        std::cout << "  Slow path UI-aligned: " << (slow_aligned ? "YES ✓" : "NO ⚠ (recommend fix)") << std::endl;
        
        std::cout << "\nOutput Files:" << std::endl;
        std::cout << "  " << m_output_prefix << "_tx.csv" << std::endl;
        std::cout << "  " << m_output_prefix << "_channel.csv" << std::endl;
        std::cout << "  " << m_output_prefix << "_vga.csv (use this for RX eye diagram)" << std::endl;
        std::cout << "  " << m_output_prefix << "_cdr_phase.csv (CDR phase tracking)" << std::endl;
        std::cout << "  " << m_output_prefix << "_metadata.json" << std::endl;
        std::cout << "========================================" << std::endl;
    }
    
    ~SerdesFullLinkTb() {
        delete vdd_src; delete clk_gen; delete link; delete time_src;
        delete d2s_tx; delete d2s_vga; delete d2s_sampler;
        delete rec_tx_out; delete rec_channel_out; delete rec_vga_out; delete rec_sampler_in; delete rec_cdr_phase;
    }
};

// Main Function
int sc_main(int argc, char* argv[]) {
    sc_core::sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", sc_core::SC_DO_NOTHING);
    
    TestScenario scenario = BASIC_LINK;
    if (argc > 1) {
        auto it = scenario_map.find(argv[1]);
        if (it != scenario_map.end()) scenario = it->second;
        else {
            std::cout << "Unknown scenario. Available: basic, eye, s4p, long_ch" << std::endl;
            return 1;
        }
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "SerDes Full Link Testbench" << std::endl;
    std::cout << "========================================" << std::endl;
    
    SerdesFullLinkTb tb("tb");
    tb.configure(scenario);
    tb.build();
    tb.run();
    tb.save_results();
    tb.print_summary();
    
    sc_core::sc_stop();
    std::cout << "\nTestbench completed successfully." << std::endl;
    return 0;
}
