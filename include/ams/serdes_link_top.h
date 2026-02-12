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
    RxParams rx;                ///< RX parameters (CTLE, VGA, DFE Summer, Sampler, CDR)
    AdaptionParams adaption;    ///< Adaption parameters (AGC, DFE tap adaptation)
    double sample_rate;         ///< Sampling rate (Hz)
    double data_rate;           ///< Data rate (bps), determines UI
    unsigned int seed;          ///< Random seed for PRBS
    
    SerdesLinkParams()
        : sample_rate(640e9)    ///< 640 GHz sampling (64x oversampling for 10G)
        , data_rate(10e9)       ///< 10 Gbps default
        , seed(12345) {
        // Default adaption parameters (disabled)
        adaption.agc.enabled = false;
        adaption.dfe.enabled = false;
        adaption.threshold.enabled = false;
        adaption.cdr_pi.enabled = false;
        adaption.safety.freeze_on_error = false;
        adaption.safety.rollback_enable = false;
    }
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
    
    // Output port - recovered data from RX (digital)
    sca_tdf::sca_out<double> data_out;
    
    // ========================================================================
    // Monitoring Ports - DFE Output (Sampler Input)
    // ========================================================================
    
    /**
     * @brief TX Driver differential output positive (analog)
     * Use this for transmitter eye diagram analysis
     */
    sca_tdf::sca_out<double> mon_tx_out_p;
    
    /**
     * @brief TX Driver differential output negative (analog)
     * Use this for transmitter eye diagram analysis
     */
    sca_tdf::sca_out<double> mon_tx_out_n;
    
    /**
     * @brief DFE Summer differential output positive (Sampler input)
     * Use this for receiver eye diagram analysis
     */
    sca_tdf::sca_out<double> mon_dfe_out_p;
    
    /**
     * @brief DFE Summer differential output negative (Sampler input)
     * Use this for receiver eye diagram analysis
     */
    sca_tdf::sca_out<double> mon_dfe_out_n;
    
    /**
     * @brief VGA differential output positive (before DFE, analog)
     * Use this for receiver eye diagram before DFE compensation
     */
    sca_tdf::sca_out<double> mon_vga_out_p;
    
    /**
     * @brief VGA differential output negative (before DFE, analog)
     * Use this for receiver eye diagram before DFE compensation
     */
    sca_tdf::sca_out<double> mon_vga_out_n;
    
    /**
     * @brief CDR phase adjustment output (seconds)
     * Use this for CDR phase tracking analysis
     */
    sca_tdf::sca_out<double> mon_cdr_phase;
    
    // ========================================================================
    // DFE Tap Monitoring Interface
    // ========================================================================
    
    /**
     * @brief Get DFE tap coefficient (for monitoring)
     * @param tap_index Tap index (1-5)
     * @return Current DFE tap value
     */
    double get_dfe_tap(int tap_index) const;
    
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
    SignalPassThrough* m_tx_tap_p;      ///< Tap for TX output positive monitoring
    SignalPassThrough* m_tx_tap_n;      ///< Tap for TX output negative monitoring
    SignalPassThrough* m_dfe_tap_p;     ///< Tap for DFE output positive monitoring
    SignalPassThrough* m_dfe_tap_n;     ///< Tap for DFE output negative monitoring
    SignalPassThrough* m_vga_tap_p;     ///< Tap for VGA output positive monitoring
    SignalPassThrough* m_vga_tap_n;     ///< Tap for VGA output negative monitoring
    SignalPassThrough* m_cdr_tap;       ///< Tap for CDR phase output monitoring
    
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
