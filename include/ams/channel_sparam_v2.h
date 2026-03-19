/**
 * @file channel_sparam_v2.h
 * @brief SystemC-AMS wrapper for C++ Pole-Residue Channel Filter
 * 
 * This is the V2 implementation that uses the pure C++ PoleResidueFilter
 * for numerical stability, while maintaining AMS compatibility.
 * 
 * Usage:
 *   ChannelExtendedParams params;
 *   params.method = ChannelMethod::POLE_RESIDUE;
 *   params.config_file = "channel.json";
 *   params.fs = 100e9;
 *   
 *   ChannelSParamV2 channel("channel", params);
 */

#ifndef SERDES_CHANNEL_SPARAM_V2_H
#define SERDES_CHANNEL_SPARAM_V2_H

#include <systemc-ams>
#include <string>
#include <memory>

#include "common/parameters.h"

// Forward declaration of C++ filter
namespace serdes {
namespace cpp {
    class PoleResidueFilter;
}
}

namespace serdes {

/**
 * @brief Channel S-Parameter Model V2 (C++ core + AMS wrapper)
 * 
 * Implements S-parameter channel using VectorFitting pole-residue representation.
 * Uses a pure C++ PoleResidueFilter internally for numerical stability.
 */
class ChannelSParamV2 : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> in;    ///< Input signal from TX
    sca_tdf::sca_out<double> out;  ///< Output signal to RX
    
    /**
     * @brief Constructor
     * @param nm SystemC module name
     * @param ext_params Extended channel parameters (includes config file)
     */
    ChannelSParamV2(sc_core::sc_module_name nm, const ChannelExtendedParams& ext_params);
    
    /**
     * @brief Destructor
     */
    ~ChannelSParamV2();
    
    /**
     * @brief Set TDF attributes (sample rate, etc.)
     */
    void set_attributes() override;
    
    /**
     * @brief Initialize module
     */
    void initialize() override;
    
    /**
     * @brief Process one sample
     */
    void processing() override;
    
    /**
     * @brief Load configuration from JSON file
     * @param config_path Path to JSON config file
     * @return true if successful
     */
    bool load_config(const std::string& config_path);
    
    /**
     * @brief Get DC gain of the channel
     * @return DC gain (linear)
     */
    double get_dc_gain() const;
    
    /**
     * @brief Check if channel is properly initialized
     * @return true if initialized
     */
    bool is_initialized() const;

private:
    ChannelExtendedParams m_params;  ///< Channel parameters
    std::unique_ptr<serdes::cpp::PoleResidueFilter> m_filter;  ///< C++ filter core
    bool m_initialized;              ///< Initialization flag
    
    /**
     * @brief Parse JSON configuration and initialize filter
     * @param json_content JSON string
     * @return true if successful
     */
    bool parse_json_config(const std::string& json_content);
};

} // namespace serdes

#endif // SERDES_CHANNEL_SPARAM_V2_H
