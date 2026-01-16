#include <gtest/gtest.h>
#include <systemc-ams>
#include <cmath>
#include "ams/rx_sampler.h"
#include "common/parameters.h"

using namespace serdes;

// Differential signal source module for testing
class DifferentialSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out_p;
    sca_tdf::sca_out<double> out_n;
    
    double m_amplitude;
    double m_vcm;
    
    DifferentialSource(sc_core::sc_module_name nm, double amplitude, double vcm)
        : sca_tdf::sca_module(nm)
        , out_p("out_p")
        , out_n("out_n")
        , m_amplitude(amplitude)
        , m_vcm(vcm)
    {}
    
    void set_attributes() {
        out_p.set_rate(1);
        out_n.set_rate(1);
        out_p.set_timestep(1.0 / 100e9, sc_core::SC_SEC);
        out_n.set_timestep(1.0 / 100e9, sc_core::SC_SEC);
    }
    
    void processing() {
        out_p.write(m_vcm + 0.5 * m_amplitude);
        out_n.write(m_vcm - 0.5 * m_amplitude);
    }
};

// Clock source module for testing
class ClockSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> clk_out;
    
    double m_frequency;
    double m_amplitude;
    double m_vcm;
    int m_step_count;
    
    ClockSource(sc_core::sc_module_name nm, double frequency, double amplitude, double vcm)
        : sca_tdf::sca_module(nm)
        , clk_out("clk_out")
        , m_frequency(frequency)
        , m_amplitude(amplitude)
        , m_vcm(vcm)
        , m_step_count(0)
    {}
    
    void set_attributes() {
        clk_out.set_rate(1);
        clk_out.set_timestep(1.0 / 100e9, sc_core::SC_SEC);
    }
    
    void processing() {
        double t = m_step_count * 1.0 / 100e9;
        double clk = m_vcm + 0.5 * m_amplitude * sin(2.0 * M_PI * m_frequency * t);
        clk_out.write(clk);
        m_step_count++;
    }
};

// Testbench for sampler basic functionality
SC_MODULE(SamplerBasicTestbench) {
    DifferentialSource* src;
    ClockSource* clk_src;
    RxSamplerTdf* sampler;
    
    sca_tdf::sca_signal<double> sig_in_p;
    sca_tdf::sca_signal<double> sig_in_n;
    sca_tdf::sca_signal<double> sig_clk;
    sca_tdf::sca_signal<double> sig_phase;
    sca_tdf::sca_signal<double> sig_out;
    sc_core::sc_signal<bool> sig_out_de;
    
    RxSamplerParams params;
    double input_amplitude;
    
    SamplerBasicTestbench(sc_core::sc_module_name nm, const RxSamplerParams& p, double amp)
        : sc_core::sc_module(nm)
        , params(p)
        , input_amplitude(amp)
    {
        // Create modules
        src = new DifferentialSource("src", input_amplitude, 0.6);
        clk_src = new ClockSource("clk_src", 10e9, 1.0, 0.5);
        sampler = new RxSamplerTdf("sampler", params);
        
        // Connect signals
        src->out_p(sig_in_p);
        src->out_n(sig_in_n);
        clk_src->clk_out(sig_clk);
        
        sampler->in_p(sig_in_p);
        sampler->in_n(sig_in_n);
        sampler->clk_sample(sig_clk);
        sampler->phase_offset(sig_phase);
        sampler->data_out(sig_out);
        sampler->data_out_de(sig_out_de);
    }
    
    ~SamplerBasicTestbench() {
        delete src;
        delete clk_src;
        delete sampler;
    }
    
    double get_output() {
        return sig_out.read(0);
    }
    
    bool get_output_de() {
        return sig_out_de.read();
    }
};

// Test case 1: Basic functionality test
TEST(SamplerBasicTest, BasicDecision) {
    // Configure parameters
    RxSamplerParams params;
    params.resolution = 0.1;  // Larger resolution for clear decision
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Create testbench with positive input
    double input_amp = 0.2;  // 200mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Positive input should result in 1.0 output
    double output = tb->get_output();
    EXPECT_NEAR(output, 1.0, 0.1) << "Positive input should result in 1.0 output";
    
    delete tb;
}

// Test case 2: Negative input test
TEST(SamplerBasicTest, NegativeInputDecision) {
    // Configure parameters
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Create testbench with negative input (by using negative amplitude)
    double input_amp = -0.2;  // -200mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Negative input should result in 0.0 output
    double output = tb->get_output();
    EXPECT_NEAR(output, 0.0, 0.1) << "Negative input should result in 0.0 output";
    
    delete tb;
}

// Test case 3: Hysteresis behavior test
TEST(SamplerBasicTest, HysteresisBehavior) {
    // Configure parameters with hysteresis
    RxSamplerParams params;
    params.threshold = 0.0;
    params.hysteresis = 0.05;
    params.resolution = 0.1;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Create testbench with small positive input
    double input_amp = 0.03;  // 30mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Small positive input should result in previous state (0.0)
    double output1 = tb->get_output();
    EXPECT_NEAR(output1, 0.0, 0.1) << "Small positive input should retain previous state (0.0)";
    
    delete tb;
}

// Test case 4: Parameter validation test
TEST(SamplerBasicTest, ParameterValidation) {
    // Configure invalid parameters (hysteresis >= resolution)
    RxSamplerParams params;
    params.hysteresis = 0.1;
    params.resolution = 0.05;  // Invalid: hysteresis > resolution
    
    // Test: Construction should throw exception
    EXPECT_THROW(
        RxSamplerTdf* sampler = new RxSamplerTdf("sampler", params),
        std::invalid_argument
    ) << "Invalid parameters should throw exception";
}

// Test case 5: Noise effect test
TEST(SamplerBasicTest, NoiseEffect) {
    // Configure parameters with noise enabled
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = true;
    params.noise_sigma = 0.05;
    params.noise_seed = 12345;
    
    // Create testbench with input near threshold
    double input_amp = 0.05;  // 50mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Output should be either 0.0 or 1.0 due to noise
    double output = tb->get_output();
    EXPECT_TRUE(output == 0.0 || output == 1.0) << "Output should be digital (0.0 or 1.0)";
    
    delete tb;
}

// Test case 6: Offset effect test
TEST(SamplerBasicTest, OffsetEffect) {
    // Configure parameters with offset enabled
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = true;
    params.offset_value = 0.15;  // 150mV offset
    params.noise_enable = false;
    
    // Create testbench with zero input
    double input_amp = 0.0;  // 0mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Zero input with positive offset should result in 1.0 output
    double output = tb->get_output();
    EXPECT_NEAR(output, 1.0, 0.1) << "Zero input with positive offset should result in 1.0 output";
    
    delete tb;
}

// Test case 7: Fuzzy decision test
TEST(SamplerBasicTest, FuzzyDecision) {
    // Configure parameters for fuzzy decision
    RxSamplerParams params;
    params.threshold = 0.0;
    params.resolution = 0.1;  // 100mV resolution region
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Create testbench with input in fuzzy region
    double input_amp = 0.05;  // 50mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Output should be either 0.0 or 1.0 (random in fuzzy region)
    double output = tb->get_output();
    EXPECT_TRUE(output == 0.0 || output == 1.0) << "Output should be digital (0.0 or 1.0)";
    
    delete tb;
}

// Test case 8: Phase source parameter validation
TEST(SamplerBasicTest, PhaseSourceValidation) {
    // Configure invalid phase source
    RxSamplerParams params;
    params.phase_source = "invalid";
    
    // Test: Construction should throw exception
    EXPECT_THROW(
        RxSamplerTdf* sampler = new RxSamplerTdf("sampler", params),
        std::invalid_argument
    ) << "Invalid phase source should throw exception";
}

// Test case 9: Valid phase source test
TEST(SamplerBasicTest, ValidPhaseSource) {
    // Test valid phase sources
    std::vector<std::string> valid_sources = {"clock", "phase"};
    
    for (const std::string& source : valid_sources) {
        RxSamplerParams params;
        params.phase_source = source;
        
        // Test: Construction should succeed
        EXPECT_NO_THROW(
            RxSamplerTdf* sampler = new RxSamplerTdf("sampler", params);
            delete sampler;
        ) << "Valid phase source '" << source << "' should not throw exception";
    }
}

// Test case 10: Output range test
TEST(SamplerBasicTest, OutputRange) {
    // Configure parameters
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Test with various input amplitudes
    double test_amplitudes[] = {-0.5, -0.1, 0.0, 0.1, 0.5};
    
    for (double amp : test_amplitudes) {
        SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, amp);
        
        sc_core::sc_start(10, sc_core::SC_NS);
        
        double output = tb->get_output();
        
        // Test: Output should be either 0.0 or 1.0
        EXPECT_TRUE(output == 0.0 || output == 1.0) 
            << "Output should be digital (0.0 or 1.0) for input amplitude " << amp;
        
        delete tb;
    }
}

// Test case 11: DE domain output test
TEST(SamplerBasicTest, DEOutputVerification) {
    // Configure parameters
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Test with positive input
    double input_amp = 0.2;  // 200mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: Both TDF and DE outputs should match
    double tdf_output = tb->get_output();
    bool de_output = tb->get_output_de();
    
    // Convert TDF output to bool for comparison
    bool tdf_output_bool = (tdf_output != 0.0);
    
    EXPECT_EQ(tdf_output_bool, de_output) 
        << "TDF and DE outputs should match: TDF=" << tdf_output_bool << ", DE=" << de_output;
    
    // Test: DE output should be boolean (true for positive input)
    EXPECT_TRUE(de_output) << "DE output should be true for positive input";
    
    delete tb;
}

// Test case 12: Negative input DE output test
TEST(SamplerBasicTest, NegativeInputDEOutput) {
    // Configure parameters
    RxSamplerParams params;
    params.resolution = 0.1;
    params.hysteresis = 0.02;
    params.offset_enable = false;
    params.noise_enable = false;
    
    // Test with negative input
    double input_amp = -0.2;  // -200mV differential input
    SamplerBasicTestbench* tb = new SamplerBasicTestbench("tb", params, input_amp);
    
    // Run simulation
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // Test: DE output should be false for negative input
    bool de_output = tb->get_output_de();
    
    EXPECT_FALSE(de_output) << "DE output should be false for negative input";
    
    delete tb;
}
