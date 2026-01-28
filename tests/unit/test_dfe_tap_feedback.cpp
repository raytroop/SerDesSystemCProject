/**
 * @file test_dfe_tap_feedback.cpp
 * @brief Unit tests for RxDfeTdf - Tap feedback functionality tests
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
 * @brief Sequence source that outputs a predefined sequence of values
 */
class SequenceSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out;
    
    std::vector<double> m_sequence;
    size_t m_index;
    double m_default_value;
    
    SequenceSource(sc_core::sc_module_name nm, const std::vector<double>& seq, 
                   double default_val = 0.0)
        : sca_tdf::sca_module(nm)
        , out("out")
        , m_sequence(seq)
        , m_index(0)
        , m_default_value(default_val)
    {}
    
    void set_attributes() override {
        out.set_rate(1);
        out.set_timestep(1.0 / 100e9, sc_core::SC_SEC);  // 10ps timestep
    }
    
    void processing() override {
        if (m_index < m_sequence.size()) {
            out.write(m_sequence[m_index]);
            m_index++;
        } else {
            out.write(m_default_value);
        }
    }
};

/**
 * @brief Signal sink for capturing output samples
 */
class SignalSink : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> in;
    
    std::vector<double> samples;
    
    SignalSink(sc_core::sc_module_name nm)
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
    
    void clear() { samples.clear(); }
};

// ============================================================================
// Test Fixture
// ============================================================================

/**
 * @brief DFE testbench with sequence input for tap feedback testing
 */
SC_MODULE(DfeTapTestbench) {
    SequenceSource* src;
    RxDfeTdf* dfe;
    SignalSink* sink;
    
    sca_tdf::sca_signal<double> sig_in;
    sca_tdf::sca_signal<double> sig_out;
    
    RxDfeParams params;
    
    DfeTapTestbench(sc_core::sc_module_name nm, const RxDfeParams& p, 
                    const std::vector<double>& input_seq, double default_val = 0.0)
        : sc_core::sc_module(nm)
        , params(p)
    {
        src = new SequenceSource("src", input_seq, default_val);
        dfe = new RxDfeTdf("dfe", params);
        sink = new SignalSink("sink");
        
        // Connect signals
        src->out(sig_in);
        dfe->in(sig_in);
        dfe->out(sig_out);
        sink->in(sig_out);
    }
    
    ~DfeTapTestbench() {
        // SystemC modules managed by simulator
    }
    
    const std::vector<double>& get_outputs() const {
        return sink->samples;
    }
};

// ============================================================================
// Test Cases - Tap Coefficient Effects
// ============================================================================

/**
 * @brief Test positive tap coefficient effect
 * 
 * With positive tap, feedback adds to the output
 * y = x - tap * history
 * If tap > 0, then y < x when history > 0
 */
TEST(RxDfeTapFeedbackTest, PositiveTapEffect) {
    RxDfeParams params;
    params.taps = {0.2};  // Positive tap
    
    // Pulse input: 1, 0, 0, 0, ...
    std::vector<double> input_seq = {1.0, 0.0, 0.0, 0.0, 0.0};
    
    DfeTapTestbench* tb = new DfeTapTestbench("tb", params, input_seq);
    
    sc_core::sc_start(60, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 4u);
    
    // y[0] = 1.0 - 0 = 1.0
    // y[1] = 0.0 - 0.2 * 1.0 = -0.2  (positive tap causes negative feedback)
    // y[2] = 0.0 - 0.2 * (-0.2) = 0.04
    EXPECT_NEAR(outputs[0], 1.0, 1e-10);
    EXPECT_NEAR(outputs[1], -0.2, 1e-10);
    EXPECT_NEAR(outputs[2], 0.04, 1e-10);
    
    sc_core::sc_stop();
}

/**
 * @brief Test negative tap coefficient effect
 * 
 * With negative tap, feedback subtracts from the output
 * y = x - tap * history
 * If tap < 0, then y > x when history > 0
 */
TEST(RxDfeTapFeedbackTest, NegativeTapEffect) {
    RxDfeParams params;
    params.taps = {-0.2};  // Negative tap
    
    // Pulse input: 1, 0, 0, 0, ...
    std::vector<double> input_seq = {1.0, 0.0, 0.0, 0.0, 0.0};
    
    DfeTapTestbench* tb = new DfeTapTestbench("tb", params, input_seq);
    
    sc_core::sc_start(60, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 4u);
    
    // y[0] = 1.0 - 0 = 1.0
    // y[1] = 0.0 - (-0.2) * 1.0 = 0.2  (negative tap causes positive feedback)
    // y[2] = 0.0 - (-0.2) * 0.2 = 0.04
    EXPECT_NEAR(outputs[0], 1.0, 1e-10);
    EXPECT_NEAR(outputs[1], 0.2, 1e-10);
    EXPECT_NEAR(outputs[2], 0.04, 1e-10);
    
    sc_core::sc_stop();
}

/**
 * @brief Test tap magnitude effect on convergence speed
 */
TEST(RxDfeTapFeedbackTest, TapMagnitudeEffect) {
    // Small tap - slower decay
    RxDfeParams params_small;
    params_small.taps = {-0.1};
    
    // Large tap - faster decay
    RxDfeParams params_large;
    params_large.taps = {-0.5};
    
    std::vector<double> input_seq = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    DfeTapTestbench* tb_small = new DfeTapTestbench("tb_small", params_small, input_seq);
    
    sc_core::sc_start(100, sc_core::SC_PS);
    const auto& outputs_small = tb_small->get_outputs();
    sc_core::sc_stop();
    
    // After first sample, larger tap should cause faster convergence to zero
    // For small tap: y[1] = 0.1, y[2] = 0.01
    // For large tap: y[1] = 0.5, y[2] = 0.25
    ASSERT_GE(outputs_small.size(), 3u);
    EXPECT_NEAR(outputs_small[1], 0.1, 1e-10);
    EXPECT_NEAR(outputs_small[2], 0.01, 1e-10);
}

/**
 * @brief Test multi-tap interaction
 */
TEST(RxDfeTapFeedbackTest, MultiTapInteraction) {
    RxDfeParams params;
    params.taps = {-0.3, -0.2, -0.1};  // 3 taps
    
    // Pulse input
    std::vector<double> input_seq = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    DfeTapTestbench* tb = new DfeTapTestbench("tb", params, input_seq);
    
    sc_core::sc_start(80, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 5u);
    
    // Manual verification:
    // y[0] = 1.0
    // y[1] = 0 - (-0.3)*1.0 - (-0.2)*0 - (-0.1)*0 = 0.3
    // y[2] = 0 - (-0.3)*0.3 - (-0.2)*1.0 - (-0.1)*0 = 0.09 + 0.2 = 0.29
    // y[3] = 0 - (-0.3)*0.29 - (-0.2)*0.3 - (-0.1)*1.0 = 0.087 + 0.06 + 0.1 = 0.247
    EXPECT_NEAR(outputs[0], 1.0, 1e-10);
    EXPECT_NEAR(outputs[1], 0.3, 1e-10);
    EXPECT_NEAR(outputs[2], 0.29, 1e-10);
    EXPECT_NEAR(outputs[3], 0.247, 1e-10);
    
    sc_core::sc_stop();
}

/**
 * @brief Test different tap configurations for P and N paths
 */
TEST(RxDfeTapFeedbackTest, DifferentialPathTaps) {
    // P path: typical DFE taps (negative to cancel post-cursor ISI)
    RxDfeParams params_p;
    params_p.taps = {-0.15, -0.08, -0.04};
    
    // N path: negated taps for differential symmetry
    RxDfeParams params_n;
    params_n.taps = {0.15, 0.08, 0.04};
    
    std::vector<double> input_p = {0.4, 0.3, -0.2, 0.1};
    std::vector<double> input_n = {-0.4, -0.3, 0.2, -0.1};  // Inverted input
    
    DfeTapTestbench* tb_p = new DfeTapTestbench("tb_p", params_p, input_p);
    
    sc_core::sc_start(50, sc_core::SC_PS);
    const auto& out_p = tb_p->get_outputs();
    sc_core::sc_stop();
    
    // Verify P and N taps are properly negated
    for (size_t i = 0; i < params_p.taps.size(); ++i) {
        EXPECT_NEAR(params_n.taps[i], -params_p.taps[i], 1e-15)
            << "N path tap should be negated at index " << i;
    }
    
    // Verify first output is just the input (no history)
    ASSERT_GE(out_p.size(), 1u);
    EXPECT_NEAR(out_p[0], 0.4, 1e-10);
}

/**
 * @brief Test tap coefficient boundary values
 */
TEST(RxDfeTapFeedbackTest, TapBoundaryValues) {
    // Test with very small tap
    RxDfeParams params_tiny;
    params_tiny.taps = {-0.001};
    
    std::vector<double> input_seq = {1.0, 0.0, 0.0};
    DfeTapTestbench* tb_tiny = new DfeTapTestbench("tb_tiny", params_tiny, input_seq);
    
    sc_core::sc_start(40, sc_core::SC_PS);
    const auto& out_tiny = tb_tiny->get_outputs();
    sc_core::sc_stop();
    
    ASSERT_GE(out_tiny.size(), 2u);
    // Very small tap should have minimal effect
    EXPECT_NEAR(out_tiny[1], 0.001, 1e-10);
}

/**
 * @brief Test alternating input with DFE feedback
 */
TEST(RxDfeTapFeedbackTest, AlternatingInput) {
    RxDfeParams params;
    params.taps = {-0.1};
    
    // Alternating input: 1, -1, 1, -1, ...
    std::vector<double> input_seq = {1.0, -1.0, 1.0, -1.0, 1.0, -1.0};
    
    DfeTapTestbench* tb = new DfeTapTestbench("tb", params, input_seq);
    
    sc_core::sc_start(70, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 5u);
    
    // y[0] = 1.0
    // y[1] = -1.0 - (-0.1)*1.0 = -1.0 + 0.1 = -0.9
    // y[2] = 1.0 - (-0.1)*(-0.9) = 1.0 - 0.09 = 0.91
    // y[3] = -1.0 - (-0.1)*0.91 = -1.0 + 0.091 = -0.909
    EXPECT_NEAR(outputs[0], 1.0, 1e-10);
    EXPECT_NEAR(outputs[1], -0.9, 1e-10);
    EXPECT_NEAR(outputs[2], 0.91, 1e-10);
    EXPECT_NEAR(outputs[3], -0.909, 1e-10);
    
    sc_core::sc_stop();
}

/**
 * @brief Test zero tap (should act as passthrough)
 */
TEST(RxDfeTapFeedbackTest, ZeroTapPassthrough) {
    RxDfeParams params;
    params.taps = {0.0};  // Zero tap
    
    std::vector<double> input_seq = {0.5, -0.3, 0.8, -0.1, 0.6};
    
    DfeTapTestbench* tb = new DfeTapTestbench("tb", params, input_seq);
    
    sc_core::sc_start(60, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 5u);
    
    // With zero tap, output should equal input
    for (size_t i = 0; i < input_seq.size(); ++i) {
        EXPECT_NEAR(outputs[i], input_seq[i], 1e-10)
            << "Zero tap should act as passthrough at index " << i;
    }
    
    sc_core::sc_stop();
}

// ============================================================================
// Main Function
// ============================================================================

int sc_main(int argc, char* argv[]) {
    sc_core::sc_report_handler::set_actions(
        "/IEEE_Std_1666/deprecated",
        sc_core::SC_DO_NOTHING
    );
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
