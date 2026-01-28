#ifndef SERDES_RX_TOP_H
#define SERDES_RX_TOP_H

#include <systemc-ams>
#include "common/parameters.h"
#include "ams/rx_ctle.h"
#include "ams/rx_vga.h"
#include "ams/rx_dfe.h"
#include "ams/rx_sampler.h"
#include "ams/rx_cdr.h"

namespace serdes {

/**
 * @brief RX Top-level Module
 * 
 * Integrates the complete RX signal chain:
 *   Differential Input → CTLE → VGA → DFE_P/DFE_N → Sampler ↔ CDR → Digital Output
 * 
 * Signal Flow:
 * - External differential input (from Channel or test source)
 * - CTLE: Continuous-Time Linear Equalizer for high-frequency boost
 * - VGA: Variable Gain Amplifier for amplitude adjustment
 * - DFE: Decision Feedback Equalizer (dual instances for differential)
 *   - DFE_P: Positive path with normal taps
 *   - DFE_N: Negative path with negated taps (for differential symmetry)
 * - Sampler: Decision circuit with hysteresis
 * - CDR: Clock and Data Recovery (closed-loop with Sampler)
 * 
 * Key Features:
 * - Dual DFE architecture for differential processing
 * - CDR closed-loop: Sampler.phase_offset ← CDR.phase_out
 * - Phase-driven sampling mode (CDR controls sampling phase)
 * 
 * Note: WaveGen and Channel are NOT included in this module. They should be
 * instantiated externally and connected to the differential input ports.
 */
SC_MODULE(RxTopModule) {
public:
    // ========================================================================
    // External Ports (exposed to parent module)
    // ========================================================================
    
    // Differential input ports - from external source (e.g., Channel)
    sca_tdf::sca_in<double> in_p;   ///< Positive terminal input
    sca_tdf::sca_in<double> in_n;   ///< Negative terminal input
    
    // Power supply input - for PSRR modeling in CTLE/VGA
    sca_tdf::sca_in<double> vdd;
    
    // Digital output port - sampler decision output
    sca_tdf::sca_out<double> data_out;
    
    // ========================================================================
    // Constructor & Destructor
    // ========================================================================
    
    /**
     * @brief Construct RX top module
     * @param nm Module name
     * @param rx_params RX parameters (CTLE, VGA, DFE, Sampler, CDR)
     */
    RxTopModule(sc_core::sc_module_name nm, const RxParams& rx_params);
    
    /**
     * @brief Destructor - clean up sub-modules
     */
    ~RxTopModule();
    
    // ========================================================================
    // Debug Interface
    // ========================================================================
    
    /**
     * @brief Get CTLE output P signal (for debugging/monitoring)
     */
    const sca_tdf::sca_signal<double>& get_ctle_out_p_signal() const { 
        return m_sig_ctle_out_p; 
    }
    
    /**
     * @brief Get CTLE output N signal (for debugging/monitoring)
     */
    const sca_tdf::sca_signal<double>& get_ctle_out_n_signal() const { 
        return m_sig_ctle_out_n; 
    }
    
    /**
     * @brief Get VGA output P signal (for debugging/monitoring)
     */
    const sca_tdf::sca_signal<double>& get_vga_out_p_signal() const { 
        return m_sig_vga_out_p; 
    }
    
    /**
     * @brief Get VGA output N signal (for debugging/monitoring)
     */
    const sca_tdf::sca_signal<double>& get_vga_out_n_signal() const { 
        return m_sig_vga_out_n; 
    }
    
    /**
     * @brief Get DFE output P signal (for debugging/monitoring)
     */
    const sca_tdf::sca_signal<double>& get_dfe_out_p_signal() const { 
        return m_sig_dfe_out_p; 
    }
    
    /**
     * @brief Get DFE output N signal (for debugging/monitoring)
     */
    const sca_tdf::sca_signal<double>& get_dfe_out_n_signal() const { 
        return m_sig_dfe_out_n; 
    }
    
    /**
     * @brief Get current CDR phase output (for debugging/monitoring)
     * @return Raw phase value in seconds
     */
    double get_cdr_phase() const;
    
    /**
     * @brief Get CDR integral state (for debugging/monitoring)
     * @return Integral accumulator value
     */
    double get_cdr_integral_state() const;

private:
    // ========================================================================
    // Sub-modules
    // ========================================================================
    RxCtleTdf* m_ctle;              ///< Continuous-Time Linear Equalizer
    RxVgaTdf* m_vga;                ///< Variable Gain Amplifier
    RxDfeTdf* m_dfe_p;              ///< DFE for P path (normal taps)
    RxDfeTdf* m_dfe_n;              ///< DFE for N path (negated taps)
    RxSamplerTdf* m_sampler;        ///< Decision circuit
    RxCdrTdf* m_cdr;                ///< Clock and Data Recovery
    
    // ========================================================================
    // Internal Signals
    // ========================================================================
    
    // CTLE output → VGA input
    sca_tdf::sca_signal<double> m_sig_ctle_out_p;   ///< CTLE positive output
    sca_tdf::sca_signal<double> m_sig_ctle_out_n;   ///< CTLE negative output
    
    // VGA output → DFE input
    sca_tdf::sca_signal<double> m_sig_vga_out_p;    ///< VGA positive output
    sca_tdf::sca_signal<double> m_sig_vga_out_n;    ///< VGA negative output
    
    // DFE output → Sampler input
    sca_tdf::sca_signal<double> m_sig_dfe_out_p;    ///< DFE_P output
    sca_tdf::sca_signal<double> m_sig_dfe_out_n;    ///< DFE_N output
    
    // Sampler → CDR → Sampler (closed loop)
    sca_tdf::sca_signal<double> m_sig_sampler_out;  ///< Sampler decision output
    sca_tdf::sca_signal<double> m_sig_cdr_phase;    ///< CDR phase output → Sampler
    
    // Dummy clock for Sampler (unused in phase-driven mode but required)
    sca_tdf::sca_signal<double> m_sig_clk;
    
    // ========================================================================
    // Internal Helper Modules
    // ========================================================================
    
    /**
     * @brief Simple constant source for dummy clock signal
     */
    class ConstClockSource : public sca_tdf::sca_module {
    public:
        sca_tdf::sca_out<double> out;
        
        ConstClockSource(sc_core::sc_module_name nm)
            : sca_tdf::sca_module(nm), out("out") {}
        
        void set_attributes() override { out.set_rate(1); }
        void processing() override { out.write(0.0); }
    };
    
    /**
     * @brief Simple pass-through module to duplicate signal to external port
     * Reads from internal signal and writes to output port
     */
    class SignalPassThrough : public sca_tdf::sca_module {
    public:
        sca_tdf::sca_in<double> in;
        sca_tdf::sca_out<double> out;
        
        SignalPassThrough(sc_core::sc_module_name nm)
            : sca_tdf::sca_module(nm), in("in"), out("out") {}
        
        void set_attributes() override { 
            in.set_rate(1); 
            out.set_rate(1); 
        }
        void processing() override { out.write(in.read()); }
    };
    
    ConstClockSource* m_clk_src;        ///< Dummy clock source
    SignalPassThrough* m_data_out_tap;  ///< Pass-through for external data_out
    
    // ========================================================================
    // Parameters
    // ========================================================================
    RxParams m_params;
    RxDfeParams m_dfe_params_n;     ///< Negated taps for DFE_N path
    
    // ========================================================================
    // Private Methods
    // ========================================================================
    
    /**
     * @brief Setup negated DFE taps for N path
     * For differential symmetry: taps_n[i] = -taps_p[i]
     */
    void setup_dfe_negated_taps();
};

} // namespace serdes

#endif // SERDES_RX_TOP_H
