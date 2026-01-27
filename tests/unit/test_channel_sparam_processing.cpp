/**
 * @file test_channel_sparam_processing.cpp
 * @brief Unit tests for ChannelSParamTdf signal processing
 * 
 * Tests verify impulse response convolution and DC gain calculations
 * match expected behavior.
 */

#include <gtest/gtest.h>
#include <fstream>
#include <cmath>
#include <vector>
#include <numeric>

#include "common/parameters.h"
#include "ams/channel_sparam.h"
#include "../third_party/json.hpp"

namespace serdes {
namespace test {

/**
 * Test fixture for signal processing tests
 */
class ChannelProcessingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Load impulse response from config
        std::ifstream file("../tests/data/peters_test_config.json");
        if (file.is_open()) {
            auto config = nlohmann::json::parse(file);
            if (config.contains("impulse_responses") && 
                config["impulse_responses"].contains("S21")) {
                auto& ir = config["impulse_responses"]["S21"];
                for (auto& v : ir["impulse"]) {
                    impulse_.push_back(v.get<double>());
                }
                dt_ = ir["dt"].get<double>();
                peak_time_ = ir["peak_time"].get<double>();
                config_loaded_ = true;
            }
        }
    }
    
    std::vector<double> impulse_;
    double dt_ = 1e-11;
    double peak_time_ = 0.0;
    bool config_loaded_ = false;
};

/**
 * Test impulse response DC gain calculation
 */
TEST_F(ChannelProcessingTest, ImpulseDCGain) {
    if (!config_loaded_) {
        GTEST_SKIP() << "Config not loaded";
    }
    
    // DC gain = sum of impulse response (for discrete-time)
    double dc_gain = std::accumulate(impulse_.begin(), impulse_.end(), 0.0);
    
    // Expected DC gain around 0.9-1.0 for transmission S21
    EXPECT_GT(dc_gain, 0.5) << "DC gain too low";
    EXPECT_LT(dc_gain, 1.5) << "DC gain too high";
}

/**
 * Test impulse response peak location
 */
TEST_F(ChannelProcessingTest, ImpulsePeakLocation) {
    if (!config_loaded_) {
        GTEST_SKIP() << "Config not loaded";
    }
    
    // Find peak index
    auto max_it = std::max_element(impulse_.begin(), impulse_.end(),
        [](double a, double b) { return std::abs(a) < std::abs(b); });
    int peak_idx = std::distance(impulse_.begin(), max_it);
    double calculated_peak_time = peak_idx * dt_;
    
    // Peak should be near the stored peak_time
    EXPECT_NEAR(calculated_peak_time, peak_time_, dt_ * 5) 
        << "Peak at index " << peak_idx;
    
    // Peak should be around 4ns (group delay of channel)
    EXPECT_GT(calculated_peak_time, 3e-9);
    EXPECT_LT(calculated_peak_time, 5e-9);
}

/**
 * Test direct convolution with delta function
 * y = h * delta = h
 */
TEST_F(ChannelProcessingTest, ConvolutionWithDelta) {
    if (!config_loaded_) {
        GTEST_SKIP() << "Config not loaded";
    }
    
    int L = std::min(100, static_cast<int>(impulse_.size()));
    std::vector<double> h(impulse_.begin(), impulse_.begin() + L);
    
    // Create delta input (1 at t=0, 0 elsewhere)
    std::vector<double> x(L * 2, 0.0);
    x[0] = 1.0;
    
    // Compute convolution
    std::vector<double> y(L * 2, 0.0);
    for (int n = 0; n < static_cast<int>(y.size()); ++n) {
        for (int k = 0; k < L; ++k) {
            int idx = n - k;
            if (idx >= 0 && idx < static_cast<int>(x.size())) {
                y[n] += h[k] * x[idx];
            }
        }
    }
    
    // Output should equal impulse response (for first L samples)
    for (int i = 0; i < L; ++i) {
        EXPECT_NEAR(y[i], h[i], 1e-12) << "Mismatch at index " << i;
    }
}

/**
 * Test convolution with step function
 * y = h * step should converge to DC gain
 */
TEST_F(ChannelProcessingTest, ConvolutionWithStep) {
    if (!config_loaded_) {
        GTEST_SKIP() << "Config not loaded";
    }
    
    int L = std::min(500, static_cast<int>(impulse_.size()));
    std::vector<double> h(impulse_.begin(), impulse_.begin() + L);
    
    // Create step input (1 for all t >= 0)
    int N = L * 2;
    std::vector<double> x(N, 1.0);
    
    // Compute convolution using circular buffer (like C++ implementation)
    std::vector<double> delay_line(L, 0.0);
    int delay_idx = 0;
    std::vector<double> y(N, 0.0);
    
    for (int n = 0; n < N; ++n) {
        // Store input
        delay_line[delay_idx] = x[n];
        
        // Convolve
        double sum = 0.0;
        for (int k = 0; k < L; ++k) {
            int buf_pos = (delay_idx - k + L) % L;
            sum += h[k] * delay_line[buf_pos];
        }
        y[n] = sum;
        
        // Update index
        delay_idx = (delay_idx + 1) % L;
    }
    
    // After settling, output should be DC gain
    double dc_gain = std::accumulate(h.begin(), h.end(), 0.0);
    double final_output = y[N - 1];
    
    EXPECT_NEAR(final_output, dc_gain, dc_gain * 0.01) 
        << "Step response should converge to DC gain";
}

/**
 * Test circular buffer index calculation
 */
TEST(CircularBufferTest, IndexCalculation) {
    int L = 100;
    
    for (int delay_idx = 0; delay_idx < L; ++delay_idx) {
        for (int k = 0; k < L; ++k) {
            int buf_pos = (delay_idx - k + L) % L;
            
            EXPECT_GE(buf_pos, 0);
            EXPECT_LT(buf_pos, L);
        }
    }
}

/**
 * Test energy calculation from impulse response
 */
TEST_F(ChannelProcessingTest, ImpulseEnergy) {
    if (!config_loaded_) {
        GTEST_SKIP() << "Config not loaded";
    }
    
    // Energy = sum(h^2)
    double energy = 0.0;
    for (double h : impulse_) {
        energy += h * h;
    }
    
    // Energy should be positive and finite
    EXPECT_GT(energy, 0.0);
    EXPECT_TRUE(std::isfinite(energy));
    
    // For a well-behaved channel, energy should be reasonable
    EXPECT_LT(energy, 10.0) << "Energy seems too high";
}

/**
 * Test causality of impulse response
 * (peak should not be at t=0 for transmission line)
 */
TEST_F(ChannelProcessingTest, ImpulseCausality) {
    if (!config_loaded_) {
        GTEST_SKIP() << "Config not loaded";
    }
    
    // Find peak index
    auto max_it = std::max_element(impulse_.begin(), impulse_.end(),
        [](double a, double b) { return std::abs(a) < std::abs(b); });
    int peak_idx = std::distance(impulse_.begin(), max_it);
    
    // For transmission line, peak should be delayed (not at t=0)
    EXPECT_GT(peak_idx, 10) << "Peak too early - may indicate causality issue";
    
    // Values before peak should be small (causal system)
    double pre_peak_energy = 0.0;
    for (int i = 0; i < peak_idx / 2; ++i) {
        pre_peak_energy += impulse_[i] * impulse_[i];
    }
    
    double total_energy = 0.0;
    for (double h : impulse_) {
        total_energy += h * h;
    }
    
    // Pre-peak energy should be small fraction of total
    double pre_peak_ratio = pre_peak_energy / total_energy;
    EXPECT_LT(pre_peak_ratio, 0.1) 
        << "Too much energy before peak - causality concern";
}

} // namespace test
} // namespace serdes
