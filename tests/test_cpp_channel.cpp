/**
 * @file test_cpp_channel.cpp
 * @brief Test program for C++ PoleResidueFilter
 * 
 * This is a standalone test that doesn't require SystemC-AMS.
 * It verifies the C++ filter against a simple known transfer function.
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <fstream>
#include <iomanip>
#include <algorithm>

#include "pole_residue_filter.h"

using namespace serdes::cpp;

// ============================================================================
// Test 1: Simple first-order low-pass filter
// H(s) = 1 / (s/omega_c + 1) = omega_c / (s + omega_c)
// Pole: p = -omega_c, Residue: r = omega_c, Constant: 0
// ============================================================================
bool test_first_order_lpf() {
    std::cout << "\n=== Test 1: First-order LPF ===\n";
    
    const double fc = 1e9;  // 1 GHz cutoff
    const double omega_c = 2.0 * M_PI * fc;
    const double fs = 100e9;  // 100 GHz sampling
    
    // H(s) = omega_c / (s + omega_c)
    // Pole: -omega_c, Residue: omega_c
    std::vector<std::complex<double>> poles = {std::complex<double>(-omega_c, 0.0)};
    std::vector<std::complex<double>> residues = {std::complex<double>(omega_c, 0.0)};
    
    PoleResidueFilter filter;
    bool ok = filter.init(poles, residues, 0.0, 0.0, fs);
    
    if (!ok) {
        std::cerr << "FAILED: Filter initialization\n";
        return false;
    }
    
    // DC gain should be 1.0
    double dc_gain = filter.get_dc_gain();
    std::cout << "DC gain: " << dc_gain << " (expected: 1.0)\n";
    
    if (std::abs(dc_gain - 1.0) > 0.01) {
        std::cerr << "FAILED: DC gain error\n";
        return false;
    }
    
    // Step response test
    const int n_samples = 1000;
    std::vector<double> input(n_samples, 1.0);  // Step input
    std::vector<double> output(n_samples);
    
    // First sample is 0 (step starts at t=0)
    output[0] = filter.process(0.0);
    for (int i = 1; i < n_samples; ++i) {
        output[i] = filter.process(1.0);
    }
    
    // Check settling (should reach ~99% by 5*tau)
    // tau = 1/omega_c, 5*tau at sample 5*fs/(2*pi*fc) = 5*100e9/6.28e9 ≈ 80 samples
    int settling_sample = static_cast<int>(5.0 * fs / omega_c);
    if (settling_sample >= n_samples) settling_sample = n_samples - 1;
    
    std::cout << "Output at sample " << settling_sample << ": " 
              << output[settling_sample] << " (expected: ~0.99)\n";
    
    if (output[settling_sample] < 0.95) {
        std::cerr << "FAILED: Settling time too long\n";
        return false;
    }
    
    // Save step response for plotting
    std::ofstream file("test_step_response.csv");
    file << "sample,input,output\n";
    for (int i = 0; i < n_samples; ++i) {
        file << i << "," << (i == 0 ? 0.0 : 1.0) << "," << output[i] << "\n";
    }
    file.close();
    std::cout << "Saved step response to test_step_response.csv\n";
    
    std::cout << "PASSED\n";
    return true;
}

// ============================================================================
// Test 2: Frequency response verification
// ============================================================================
bool test_frequency_response() {
    std::cout << "\n=== Test 2: Frequency Response ===\n";
    
    const double fc = 1e9;  // 1 GHz
    const double omega_c = 2.0 * M_PI * fc;
    const double fs = 100e9;
    
    std::vector<std::complex<double>> poles = {std::complex<double>(-omega_c, 0.0)};
    std::vector<std::complex<double>> residues = {std::complex<double>(omega_c, 0.0)};
    
    PoleResidueFilter filter;
    filter.init(poles, residues, 0.0, 0.0, fs);
    
    // Test frequencies
    std::vector<double> freqs = {0.1e9, 0.5e9, 1.0e9, 2.0e9, 5.0e9};
    std::vector<double> mag_expected(freqs.size());
    
    std::cout << "Frequency (GHz) | Expected | Computed | Error\n";
    std::cout << "------------------------------------------------\n";
    
    bool pass = true;
    for (size_t i = 0; i < freqs.size(); ++i) {
        double f = freqs[i];
        
        // Theoretical: |H(jw)| = omega_c / sqrt(w^2 + omega_c^2)
        double w = 2.0 * M_PI * f;
        double mag_exp = omega_c / std::sqrt(w*w + omega_c*omega_c);
        
        // Get from filter
        std::vector<double> freq_vec = {f};
        double mag_comp, phase_comp;
        filter.get_frequency_response(freq_vec.data(), 1, &mag_comp, &phase_comp, nullptr, nullptr);
        
        double error = std::abs(mag_comp - mag_exp) / mag_exp * 100.0;
        
        std::cout << std::fixed << std::setprecision(1);
        std::cout << f/1e9 << "             | " << mag_exp << " | " 
                  << mag_comp << " | " << error << "%\n";
        
        if (error > 5.0) {  // Allow 5% error
            pass = false;
        }
    }
    
    if (pass) {
        std::cout << "PASSED\n";
    } else {
        std::cerr << "FAILED: Frequency response error too large\n";
    }
    
    return pass;
}

// ============================================================================
// Test 3: Complex pole pair (resonator)
// ============================================================================
bool test_complex_pole() {
    std::cout << "\n=== Test 3: Complex Pole Pair ===\n";
    
    const double f0 = 5e9;  // 5 GHz resonant frequency
    const double Q = 10.0;   // Quality factor
    const double fs = 100e9;
    
    // Second-order resonator: H(s) = (omega_0/Q * s) / (s^2 + omega_0/Q * s + omega_0^2)
    // Poles: p = -omega_0/(2Q) ± j*omega_0*sqrt(1 - 1/(4Q^2))
    // Approx: p ≈ -omega_0/(2Q) ± j*omega_0 for high Q
    
    const double omega_0 = 2.0 * M_PI * f0;
    const double sigma = -omega_0 / (2.0 * Q);
    const double omega_d = omega_0 * std::sqrt(1.0 - 1.0/(4.0*Q*Q));
    
    // For pole-residue form with complex poles (upper half plane only):
    // H(s) = r/(s-p) + r*/(s-p*)
    // Let's use a bandpass response
    std::complex<double> p(sigma, omega_d);
    std::complex<double> r(1.0, 0.0);  // Simplified residue
    
    std::vector<std::complex<double>> poles = {p};
    std::vector<std::complex<double>> residues = {r};
    
    PoleResidueFilter filter;
    bool ok = filter.init(poles, residues, 0.0, 0.0, fs);
    
    if (!ok) {
        std::cerr << "FAILED: Complex pole initialization\n";
        return false;
    }
    
    std::cout << "Initialized with complex pole at " << f0/1e9 << " GHz, Q=" << Q << "\n";
    std::cout << "Number of sections: " << filter.get_num_sections() << " (expected: 1)\n";
    
    // Check frequency response has peak at f0
    std::vector<double> freqs;
    for (double f = 0.1e9; f <= 10e9; f += 0.1e9) {
        freqs.push_back(f);
    }
    
    std::vector<double> mags(freqs.size());
    std::vector<double> phases(freqs.size());
    filter.get_frequency_response(freqs.data(), static_cast<int>(freqs.size()),
                                   mags.data(), phases.data(), nullptr, nullptr);
    
    // Find peak
    auto max_it = std::max_element(mags.begin(), mags.end());
    size_t peak_idx = std::distance(mags.begin(), max_it);
    double peak_freq = freqs[peak_idx];
    
    std::cout << "Response peak at " << peak_freq/1e9 << " GHz (expected: ~" << f0/1e9 << " GHz)\n";
    
    // Allow 10% frequency error
    if (std::abs(peak_freq - f0) / f0 > 0.1) {
        std::cerr << "FAILED: Peak frequency error too large\n";
        return false;
    }
    
    std::cout << "PASSED\n";
    return true;
}

// ============================================================================
// Test 4: Impulse response energy conservation
// ============================================================================
bool test_energy_conservation() {
    std::cout << "\n=== Test 4: Energy Conservation ===\n";
    
    // A lossless all-pass filter should preserve energy
    // H(s) = (s - a) / (s + a) for real a > 0
    
    const double a = 2.0 * M_PI * 1e9;  // 1 GHz
    const double fs = 100e9;
    
    // Poles: -a, Residues: -2a (from partial fraction of (s-a)/(s+a) = 1 - 2a/(s+a))
    std::vector<std::complex<double>> poles = {std::complex<double>(-a, 0.0)};
    std::vector<std::complex<double>> residues = {std::complex<double>(-2.0*a, 0.0)};
    
    PoleResidueFilter filter;
    filter.init(poles, residues, 1.0, 0.0, fs);  // constant = 1
    
    // Input: sine wave
    const int n_samples = 10000;
    const double f_in = 100e6;  // 100 MHz
    double input_energy = 0.0;
    double output_energy = 0.0;
    
    for (int i = 0; i < n_samples; ++i) {
        double t = i / fs;
        double input = std::sin(2.0 * M_PI * f_in * t);
        double output = filter.process(input);
        
        input_energy += input * input;
        output_energy += output * output;
    }
    
    double energy_ratio = output_energy / input_energy;
    std::cout << "Input energy: " << input_energy << "\n";
    std::cout << "Output energy: " << output_energy << "\n";
    std::cout << "Energy ratio: " << energy_ratio << " (expected: ~1.0 for all-pass)\n";
    
    // Allow 5% energy error
    if (std::abs(energy_ratio - 1.0) > 0.05) {
        std::cerr << "FAILED: Energy not conserved\n";
        return false;
    }
    
    std::cout << "PASSED\n";
    return true;
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "C++ PoleResidueFilter Test Suite\n";
    std::cout << "========================================\n";
    
    int passed = 0;
    int failed = 0;
    
    if (test_first_order_lpf()) passed++; else failed++;
    if (test_frequency_response()) passed++; else failed++;
    if (test_complex_pole()) passed++; else failed++;
    if (test_energy_conservation()) passed++; else failed++;
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "========================================\n";
    
    return failed > 0 ? 1 : 0;
}
