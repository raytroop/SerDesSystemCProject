/**
 * @file pole_residue_filter.h
 * @brief Pole-Residue Filter Implementation (Pure C++, no SystemC-AMS dependency)
 * 
 * Implements S-parameter channel modeling using VectorFitting pole-residue representation:
 * H(s) = sum(r_i / (s - p_i)) + constant + proportional * s
 * 
 * Features:
 * - State-space implementation with bilinear transform (Tustin)
 * - Cascaded second-order sections for numerical stability
 * - Sample-by-sample processing for real-time simulation
 * - Block processing for batch efficiency
 * 
 * Usage:
 *   PoleResidueFilter filter;
 *   filter.init(poles, residues, constant, proportional, fs);
 *   double output = filter.process(input);
 * 
 * @author SerDes Team
 * @version 1.0
 */

#ifndef POLE_RESIDUE_FILTER_H
#define POLE_RESIDUE_FILTER_H

#include <vector>
#include <complex>
#include <memory>

namespace serdes {
namespace cpp {

/**
 * @brief State-space representation of a filter section
 * 
 * Discrete-time state-space:
 *   x[n+1] = Ad * x[n] + Bd * u[n]
 *   y[n]   = Cd * x[n] + Dd * u[n]
 */
struct StateSpaceSection {
    int n_states;                              ///< Number of states (1 or 2)
    std::vector<double> Ad;                    ///< State transition matrix (n_states x n_states)
    std::vector<double> Bd;                    ///< Input matrix (n_states x 1)
    std::vector<double> Cd;                    ///< Output matrix (1 x n_states)
    double Dd;                                 ///< Feedthrough scalar
    std::vector<double> state;                 ///< Current state vector
    
    StateSpaceSection() : n_states(0), Dd(0.0) {}
    
    /**
     * @brief Initialize from continuous-time pole-residue pair
     * 
     * For complex pole: H(s) = r/(s-p) + r* / (s-p*) 
     *                    -> 2-state section
     * For real pole: H(s) = r/(s-p) 
     *                    -> 1-state section
     * 
     * Uses bilinear (Tustin) transform for discretization
     * 
     * @param p Pole (complex)
     * @param r Residue (complex)
     * @param dt Sample time (1/fs)
     */
    void init(const std::complex<double>& p, 
              const std::complex<double>& r, 
              double dt);
    
    /**
     * @brief Process single sample
     * @param input Input sample
     * @return Output sample
     */
    double process(double input);
    
    /**
     * @brief Reset state to zero
     */
    void reset();
};

/**
 * @brief Pole-Residue Filter for S-parameter channel modeling
 * 
 * This class implements a rational transfer function represented by
 * poles and residues, typically obtained from VectorFitting of S-parameters.
 */
class PoleResidueFilter {
public:
    /**
     * @brief Default constructor
     */
    PoleResidueFilter();
    
    /**
     * @brief Destructor
     */
    ~PoleResidueFilter();
    
    /**
     * @brief Initialize filter from pole-residue representation
     * 
     * @param poles Vector of poles (complex, rad/s)
     * @param residues Vector of residues (complex)
     * @param constant Constant term (feedthrough)
     * @param proportional Proportional term (s coefficient, usually ~0 for physical systems)
     * @param fs Sampling frequency (Hz)
     * @return true if initialization successful
     */
    bool init(const std::vector<std::complex<double>>& poles,
              const std::vector<std::complex<double>>& residues,
              double constant,
              double proportional,
              double fs);
    
    /**
     * @brief Initialize from separate real/imag arrays (JSON format)
     * 
     * @param poles_real Real parts of poles
     * @param poles_imag Imaginary parts of poles
     * @param residues_real Real parts of residues
     * @param residues_imag Imaginary parts of residues
     * @param constant Constant term
     * @param proportional Proportional term
     * @param fs Sampling frequency (Hz)
     * @return true if initialization successful
     */
    bool init(const std::vector<double>& poles_real,
              const std::vector<double>& poles_imag,
              const std::vector<double>& residues_real,
              const std::vector<double>& residues_imag,
              double constant,
              double proportional,
              double fs);
    
    /**
     * @brief Process single sample
     * 
     * Use this for real-time simulation where samples arrive one at a time.
     * State is maintained internally between calls.
     * 
     * @param input Input sample
     * @return Output sample
     */
    double process(double input);
    
    /**
     * @brief Process block of samples
     * 
     * More efficient for batch processing (e.g., loading waveform from file).
     * 
     * @param input Input array
     * @param output Output array (must be pre-allocated)
     * @param n_samples Number of samples
     */
    void process_block(const double* input, double* output, int n_samples);
    
    /**
     * @brief Reset all internal states to zero
     */
    void reset();
    
    /**
     * @brief Get frequency response at given frequencies
     * 
     * Computes H(f) = sum(r_i / (j*2*pi*f - p_i)) + constant + proportional*j*2*pi*f
     * 
     * @param frequencies Array of frequencies (Hz)
     * @param n_freqs Number of frequencies
     * @param mag Output magnitude array (optional, can be nullptr)
     * @param phase Output phase array (optional, can be nullptr)
     * @param real Output real part array (optional, can be nullptr)
     * @param imag Output imaginary part array (optional, can be nullptr)
     */
    void get_frequency_response(const double* frequencies, int n_freqs,
                                double* mag, double* phase,
                                double* real, double* imag) const;
    
    /**
     * @brief Get DC gain (H(0))
     * @return DC gain value
     */
    double get_dc_gain() const;
    
    /**
     * @brief Get number of sections
     * @return Number of state-space sections
     */
    int get_num_sections() const { return static_cast<int>(sections_.size()); }
    
    /**
     * @brief Check if filter is initialized
     * @return true if initialized
     */
    bool is_initialized() const { return initialized_; }
    
    /**
     * @brief Get sampling frequency
     * @return Sampling frequency (Hz)
     */
    double get_fs() const { return fs_; }

private:
    std::vector<StateSpaceSection> sections_;  ///< Cascaded state-space sections
    double constant_;                          ///< Constant term (D)
    double proportional_;                      ///< Proportional term (s coefficient)
    double fs_;                                ///< Sampling frequency (Hz)
    double dt_;                                ///< Sample time (1/fs)
    double input_prev_;                        ///< Previous input (for proportional term)
    bool initialized_;                         ///< Initialization flag
    
    // Original poles/residues for frequency response calculation
    std::vector<std::complex<double>> poles_;
    std::vector<std::complex<double>> residues_;
};

} // namespace cpp
} // namespace serdes

#endif // POLE_RESIDUE_FILTER_H
