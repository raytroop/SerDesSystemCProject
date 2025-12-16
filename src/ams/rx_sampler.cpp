#include "ams/rx_sampler.h"
#include <cmath>
#include <stdexcept>

namespace serdes {

RxSamplerTdf::RxSamplerTdf(sc_core::sc_module_name nm, const RxSamplerParams& params)
    : sca_tdf::sca_module(nm)
    , in_p("in_p")
    , in_n("in_n")
    , clk_sample("clk_sample")
    , phase_offset("phase_offset")
    , data_out("data_out")
    , data_out_de("data_out_de")
    , m_params(params)
    , m_prev_bit(false)
    , m_rng(params.noise_seed)
    , m_noise_dist(0.0, params.noise_sigma)
    , m_decision_dist(0.0, 1.0)
{
    // Validate parameters during construction
    validate_parameters();
}

void RxSamplerTdf::set_attributes() {
    // Set input and output rates
    in_p.set_rate(1);
    in_n.set_rate(1);
    clk_sample.set_rate(1);
    phase_offset.set_rate(1);
    data_out.set_rate(1);
    data_out_de.set_rate(1);  // DE domain output rate
    
    // Set default timestep if needed
    set_timestep(1.0 / 100e9, sc_core::SC_SEC);  // 100 GHz sampling for high-speed SerDes
}

void RxSamplerTdf::initialize() {
    // Initialize previous bit state
    m_prev_bit = false;
    
    // Reset random number generator with configured seed
    m_rng.seed(m_params.noise_seed);
    m_noise_dist = std::normal_distribution<double>(0.0, m_params.noise_sigma);
}

void RxSamplerTdf::processing() {
    // Step 1: Read differential inputs
    double v_in_p = in_p.read();
    double v_in_n = in_n.read();
    
    // Step 2: Calculate differential voltage
    double v_diff = v_in_p - v_in_n;
    
    // Step 3: Apply offset if enabled
    if (m_params.offset_enable) {
        v_diff += m_params.offset_value;
    }
    
    // Step 4: Inject noise if enabled
    if (m_params.noise_enable) {
        v_diff += m_noise_dist(m_rng);
    }
    
    // Step 5: Make decision based on v_diff
    bool bit_out = make_decision(v_diff);
    
    // Step 6: Update previous bit state
    m_prev_bit = bit_out;
    
    // Step 7: Write outputs
    // TDF domain output (analog-compatible): convert bool to double: 0.0 for false, 1.0 for true
    data_out.write(bit_out ? 1.0 : 0.0);
    
    // DE domain output (discrete event): direct bool output
    data_out_de.write(bit_out);
}

void RxSamplerTdf::validate_parameters() {
    // Check that hysteresis is less than resolution to avoid decision ambiguity
    if (m_params.hysteresis >= m_params.resolution) {
        throw std::invalid_argument(
            "Hysteresis must be less than resolution to avoid decision ambiguity. "
            "Current values: hysteresis = " + std::to_string(m_params.hysteresis) + 
            ", resolution = " + std::to_string(m_params.resolution)
        );
    }
    
    // Validate phase source parameter
    if (m_params.phase_source != "clock" && m_params.phase_source != "phase") {
        throw std::invalid_argument(
            "Phase source must be either 'clock' or 'phase'. "
            "Current value: " + m_params.phase_source
        );
    }
}

bool RxSamplerTdf::make_decision(double v_diff) {
    bool bit_out;
    
    // Check if we're in the fuzzy decision region
    if (std::abs(v_diff) < m_params.resolution) {
        // Fuzzy region: random decision based on Bernoulli distribution
        bit_out = (m_decision_dist(m_rng) < 0.5) ? false : true;
    } else {
        // Deterministic region: hysteresis-based decision
        if (v_diff > m_params.threshold + m_params.hysteresis / 2.0) {
            bit_out = true;
        } else if (v_diff < m_params.threshold - m_params.hysteresis / 2.0) {
            bit_out = false;
        } else {
            bit_out = m_prev_bit;  // Hysteresis: maintain previous state
        }
    }
    
    return bit_out;
}

} // namespace serdes
