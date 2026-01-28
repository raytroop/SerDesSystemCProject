/**
 * @file test_dfe_basic.cpp
 * @brief Unit tests for RxDfeTdf - Basic functionality tests
 */

#include <gtest/gtest.h>
#include <systemc-ams>
#include <cmath>
#include "ams/rx_dfe.h"
#include "common/parameters.h"

using namespace serdes;

// ============================================================================
// Helper Modules
// ============================================================================

/**
 * @brief Single-ended signal source for DFE testing
 */
class SingleEndedSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out;
    
    double m_value;
    
    SingleEndedSource(sc_core::sc_module_name nm, double value = 0.0)
        : sca_tdf::sca_module(nm)
        , out("out")
        , m_value(value)
    {}
    
    void set_value(double v) { m_value = v; }
    
    void set_attributes() override {
        out.set_rate(1);
        out.set_timestep(1.0 / 100e9, sc_core::SC_SEC);  // 10ps timestep
    }
    
    void processing() override {
        out.write(m_value);
    }
};

/**
 * @brief Single-ended signal sink for capturing DFE output
 */
class SingleEndedSink : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> in;
    
    std::vector<double> samples;
    
    SingleEndedSink(sc_core::sc_module_name nm)
        : sca_tdf::sca_module(nm)
        , in("in")
    {}
    
    void set_attributes() override {
        in.set_rate(1);
        in.set_timestep(1.0 / 100e9, sc_core::SC_SEC);
    }
    
    void processing() override {
        samples.push_back(in.read());
    }
    
    double get_last() const {
        return samples.empty() ? 0.0 : samples.back();
    }
    
    void clear() { samples.clear(); }
};

/**
 * @brief Sequence source that outputs a predefined sequence of values
 */
class SequenceSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out;
    
    std::vector<double> m_sequence;
    size_t m_index;
    
    SequenceSource(sc_core::sc_module_name nm, const std::vector<double>& seq)
        : sca_tdf::sca_module(nm)
        , out("out")
        , m_sequence(seq)
        , m_index(0)
    {}
    
    void set_attributes() override {
        out.set_rate(1);
        out.set_timestep(1.0 / 100e9, sc_core::SC_SEC);
    }
    
    void processing() override {
        if (m_index < m_sequence.size()) {
            out.write(m_sequence[m_index]);
            m_index++;
        } else {
            out.write(0.0);
        }
    }
};

// ============================================================================
// Test Fixtures
// ============================================================================

/**
 * @brief Basic DFE testbench with constant input
 */
SC_MODULE(DfeBasicTestbench) {
    SingleEndedSource* src;
    RxDfeTdf* dfe;
    SingleEndedSink* sink;
    
    sca_tdf::sca_signal<double> sig_in;
    sca_tdf::sca_signal<double> sig_out;
    
    RxDfeParams params;
    
    DfeBasicTestbench(sc_core::sc_module_name nm, const RxDfeParams& p, double input_val = 0.0)
        : sc_core::sc_module(nm)
        , params(p)
    {
        src = new SingleEndedSource("src", input_val);
        dfe = new RxDfeTdf("dfe", params);
        sink = new SingleEndedSink("sink");
        
        // Connect signals
        src->out(sig_in);
        dfe->in(sig_in);
        dfe->out(sig_out);
        sink->in(sig_out);
    }
    
    ~DfeBasicTestbench() {
        // SystemC modules managed by simulator
    }
    
    double get_output() const {
        return sink->get_last();
    }
    
    const std::vector<double>& get_samples() const {
        return sink->samples;
    }
};

/**
 * @brief DFE testbench with sequence input
 */
SC_MODULE(DfeSequenceTestbench) {
    SequenceSource* src;
    RxDfeTdf* dfe;
    SingleEndedSink* sink;
    
    sca_tdf::sca_signal<double> sig_in;
    sca_tdf::sca_signal<double> sig_out;
    
    RxDfeParams params;
    
    DfeSequenceTestbench(sc_core::sc_module_name nm, const RxDfeParams& p, 
                         const std::vector<double>& input_seq)
        : sc_core::sc_module(nm)
        , params(p)
    {
        src = new SequenceSource("src", input_seq);
        dfe = new RxDfeTdf("dfe", params);
        sink = new SingleEndedSink("sink");
        
        // Connect signals
        src->out(sig_in);
        dfe->in(sig_in);
        dfe->out(sig_out);
        sink->in(sig_out);
    }
    
    ~DfeSequenceTestbench() {
        // SystemC modules managed by simulator
    }
    
    const std::vector<double>& get_outputs() const {
        return sink->samples;
    }
};

// ============================================================================
// Test Cases
// ============================================================================

/**
 * @brief Test default parameter construction
 */
TEST(RxDfeBasicTest, DefaultConstruction) {
    RxDfeParams params;
    
    // Verify default values from parameters.h
    EXPECT_EQ(params.taps.size(), 3u);
    EXPECT_NEAR(params.taps[0], -0.05, 1e-10);
    EXPECT_NEAR(params.taps[1], -0.02, 1e-10);
    EXPECT_NEAR(params.taps[2], 0.01, 1e-10);
    EXPECT_EQ(params.update, "sign-lms");
    EXPECT_NEAR(params.mu, 1e-4, 1e-15);
}

/**
 * @brief Test port connection (instantiation without simulation)
 */
TEST(RxDfeBasicTest, PortConnection) {
    RxDfeParams params;
    DfeBasicTestbench* tb = new DfeBasicTestbench("tb", params, 0.0);
    
    // If we reach here without exception, port connection succeeded
    SUCCEED() << "DFE port connection test passed";
    
    // Note: SystemC modules managed by simulator, no manual delete
}

/**
 * @brief Test zero input produces zero output (with zero history)
 */
TEST(RxDfeBasicTest, ZeroInputZeroOutput) {
    RxDfeParams params;
    params.taps = {-0.1, -0.05, 0.02};  // 3 taps
    
    DfeBasicTestbench* tb = new DfeBasicTestbench("tb", params, 0.0);
    
    // Run short simulation
    sc_core::sc_start(100, sc_core::SC_PS);  // 10 samples
    
    // With zero input and zero history, output should be zero
    const auto& samples = tb->get_samples();
    ASSERT_GT(samples.size(), 0u);
    
    for (const auto& s : samples) {
        EXPECT_NEAR(s, 0.0, 1e-12) << "Zero input should produce zero output";
    }
    
    sc_core::sc_stop();
}

/**
 * @brief Test constant non-zero input
 * 
 * With constant input x and DFE feedback:
 * y[n] = x - sum(taps[i] * y[n-1-i])
 * 
 * At steady state with constant x:
 * y_ss = x - sum(taps[i]) * y_ss
 * y_ss * (1 + sum(taps)) = x
 * y_ss = x / (1 + sum(taps))
 */
TEST(RxDfeBasicTest, ConstantInputSteadyState) {
    RxDfeParams params;
    params.taps = {-0.1, -0.05};  // 2 taps, sum = -0.15
    
    double input_val = 0.5;
    DfeBasicTestbench* tb = new DfeBasicTestbench("tb", params, input_val);
    
    // Run enough time for steady state
    sc_core::sc_start(500, sc_core::SC_PS);  // 50 samples
    
    const auto& samples = tb->get_samples();
    ASSERT_GT(samples.size(), 20u);
    
    // Calculate expected steady state
    double tap_sum = 0.0;
    for (double t : params.taps) {
        tap_sum += t;
    }
    double expected_ss = input_val / (1.0 + tap_sum);
    
    // Check last few samples approach steady state
    double last_output = samples.back();
    EXPECT_NEAR(last_output, expected_ss, 0.01) 
        << "Steady state should be x/(1+sum(taps))";
    
    sc_core::sc_stop();
}

/**
 * @brief Test DFE with single tap
 */
TEST(RxDfeBasicTest, SingleTap) {
    RxDfeParams params;
    params.taps = {-0.2};  // Single tap
    
    // Input sequence: 1, 0, 0, 0, ...
    std::vector<double> input_seq = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    DfeSequenceTestbench* tb = new DfeSequenceTestbench("tb", params, input_seq);
    
    sc_core::sc_start(100, sc_core::SC_PS);  // Run 10 samples
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 3u);
    
    // y[0] = 1.0 - 0 = 1.0 (no history yet)
    // y[1] = 0.0 - (-0.2) * 1.0 = 0.2
    // y[2] = 0.0 - (-0.2) * 0.2 = 0.04
    EXPECT_NEAR(outputs[0], 1.0, 1e-10);
    EXPECT_NEAR(outputs[1], 0.2, 1e-10);
    EXPECT_NEAR(outputs[2], 0.04, 1e-10);
    
    sc_core::sc_stop();
}

/**
 * @brief Test DFE with multiple taps
 */
TEST(RxDfeBasicTest, MultipleTaps) {
    RxDfeParams params;
    params.taps = {-0.1, -0.05};  // Two taps
    
    // Input sequence: 1, 0, 0, 0, ...
    std::vector<double> input_seq = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    DfeSequenceTestbench* tb = new DfeSequenceTestbench("tb", params, input_seq);
    
    sc_core::sc_start(100, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 4u);
    
    // Manual calculation:
    // y[0] = 1.0 - 0 = 1.0
    // y[1] = 0.0 - (-0.1)*1.0 - (-0.05)*0 = 0.1
    // y[2] = 0.0 - (-0.1)*0.1 - (-0.05)*1.0 = 0.01 + 0.05 = 0.06
    // y[3] = 0.0 - (-0.1)*0.06 - (-0.05)*0.1 = 0.006 + 0.005 = 0.011
    EXPECT_NEAR(outputs[0], 1.0, 1e-10);
    EXPECT_NEAR(outputs[1], 0.1, 1e-10);
    EXPECT_NEAR(outputs[2], 0.06, 1e-10);
    EXPECT_NEAR(outputs[3], 0.011, 1e-10);
    
    sc_core::sc_stop();
}

/**
 * @brief Test DFE with empty taps (passthrough mode)
 */
TEST(RxDfeBasicTest, EmptyTapsPassthrough) {
    RxDfeParams params;
    params.taps = {};  // No taps - should act as passthrough
    
    std::vector<double> input_seq = {0.5, -0.3, 0.8, -0.1};
    
    DfeSequenceTestbench* tb = new DfeSequenceTestbench("tb", params, input_seq);
    
    sc_core::sc_start(50, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 4u);
    
    // With empty taps, output should equal input
    for (size_t i = 0; i < input_seq.size(); ++i) {
        EXPECT_NEAR(outputs[i], input_seq[i], 1e-10)
            << "Empty taps should act as passthrough at index " << i;
    }
    
    sc_core::sc_stop();
}

/**
 * @brief Test negated taps for N path (differential processing)
 */
TEST(RxDfeBasicTest, NegatedTapsForNPath) {
    // P path: normal taps
    RxDfeParams params_p;
    params_p.taps = {-0.1, -0.05, 0.02};
    
    // N path: negated taps
    RxDfeParams params_n;
    params_n.taps = {0.1, 0.05, -0.02};
    
    std::vector<double> input_seq = {0.5, 0.3, -0.2, 0.4};
    
    DfeSequenceTestbench* tb_p = new DfeSequenceTestbench("tb_p", params_p, input_seq);
    
    sc_core::sc_start(50, sc_core::SC_PS);
    const auto& outputs_p = tb_p->get_outputs();
    
    sc_core::sc_stop();
    
    // Verify taps are properly negated
    for (size_t i = 0; i < params_p.taps.size(); ++i) {
        EXPECT_NEAR(params_n.taps[i], -params_p.taps[i], 1e-15)
            << "N path taps should be negated at index " << i;
    }
}

// ============================================================================
// Main Function
// ============================================================================

int sc_main(int argc, char* argv[]) {
    // Disable SystemC deprecation warnings
    sc_core::sc_report_handler::set_actions(
        "/IEEE_Std_1666/deprecated",
        sc_core::SC_DO_NOTHING
    );
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
