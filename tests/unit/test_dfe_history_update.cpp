/**
 * @file test_dfe_history_update.cpp
 * @brief Unit tests for RxDfeTdf - History buffer update (FIFO shift) tests
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
 * @brief Sequence source that outputs a predefined sequence
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

/**
 * @brief Signal sink for capturing output
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
};

// ============================================================================
// Test Fixture
// ============================================================================

/**
 * @brief DFE testbench for history update testing
 */
SC_MODULE(DfeHistoryTestbench) {
    SequenceSource* src;
    RxDfeTdf* dfe;
    SignalSink* sink;
    
    sca_tdf::sca_signal<double> sig_in;
    sca_tdf::sca_signal<double> sig_out;
    
    RxDfeParams params;
    
    DfeHistoryTestbench(sc_core::sc_module_name nm, const RxDfeParams& p, 
                        const std::vector<double>& input_seq)
        : sc_core::sc_module(nm)
        , params(p)
    {
        src = new SequenceSource("src", input_seq);
        dfe = new RxDfeTdf("dfe", params);
        sink = new SignalSink("sink");
        
        src->out(sig_in);
        dfe->in(sig_in);
        dfe->out(sig_out);
        sink->in(sig_out);
    }
    
    ~DfeHistoryTestbench() {}
    
    const std::vector<double>& get_outputs() const {
        return sink->samples;
    }
};

// ============================================================================
// Test Cases - History Buffer Update
// ============================================================================

/**
 * @brief Test initial history state (all zeros)
 */
TEST(RxDfeHistoryTest, InitialHistoryZeros) {
    RxDfeParams params;
    params.taps = {-0.1, -0.05, -0.02};  // 3 taps
    
    // First input should have no feedback effect (history = 0)
    std::vector<double> input_seq = {0.5};
    
    DfeHistoryTestbench* tb = new DfeHistoryTestbench("tb", params, input_seq);
    
    sc_core::sc_start(20, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 1u);
    
    // y[0] = 0.5 - sum(taps[i] * 0) = 0.5
    EXPECT_NEAR(outputs[0], 0.5, 1e-10) 
        << "First output should equal input (initial history is zero)";
    
    sc_core::sc_stop();
}

/**
 * @brief Test history FIFO shift operation with single tap
 * 
 * History buffer should shift: [new, old1, old2, ...]
 * history[0] = current output
 * history[i] = history[i-1] from previous step
 */
TEST(RxDfeHistoryTest, SingleTapFifoShift) {
    RxDfeParams params;
    params.taps = {-0.5};  // Single tap
    
    // Sequence of distinct values to trace history
    std::vector<double> input_seq = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    DfeHistoryTestbench* tb = new DfeHistoryTestbench("tb", params, input_seq);
    
    sc_core::sc_start(60, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 5u);
    
    // y[0] = 1.0 - 0 = 1.0, history[0] = 1.0
    // y[1] = 2.0 - (-0.5)*1.0 = 2.5, history[0] = 2.5
    // y[2] = 3.0 - (-0.5)*2.5 = 4.25, history[0] = 4.25
    // y[3] = 4.0 - (-0.5)*4.25 = 6.125, history[0] = 6.125
    // y[4] = 5.0 - (-0.5)*6.125 = 8.0625
    EXPECT_NEAR(outputs[0], 1.0, 1e-10);
    EXPECT_NEAR(outputs[1], 2.5, 1e-10);
    EXPECT_NEAR(outputs[2], 4.25, 1e-10);
    EXPECT_NEAR(outputs[3], 6.125, 1e-10);
    EXPECT_NEAR(outputs[4], 8.0625, 1e-10);
    
    sc_core::sc_stop();
}

/**
 * @brief Test history FIFO shift with multiple taps
 * 
 * With 3 taps, history[0..2] should track the last 3 outputs
 */
TEST(RxDfeHistoryTest, MultiTapFifoShift) {
    RxDfeParams params;
    params.taps = {-0.1, -0.2, -0.3};  // 3 taps
    
    // Impulse followed by zeros
    std::vector<double> input_seq = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    DfeHistoryTestbench* tb = new DfeHistoryTestbench("tb", params, input_seq);
    
    sc_core::sc_start(80, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 6u);
    
    // Trace the FIFO shift:
    // n=0: in=1, hist=[0,0,0], y=1.0, new_hist=[1.0, 0, 0]
    // n=1: in=0, hist=[1.0,0,0], y=0-(-0.1)*1.0-(-0.2)*0-(-0.3)*0=0.1, new_hist=[0.1,1.0,0]
    // n=2: in=0, hist=[0.1,1.0,0], y=0-(-0.1)*0.1-(-0.2)*1.0-(-0.3)*0=0.01+0.2=0.21
    //      new_hist=[0.21,0.1,1.0]
    // n=3: in=0, hist=[0.21,0.1,1.0], y=0-(-0.1)*0.21-(-0.2)*0.1-(-0.3)*1.0
    //      y=0.021+0.02+0.3=0.341, new_hist=[0.341,0.21,0.1]
    // n=4: in=0, hist=[0.341,0.21,0.1]
    //      y=0-(-0.1)*0.341-(-0.2)*0.21-(-0.3)*0.1=0.0341+0.042+0.03=0.1061
    
    EXPECT_NEAR(outputs[0], 1.0, 1e-10);
    EXPECT_NEAR(outputs[1], 0.1, 1e-10);
    EXPECT_NEAR(outputs[2], 0.21, 1e-10);
    EXPECT_NEAR(outputs[3], 0.341, 1e-10);
    EXPECT_NEAR(outputs[4], 0.1061, 1e-10);
    
    sc_core::sc_stop();
}

/**
 * @brief Test that history stores OUTPUT values, not INPUT values
 * 
 * The DFE should store y[n] in history, not x[n]
 */
TEST(RxDfeHistoryTest, HistoryStoresOutputNotInput) {
    RxDfeParams params;
    params.taps = {-1.0};  // Tap = -1 to easily verify history content
    
    // y = x - (-1)*history[0] = x + history[0]
    std::vector<double> input_seq = {1.0, 1.0, 1.0, 1.0};
    
    DfeHistoryTestbench* tb = new DfeHistoryTestbench("tb", params, input_seq);
    
    sc_core::sc_start(50, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 4u);
    
    // If history stores output:
    // y[0] = 1 + 0 = 1, history = [1]
    // y[1] = 1 + 1 = 2, history = [2]
    // y[2] = 1 + 2 = 3, history = [3]
    // y[3] = 1 + 3 = 4, history = [4]
    
    // If history stored input (WRONG), it would be:
    // y[0] = 1 + 0 = 1
    // y[1] = 1 + 1 = 2
    // y[2] = 1 + 1 = 2 (wrong!)
    
    EXPECT_NEAR(outputs[0], 1.0, 1e-10);
    EXPECT_NEAR(outputs[1], 2.0, 1e-10);
    EXPECT_NEAR(outputs[2], 3.0, 1e-10);
    EXPECT_NEAR(outputs[3], 4.0, 1e-10);
    
    sc_core::sc_stop();
}

/**
 * @brief Test history buffer size matches tap count
 */
TEST(RxDfeHistoryTest, HistorySizeMatchesTaps) {
    // 5 taps should use 5 history slots
    RxDfeParams params;
    params.taps = {-0.1, -0.08, -0.06, -0.04, -0.02};
    
    // First 6 inputs as impulse response test
    std::vector<double> input_seq = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    DfeHistoryTestbench* tb = new DfeHistoryTestbench("tb", params, input_seq);
    
    sc_core::sc_start(90, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 7u);
    
    // y[0] = 1.0
    // y[1] = 0 + 0.1*1.0 = 0.1
    // y[2] = 0 + 0.1*0.1 + 0.08*1.0 = 0.01 + 0.08 = 0.09
    // y[3] = 0 + 0.1*0.09 + 0.08*0.1 + 0.06*1.0 = 0.009 + 0.008 + 0.06 = 0.077
    // y[4] = 0 + 0.1*0.077 + 0.08*0.09 + 0.06*0.1 + 0.04*1.0
    //      = 0.0077 + 0.0072 + 0.006 + 0.04 = 0.0609
    // y[5] = 0 + 0.1*0.0609 + 0.08*0.077 + 0.06*0.09 + 0.04*0.1 + 0.02*1.0
    //      = 0.00609 + 0.00616 + 0.0054 + 0.004 + 0.02 = 0.04165
    
    EXPECT_NEAR(outputs[0], 1.0, 1e-10);
    EXPECT_NEAR(outputs[1], 0.1, 1e-10);
    EXPECT_NEAR(outputs[2], 0.09, 1e-10);
    EXPECT_NEAR(outputs[3], 0.077, 1e-10);
    EXPECT_NEAR(outputs[4], 0.0609, 1e-10);
    EXPECT_NEAR(outputs[5], 0.04165, 1e-10);
    
    sc_core::sc_stop();
}

/**
 * @brief Test impulse response decay with history
 */
TEST(RxDfeHistoryTest, ImpulseResponseDecay) {
    RxDfeParams params;
    params.taps = {-0.3, -0.2};  // 2 taps
    
    // Single impulse followed by zeros
    std::vector<double> input_seq = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    DfeHistoryTestbench* tb = new DfeHistoryTestbench("tb", params, input_seq);
    
    sc_core::sc_start(120, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 8u);
    
    // Impulse response should decay toward zero
    // Check that outputs eventually become very small
    double last_significant = outputs[0];
    for (size_t i = 1; i < outputs.size(); ++i) {
        // Each output should be smaller in magnitude (for this tap config)
        // or at least bounded
        EXPECT_LT(std::abs(outputs[i]), std::abs(last_significant) * 2.0)
            << "Output should be bounded at index " << i;
        if (std::abs(outputs[i]) > 1e-6) {
            last_significant = outputs[i];
        }
    }
    
    // Final outputs should be close to zero
    EXPECT_LT(std::abs(outputs.back()), 0.01)
        << "Impulse response should decay toward zero";
    
    sc_core::sc_stop();
}

/**
 * @brief Test continuous data pattern through history
 */
TEST(RxDfeHistoryTest, ContinuousDataPattern) {
    RxDfeParams params;
    params.taps = {-0.15, -0.1};
    
    // Repeated pattern: 0.5, -0.5, 0.5, -0.5, ...
    std::vector<double> input_seq = {0.5, -0.5, 0.5, -0.5, 0.5, -0.5, 0.5, -0.5};
    
    DfeHistoryTestbench* tb = new DfeHistoryTestbench("tb", params, input_seq);
    
    sc_core::sc_start(100, sc_core::SC_PS);
    
    const auto& outputs = tb->get_outputs();
    ASSERT_GE(outputs.size(), 8u);
    
    // Just verify the outputs are reasonable (bounded)
    for (size_t i = 0; i < outputs.size(); ++i) {
        EXPECT_LT(std::abs(outputs[i]), 2.0)
            << "Output should be bounded at index " << i;
    }
    
    // After settling, pattern should stabilize
    // Check last few outputs show periodic behavior
    if (outputs.size() >= 8) {
        // The difference between alternating outputs should stabilize
        double diff1 = std::abs(outputs[6] - outputs[4]);
        double diff2 = std::abs(outputs[7] - outputs[5]);
        EXPECT_LT(diff1, 0.1) << "Pattern should stabilize";
        EXPECT_LT(diff2, 0.1) << "Pattern should stabilize";
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
