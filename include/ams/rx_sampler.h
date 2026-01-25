#ifndef SERDES_RX_SAMPLER_H
#define SERDES_RX_SAMPLER_H
#include <systemc-ams>
#include "common/parameters.h"
#include <random>

namespace serdes {

/**
 * @brief Sampler module for SerDes receiver
 * 
 * Features:
 * - Differential input/output (in_p, in_n, data_out)
 * - Clock-driven or phase-driven sampling
 * - Hysteresis-based decision with Schmitt trigger effect
 * - Fuzzy decision mechanism for resolution region
 * - Offset and noise injection
 * - Parameter validation
 */
class RxSamplerTdf : public sca_tdf::sca_module {
public:
    // Differential inputs
    sca_tdf::sca_in<double> in_p;
    sca_tdf::sca_in<double> in_n;
    
    // Clock or phase inputs (select based on phase_source parameter)
    sca_tdf::sca_in<double> clk_sample;  // Clock input for clock-driven mode
    sca_tdf::sca_in<double> phase_offset;  // Phase offset for phase-driven mode
    
    // Digital outputs
    sca_tdf::sca_out<double> data_out;      // TDF domain output (analog-compatible)
    sca_tdf::sca_de::sca_out<bool> data_out_de;  // TDF to DE domain bridge output
    
    /**
     * @brief Constructor
     * @param nm Module name
     * @param params Sampler parameters
     * @note Initializes data_out_de port
     */
    RxSamplerTdf(sc_core::sc_module_name nm, const RxSamplerParams& params);
    
    /**
     * @brief Set TDF module attributes
     */
    void set_attributes();
    
    /**
     * @brief Initialize sampler internal states
     */
    void initialize();
    
    /**
     * @brief Main processing function
     */
    void processing();
    
private:
    RxSamplerParams m_params;
    
    // Internal states
    bool m_prev_bit;
    
    // Random number generator for noise and fuzzy decision
    std::mt19937 m_rng;
    std::normal_distribution<double> m_noise_dist;
    std::uniform_real_distribution<double> m_decision_dist;
    
    /**
     * @brief Validate sampler parameters
     * @throws std::invalid_argument if parameters are invalid
     */
    void validate_parameters();
    
    /**
     * @brief Make decision based on input voltage
     * @param v_diff Differential input voltage
     * @return Decision result (true for high, false for low)
     */
    bool make_decision(double v_diff);
};
}
#endif
