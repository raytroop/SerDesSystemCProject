/**
 * @file pole_residue_filter.cpp
 * @brief Implementation of Pole-Residue Filter
 */

#include "pole_residue_filter.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace serdes {
namespace cpp {

// ============================================================================
// StateSpaceSection Implementation
// ============================================================================

void StateSpaceSection::init(const std::complex<double>& p, 
                              const std::complex<double>& r, 
                              double dt) {
    const double pr = p.real();
    const double pi = p.imag();
    const double rr = r.real();
    const double ri = r.imag();
    
    if (std::abs(pi) > 1e-12) {
        // Complex pole: H(s) = r/(s-p) + r*/(s-p*)
        // Convert to second-order section: H(s) = (b1*s + b0) / (s^2 + a1*s + a0)
        // 
        // where:
        //   b1 = 2*Re(r) = 2*rr
        //   b0 = -2*Re(r * conj(p)) = -2*(rr*pr + ri*pi)
        //   a1 = -2*Re(p) = -2*pr
        //   a0 = |p|^2 = pr^2 + pi^2
        
        const double b1 = 2.0 * rr;
        const double b0 = -2.0 * (rr * pr + ri * pi);
        const double a1 = -2.0 * pr;
        const double a0 = pr * pr + pi * pi;
        
        // Continuous-time state-space (controllable canonical form):
        // Ac = [0, 1; -a0, -a1]
        // Bc = [0; 1]
        // Cc = [b0, b1]
        // Dc = 0
        
        // Discretize using bilinear (Tustin) transform:
        // Ad = (I - Ac*dt/2)^-1 * (I + Ac*dt/2)
        // Bd = (I - Ac*dt/2)^-1 * Bc * dt
        // Cd = Cc * (I - Ac*dt/2)^-1
        // Dd = Dc + Cc * (I - Ac*dt/2)^-1 * Bc * dt/2
        
        // (I - Ac*dt/2)
        const double i00 = 1.0;
        const double i01 = -dt / 2.0;
        const double i10 = a0 * dt / 2.0;
        const double i11 = 1.0 + a1 * dt / 2.0;
        
        // Inverse of (I - Ac*dt/2)
        const double det = i00 * i11 - i01 * i10;
        if (std::abs(det) < 1e-15) {
            std::cerr << "Warning: Singular matrix in state-space discretization\n";
            n_states = 0;
            return;
        }
        const double det_inv = 1.0 / det;
        const double inv00 = i11 * det_inv;
        const double inv01 = -i01 * det_inv;
        const double inv10 = -i10 * det_inv;
        const double inv11 = i00 * det_inv;
        
        // (I + Ac*dt/2)
        const double p00 = 1.0;
        const double p01 = dt / 2.0;
        const double p10 = -a0 * dt / 2.0;
        const double p11 = 1.0 - a1 * dt / 2.0;
        
        // Ad = inv(I - Ac*dt/2) * (I + Ac*dt/2)
        Ad.resize(4);
        Ad[0] = inv00 * p00 + inv01 * p10;
        Ad[1] = inv00 * p01 + inv01 * p11;
        Ad[2] = inv10 * p00 + inv11 * p10;
        Ad[3] = inv10 * p01 + inv11 * p11;
        
        // Bd = inv(I - Ac*dt/2) * Bc * dt
        // Bc = [0; 1]
        Bd.resize(2);
        Bd[0] = inv01 * dt;
        Bd[1] = inv11 * dt;
        
        // Cd = Cc * inv(I - Ac*dt/2)
        // Cc = [b0, b1]
        Cd.resize(2);
        Cd[0] = b0 * inv00 + b1 * inv10;
        Cd[1] = b0 * inv01 + b1 * inv11;
        
        // Dd = Dc + Cc * inv(I - Ac*dt/2) * Bc * dt/2
        //    = 0 + [b0, b1] * [inv01; inv11] * dt/2
        Dd = (b0 * inv01 + b1 * inv11) * dt / 2.0;
        
        n_states = 2;
        state.resize(2, 0.0);
        
    } else {
        // Real pole: H(s) = r / (s - p)
        // Continuous-time: dx/dt = p*x + u, y = r*x
        // (using unity input for Bc, output scaled by r in Cc)
        
        // Discretize:
        // Ad = (1 + p*dt/2) / (1 - p*dt/2)
        // Bd = dt / (1 - p*dt/2)
        // Cd = r / (1 - p*dt/2)
        // Dd = r * dt / (2 * (1 - p*dt/2))
        
        const double denom = 1.0 - pr * dt / 2.0;
        if (std::abs(denom) < 1e-15) {
            std::cerr << "Warning: Singular denominator in real pole discretization\n";
            n_states = 0;
            return;
        }
        
        Ad.resize(1);
        Ad[0] = (1.0 + pr * dt / 2.0) / denom;
        
        Bd.resize(1);
        Bd[0] = dt / denom;
        
        Cd.resize(1);
        Cd[0] = rr / denom;
        
        Dd = rr * dt / (2.0 * denom);
        
        n_states = 1;
        state.resize(1, 0.0);
    }
}

double StateSpaceSection::process(double input) {
    if (n_states == 0) return input;
    
    // Compute output: y = Cd * x + Dd * u
    double output = Dd * input;
    for (int i = 0; i < n_states; ++i) {
        output += Cd[i] * state[i];
    }
    
    // Update state: x[n+1] = Ad * x[n] + Bd * u[n]
    std::vector<double> new_state(n_states);
    for (int i = 0; i < n_states; ++i) {
        new_state[i] = Bd[i] * input;
        for (int j = 0; j < n_states; ++j) {
            new_state[i] += Ad[i * n_states + j] * state[j];
        }
    }
    
    state = new_state;
    return output;
}

void StateSpaceSection::reset() {
    std::fill(state.begin(), state.end(), 0.0);
}

// ============================================================================
// PoleResidueFilter Implementation
// ============================================================================

PoleResidueFilter::PoleResidueFilter()
    : constant_(0.0)
    , proportional_(0.0)
    , fs_(0.0)
    , dt_(0.0)
    , input_prev_(0.0)
    , initialized_(false)
{
}

PoleResidueFilter::~PoleResidueFilter() = default;

bool PoleResidueFilter::init(const std::vector<std::complex<double>>& poles,
                              const std::vector<std::complex<double>>& residues,
                              double constant,
                              double proportional,
                              double fs) {
    if (poles.size() != residues.size()) {
        std::cerr << "Error: Poles and residues must have same size\n";
        return false;
    }
    
    if (fs <= 0.0) {
        std::cerr << "Error: Sampling frequency must be positive\n";
        return false;
    }
    
    // Store original parameters
    poles_ = poles;
    residues_ = residues;
    constant_ = constant;
    proportional_ = proportional;
    fs_ = fs;
    dt_ = 1.0 / fs;
    input_prev_ = 0.0;
    
    // Clear existing sections
    sections_.clear();
    
    // Calculate Nyquist frequency for pole filtering
    const double nyquist_freq = fs / 2.0;
    const double max_pole_freq = 0.95 * nyquist_freq;  // Allow up to 95% of Nyquist
    
    // Create state-space sections for each pole
    // Note: scikit-rf only outputs upper half-plane poles (imag > 0)
    // We handle the conjugate pair internally in StateSpaceSection
    for (size_t i = 0; i < poles.size(); ++i) {
        const double pole_freq = std::abs(poles[i]) / (2.0 * M_PI);
        
        if (pole_freq > max_pole_freq) {
            std::cout << "[PoleResidueFilter] Skipping pole " << i 
                      << " (freq=" << pole_freq/1e9 << " GHz > max)\n";
            continue;
        }
        
        StateSpaceSection section;
        section.init(poles[i], residues[i], dt_);
        
        if (section.n_states > 0) {
            sections_.push_back(std::move(section));
        }
    }
    
    std::cout << "[PoleResidueFilter] Initialized with " << sections_.size() 
              << " sections (from " << poles.size() << " poles)\n";
    std::cout << "[PoleResidueFilter] Constant=" << constant_ 
              << ", Proportional=" << proportional_ << "\n";
    
    initialized_ = !sections_.empty() || std::abs(constant_) > 1e-15;
    return initialized_;
}

bool PoleResidueFilter::init(const std::vector<double>& poles_real,
                              const std::vector<double>& poles_imag,
                              const std::vector<double>& residues_real,
                              const std::vector<double>& residues_imag,
                              double constant,
                              double proportional,
                              double fs) {
    if (poles_real.size() != poles_imag.size() ||
        poles_real.size() != residues_real.size() ||
        poles_real.size() != residues_imag.size()) {
        std::cerr << "Error: All pole/residue arrays must have same size\n";
        return false;
    }
    
    std::vector<std::complex<double>> poles(poles_real.size());
    std::vector<std::complex<double>> residues(residues_real.size());
    
    for (size_t i = 0; i < poles.size(); ++i) {
        poles[i] = std::complex<double>(poles_real[i], poles_imag[i]);
        residues[i] = std::complex<double>(residues_real[i], residues_imag[i]);
    }
    
    return init(poles, residues, constant, proportional, fs);
}

double PoleResidueFilter::process(double input) {
    if (!initialized_) {
        return input;
    }
    
    // Start with constant term (feedthrough)
    double output = constant_ * input;
    
    // Add proportional term (differentiator approximation)
    if (std::abs(proportional_) > 1e-20) {
        output += proportional_ * (input - input_prev_) / dt_;
    }
    input_prev_ = input;
    
    // Cascade through all sections
    double signal = input;
    for (auto& section : sections_) {
        signal = section.process(signal);
    }
    output += signal;
    
    return output;
}

void PoleResidueFilter::process_block(const double* input, double* output, int n_samples) {
    for (int i = 0; i < n_samples; ++i) {
        output[i] = process(input[i]);
    }
}

void PoleResidueFilter::reset() {
    for (auto& section : sections_) {
        section.reset();
    }
    input_prev_ = 0.0;
}

void PoleResidueFilter::get_frequency_response(const double* frequencies, int n_freqs,
                                                double* mag, double* phase,
                                                double* real, double* imag) const {
    for (int i = 0; i < n_freqs; ++i) {
        const double f = frequencies[i];
        const std::complex<double> s(0.0, 2.0 * M_PI * f);
        
        // H(s) = sum(r / (s - p)) + constant + proportional * s
        std::complex<double> H(constant_, 0.0);
        
        // Add proportional term
        if (std::abs(proportional_) > 1e-20) {
            H += proportional_ * s;
        }
        
        // Add pole-residue terms
        for (size_t j = 0; j < poles_.size(); ++j) {
            H += residues_[j] / (s - poles_[j]);
        }
        
        // Output requested formats
        if (mag) mag[i] = std::abs(H);
        if (phase) phase[i] = std::arg(H);
        if (real) real[i] = H.real();
        if (imag) imag[i] = H.imag();
    }
}

double PoleResidueFilter::get_dc_gain() const {
    if (!initialized_) return 1.0;
    
    // H(0) = sum(-r/p) + constant
    std::complex<double> H(constant_, 0.0);
    for (size_t i = 0; i < poles_.size(); ++i) {
        if (std::abs(poles_[i]) > 1e-15) {
            H += -residues_[i] / poles_[i];
        }
    }
    return H.real();  // Should be real for physical systems
}

} // namespace cpp
} // namespace serdes
