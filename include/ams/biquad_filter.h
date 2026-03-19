#ifndef SERDES_BIQUAD_FILTER_H
#define SERDES_BIQUAD_FILTER_H

#include <vector>
#include <complex>

namespace serdes {

/**
 * Biquad filter section - Direct Form II implementation
 * 
 * H(s) = (b0 + b1*s) / (a0 + a1*s + a2*s^2)
 * 
 * Converted to discrete-time using bilinear transform
 */
class BiquadSection {
public:
    BiquadSection();
    
    /**
     * Initialize with continuous-time coefficients
     * H(s) = (b0 + b1*s) / (a0 + a1*s + a2*s^2)
     */
    void initialize(double b0, double b1, double a0, double a1, double a2, double timestep);
    
    double process(double input);
    void reset();
    bool is_initialized() const { return m_initialized; }

private:
    // Discrete-time coefficients (Direct Form II)
    double m_b0d, m_b1d, m_b2d;
    double m_a1d, m_a2d;
    
    // State variables
    double m_w1, m_w2;
    
    bool m_initialized;
};

/**
 * Pole-residue filter data
 */
struct PoleResidueFilterData {
    std::vector<double> poles_real;
    std::vector<double> poles_imag;
    std::vector<double> residues_real;
    std::vector<double> residues_imag;
    double constant = 0.0;
    double proportional = 0.0;
    int order = 0;
    double dc_gain = 1.0;
    double mse = 0.0;
};

} // namespace serdes

#endif // SERDES_BIQUAD_FILTER_H
