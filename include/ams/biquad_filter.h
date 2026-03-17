#ifndef SERDES_BIQUAD_FILTER_H
#define SERDES_BIQUAD_FILTER_H

#include <systemc-ams>
#include <vector>
#include <complex>
#include <memory>

namespace serdes {

/**
 * Biquad filter section implementing a second-order IIR filter
 * 
 * Transfer function:
 *   H(s) = (b0*s^2 + b1*s + b2) / (s^2 + a1*s + a2)
 * 
 * Converted to discrete-time using bilinear transform:
 *   s = (2/T) * (z-1)/(z+1)
 * 
 * Discrete transfer function:
 *   H(z) = (b0d + b1d*z^-1 + b2d*z^-2) / (1 + a1d*z^-1 + a2d*z^-2)
 */
class BiquadSection {
public:
    /**
     * Constructor
     */
    BiquadSection();
    
    /**
     * Initialize biquad section with continuous-time coefficients
     * @param b0 Coefficient for s^2 term in numerator
     * @param b1 Coefficient for s term in numerator  
     * @param b2 Constant term in numerator
     * @param a1 Coefficient for s term in denominator
     * @param a2 Constant term in denominator
     * @param timestep Sampling period (T)
     */
    void initialize(double b0, double b1, double b2, double a1, double a2, double timestep);
    
    /**
     * Process a single input sample through the biquad filter
     * @param input Input sample value
     * @return Filtered output sample
     */
    double process(double input);
    
    /**
     * Reset filter state (clear delay line)
     */
    void reset();
    
    /**
     * Check if filter has been initialized
     * @return true if initialized
     */
    bool is_initialized() const { return m_initialized; }

private:
    // Continuous-time coefficients
    double m_b0, m_b1, m_b2;
    double m_a1, m_a2;
    
    // Discrete-time coefficients (after bilinear transform)
    double m_b0d, m_b1d, m_b2d;
    double m_a1d, m_a2d;
    
    // State variables (delay line)
    double m_x1, m_x2;  // Previous inputs
    double m_y1, m_y2;  // Previous outputs
    
    // Sampling period
    double m_timestep;
    
    // Initialization flag
    bool m_initialized;
};

/**
 * Pole-residue filter data for channel modeling
 * 
 * Represents transfer function as sum of pole-residue pairs:
 *   H(s) = constant + sum( residue_i / (s - pole_i) )
 */
struct PoleResidueFilterData {
    std::vector<std::complex<double>> poles;      // Poles (can be complex)
    std::vector<std::complex<double>> residues;   // Residues (can be complex)
    double constant = 0.0;                        // Constant term
    int order = 0;                                // Filter order
    double dc_gain = 1.0;                         // DC gain (for verification)
    double mse = 0.0;                             // Fitting error
};

} // namespace serdes

#endif // SERDES_BIQUAD_FILTER_H
