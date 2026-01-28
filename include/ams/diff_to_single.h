#ifndef SERDES_DIFF_TO_SINGLE_H
#define SERDES_DIFF_TO_SINGLE_H

#include <systemc-ams>

namespace serdes {

/**
 * @brief Differential to Single-ended Converter TDF Module
 * 
 * Converts a differential signal pair to single-ended signal:
 * - out = in_p - in_n (differential to single-ended conversion)
 * 
 * This module is used to connect differential output modules (e.g., TxDriver)
 * to single-ended input modules (e.g., Channel).
 */
class DiffToSingleTdf : public sca_tdf::sca_module {
public:
    // Input ports - differential pair
    sca_tdf::sca_in<double> in_p;   ///< Positive terminal input
    sca_tdf::sca_in<double> in_n;   ///< Negative terminal input
    
    // Output port - single-ended signal
    sca_tdf::sca_out<double> out;   ///< Single-ended output (in_p - in_n)
    
    /**
     * @brief Constructor
     * @param nm Module name
     */
    DiffToSingleTdf(sc_core::sc_module_name nm);
    
    // TDF callback methods
    void set_attributes() override;
    void processing() override;
};

} // namespace serdes

#endif // SERDES_DIFF_TO_SINGLE_H
