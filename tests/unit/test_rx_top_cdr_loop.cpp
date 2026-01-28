/**
 * @file test_rx_top_cdr_loop.cpp
 * @brief CDR closed-loop tests for RxTopModule
 * 
 * Tests:
 * - Phase tracking behavior
 * - Lock acquisition
 * - CDR state evolution
 */

#include <gtest/gtest.h>
#include <systemc-ams>
#include "rx_top_test_common.h"
#include <cmath>

using namespace serdes;
using namespace serdes::test;

// ============================================================================
// CDR Phase Output Test
// ============================================================================

TEST(RxTopCdrLoopTest, CdrPhaseOutputValid) {
    // Setup
    RxParams params = get_default_rx_params();
    RxTopTestbench tb(params, RxDifferentialSource::PRBS, 0.4, 5e9);
    
    // Run simulation long enough for CDR to start tracking
    sc_core::sc_start(1000, sc_core::SC_NS);
    
    // Verify CDR phase is accessible and finite
    double cdr_phase = tb.get_cdr_phase();
    EXPECT_TRUE(std::isfinite(cdr_phase)) << "CDR phase should be finite";
    
    // Phase should be within PAI range
    double pai_range = params.cdr.pai.range;
    EXPECT_LE(std::abs(cdr_phase), pai_range * 2) 
        << "CDR phase should be within reasonable range";
    
    sc_core::sc_stop();
}

// ============================================================================
// CDR Integral State Test
// ============================================================================

TEST(RxTopCdrLoopTest, CdrIntegralStateEvolution) {
    // Setup
    RxParams params = get_default_rx_params();
    RxTopTestbench tb(params, RxDifferentialSource::PRBS, 0.4, 5e9);
    
    // Get initial integral state
    double initial_integral = tb.get_cdr_integral_state();
    
    // Run simulation
    sc_core::sc_start(500, sc_core::SC_NS);
    
    // Get final integral state
    double final_integral = tb.get_cdr_integral_state();
    
    // Integral state should be finite
    EXPECT_TRUE(std::isfinite(initial_integral)) << "Initial integral should be finite";
    EXPECT_TRUE(std::isfinite(final_integral)) << "Final integral should be finite";
    
    // With PRBS input and CDR tracking, integral may change
    // (This is a sanity check, not a strict requirement)
    
    sc_core::sc_stop();
}

// ============================================================================
// Aggressive CDR Gains Test
// ============================================================================

TEST(RxTopCdrLoopTest, AggressiveCdrGains) {
    // Setup with aggressive CDR gains (faster tracking)
    RxParams params = get_aggressive_cdr_params();
    
    RxTopTestbench tb(params, RxDifferentialSource::PRBS, 0.4, 5e9);
    
    // Run simulation
    sc_core::sc_start(500, sc_core::SC_NS);
    
    // Verify output
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    
    // Aggressive CDR should still produce valid output
    EXPECT_TRUE(tb.monitor->is_valid_data()) 
        << "Aggressive CDR should produce valid data";
    
    sc_core::sc_stop();
}

// ============================================================================
// Conservative CDR Gains Test
// ============================================================================

TEST(RxTopCdrLoopTest, ConservativeCdrGains) {
    // Setup with conservative CDR gains (slower tracking)
    RxParams params = get_default_rx_params();
    params.cdr.pi.kp = 0.001;   // 10x smaller
    params.cdr.pi.ki = 1e-5;    // 10x smaller
    
    RxTopTestbench tb(params, RxDifferentialSource::PRBS, 0.4, 5e9);
    
    // Run simulation
    sc_core::sc_start(500, sc_core::SC_NS);
    
    // Verify output
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    
    // CDR phase should change slowly with conservative gains
    double cdr_phase = tb.get_cdr_phase();
    EXPECT_TRUE(std::isfinite(cdr_phase)) << "CDR phase should be finite";
    
    sc_core::sc_stop();
}

// ============================================================================
// CDR with Square Wave Input Test
// ============================================================================

TEST(RxTopCdrLoopTest, SquareWaveTracking) {
    // Setup with square wave input (regular transitions)
    RxParams params = get_default_rx_params();
    RxTopTestbench tb(params, RxDifferentialSource::SQUARE, 0.4, 5e9);
    
    // Run simulation
    sc_core::sc_start(1000, sc_core::SC_NS);
    
    // With regular square wave, CDR should track well
    ASSERT_GT(tb.get_sample_count(), 0) << "No samples collected";
    EXPECT_TRUE(tb.monitor->is_valid_data()) 
        << "Square wave should produce valid output";
    
    // Verify transitions detected
    size_t transitions = tb.monitor->count_transitions();
    EXPECT_GT(transitions, 0) << "Square wave should produce transitions";
    
    sc_core::sc_stop();
}

// ============================================================================
// CDR Phase Range Limiting Test
// ============================================================================

TEST(RxTopCdrLoopTest, PhaseRangeLimiting) {
    // Setup with small PAI range
    RxParams params = get_default_rx_params();
    params.cdr.pai.range = 1e-11;  // Very small range
    
    RxTopTestbench tb(params, RxDifferentialSource::PRBS, 0.4, 5e9);
    
    // Run simulation
    sc_core::sc_start(500, sc_core::SC_NS);
    
    // Verify CDR phase is within range
    double cdr_phase = tb.get_cdr_phase();
    double pai_range = params.cdr.pai.range;
    
    // Phase should be limited to PAI range
    EXPECT_LE(std::abs(cdr_phase), pai_range * 1.1) 
        << "CDR phase should be limited by PAI range";
    
    sc_core::sc_stop();
}
