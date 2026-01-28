#ifndef SERDES_LINK_TOP_H
#define SERDES_LINK_TOP_H

#include <systemc-ams>
#include "common/parameters.h"
#include "ams/wave_generation.h"
#include "ams/tx_top.h"
#include "ams/diff_to_single.h"
#include "ams/channel_sparam.h"
#include "ams/single_to_diff.h"
#include "ams/rx_top.h"

namespace serdes {

/**
 * @brief SerDes Link Parameters
 * 
 * Complete parameter structure for the entire SerDes link.
 */
struct SerdesLinkParams {
    WaveGenParams wave;         ///< Wave generation parameters
    TxParams tx;                ///< TX parameters (FFE, Mux, Driver)
    ChannelParams channel;      ///< Channel parameters
    RxParams rx;                ///< RX parameters (CTLE, VGA, DFE, Sampler, CDR)
    double sample_rate;         ///< Sampling rate (Hz)
    unsigned int seed;          ///< Random seed for PRBS
    
    SerdesLinkParams()
        : sample_rate(100e9)
        , seed(12345) {}
};

/**
 * @brief SerDes Link Top-level Module
 * 
 * Integrates the complete SerDes link:
 *   WaveGen -> TxTop -> DiffToSingle -> Channel -> SingleToDiff -> RxTop
 * 
 * Signal Flow:
 * - WaveGen: Generates PRBS/pulse test patterns
 * - TxTop: FFE pre-emphasis + Driver (single-ended to differential)
 * - DiffToSingle: Converts TX differential output to single-ended for channel
 * - Channel: S-parameter based channel model (attenuation + bandwidth limiting)
 * - SingleToDiff: Converts channel output to differential for RX
 * - RxTop: CTLE + VGA + DFE + Sampler + CDR (recovers data)
 * 
 * This module provides a complete end-to-end SerDes simulation in a single
 * instantiation, suitable for system-level verification.
 */
SC_MODULE(SerdesLinkTopModule) {
public:
    // ========================================================================
    // External Ports
    // ========================================================================
    
    // Power supply input - for PSRR modeling in TX/RX
    sca_tdf::sca_in<double> vdd;
    
    // Output port - recovered data from RX
    sca_tdf::sca_out<double> data_out;
    
    // ========================================================================
    // Constructor & Destructor
    // ========================================================================
    
    /**
     * @brief Construct SerDes Link top module
     * @param nm Module name
     * @param params SerDes link parameters
     */
    SerdesLinkTopModule(sc_core::sc_module_name nm, const SerdesLinkParams& params);
    
    /**
     * @brief Destructor - clean up sub-modules
     */
    ~SerdesLinkTopModule();
    
    // ========================================================================
    // Debug Interface - TX Signals
    // ========================================================================
    
    /**
     * @brief Get WaveGen output signal (TX input)
     */
    const sca_tdf::sca_signal<double>& get_wavegen_out_signal() const {
        return m_sig_wavegen_out;
    }
    
    /**
     * @brief Get TX differential output positive signal
     */
    const sca_tdf::sca_signal<double>& get_tx_out_p_signal() const {
        return m_sig_tx_out_p;
    }
    
    /**
     * @brief Get TX differential output negative signal
     */
    const sca_tdf::sca_signal<double>& get_tx_out_n_signal() const {
        return m_sig_tx_out_n;
    }
    
    // ========================================================================
    // Debug Interface - Channel Signals
    // ========================================================================
    
    /**
     * @brief Get Channel input signal (after DiffToSingle conversion)
     */
    const sca_tdf::sca_signal<double>& get_channel_in_signal() const {
        return m_sig_channel_in;
    }
    
    /**
     * @brief Get Channel output signal (before SingleToDiff conversion)
     */
    const sca_tdf::sca_signal<double>& get_channel_out_signal() const {
        return m_sig_channel_out;
    }
    
    // ========================================================================
    // Debug Interface - RX Input Signals
    // ========================================================================
    
    /**
     * @brief Get RX differential input positive signal
     */
    const sca_tdf::sca_signal<double>& get_rx_in_p_signal() const {
        return m_sig_rx_in_p;
    }
    
    /**
     * @brief Get RX differential input negative signal
     */
    const sca_tdf::sca_signal<double>& get_rx_in_n_signal() const {
        return m_sig_rx_in_n;
    }
    
    // ========================================================================
    // Debug Interface - RX Internal Signals (forwarded from RxTopModule)
    // ========================================================================
    
    /**
     * @brief Get RX CTLE output positive signal
     */
    const sca_tdf::sca_signal<double>& get_rx_ctle_out_p_signal() const {
        return m_rx->get_ctle_out_p_signal();
    }
    
    /**
     * @brief Get RX CTLE output negative signal
     */
    const sca_tdf::sca_signal<double>& get_rx_ctle_out_n_signal() const {
        return m_rx->get_ctle_out_n_signal();
    }
    
    /**
     * @brief Get RX VGA output positive signal
     */
    const sca_tdf::sca_signal<double>& get_rx_vga_out_p_signal() const {
        return m_rx->get_vga_out_p_signal();
    }
    
    /**
     * @brief Get RX VGA output negative signal
     */
    const sca_tdf::sca_signal<double>& get_rx_vga_out_n_signal() const {
        return m_rx->get_vga_out_n_signal();
    }
    
    /**
     * @brief Get RX DFE output positive signal
     */
    const sca_tdf::sca_signal<double>& get_rx_dfe_out_p_signal() const {
        return m_rx->get_dfe_out_p_signal();
    }
    
    /**
     * @brief Get RX DFE output negative signal
     */
    const sca_tdf::sca_signal<double>& get_rx_dfe_out_n_signal() const {
        return m_rx->get_dfe_out_n_signal();
    }
    
    // ========================================================================
    // Debug Interface - CDR State
    // ========================================================================
    
    /**
     * @brief Get current CDR phase output
     * @return Raw phase value in seconds
     */
    double get_cdr_phase() const {
        return m_rx->get_cdr_phase();
    }
    
    /**
     * @brief Get CDR integral state
     * @return Integral accumulator value
     */
    double get_cdr_integral_state() const {
        return m_rx->get_cdr_integral_state();
    }
    
    // ========================================================================
    // Parameter Access
    // ========================================================================
    
    /**
     * @brief Get link parameters (read-only)
     */
    const SerdesLinkParams& get_params() const { return m_params; }

private:
    // ========================================================================
    // Sub-modules
    // ========================================================================
    WaveGenerationTdf* m_wavegen;       ///< Wave generation (PRBS/pulse)
    TxTopModule* m_tx;                  ///< TX chain (FFE + Driver)
    DiffToSingleTdf* m_d2s;             ///< Differential to single-ended converter
    ChannelSParamTdf* m_channel;        ///< Channel model
    SingleToDiffTdf* m_s2d;             ///< Single-ended to differential converter
    RxTopModule* m_rx;                  ///< RX chain (CTLE + VGA + DFE + Sampler + CDR)
    
    // ========================================================================
    // Internal Helper Modules
    // ========================================================================
    
    /**
     * @brief Simple pass-through module to duplicate signal to external port
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
    
    SignalPassThrough* m_data_out_tap;  ///< Pass-through for external data_out
    
    // ========================================================================
    // Internal Signals
    // ========================================================================
    
    // WaveGen -> TX
    sca_tdf::sca_signal<double> m_sig_wavegen_out;
    
    // TX -> DiffToSingle
    sca_tdf::sca_signal<double> m_sig_tx_out_p;
    sca_tdf::sca_signal<double> m_sig_tx_out_n;
    
    // DiffToSingle -> Channel
    sca_tdf::sca_signal<double> m_sig_channel_in;
    
    // Channel -> SingleToDiff
    sca_tdf::sca_signal<double> m_sig_channel_out;
    
    // SingleToDiff -> RX
    sca_tdf::sca_signal<double> m_sig_rx_in_p;
    sca_tdf::sca_signal<double> m_sig_rx_in_n;
    
    // RX data output (internal)
    sca_tdf::sca_signal<double> m_sig_data_out;
    
    // ========================================================================
    // Parameters
    // ========================================================================
    SerdesLinkParams m_params;
};

} // namespace serdes

#endif // SERDES_LINK_TOP_H
