#ifndef SERDES_RX_TOP_H
#define SERDES_RX_TOP_H

#include <systemc-ams>
#include "common/parameters.h"
#include "ams/rx_ctle.h"
#include "ams/rx_vga.h"
#include "ams/rx_dfe_summer.h"
#include "ams/rx_sampler.h"
#include "ams/rx_cdr.h"
#include "ams/adaption.h"

namespace serdes {

/**
 * @brief RX Top-level Module
 * 
 * Integrates the complete RX signal chain:
 *   Differential Input → CTLE → VGA → DFE Summer → Sampler ↔ CDR → Digital Output
 *                                          ↑                    ↓
 *                                     Adaption (DE domain) ←───┘
 * 
 * Signal Flow:
 * - External differential input (from Channel or test source)
 * - CTLE: Continuous-Time Linear Equalizer for high-frequency boost
 * - VGA: Variable Gain Amplifier for amplitude adjustment
 * - DFE Summer: Decision Feedback Equalizer (差分架构，单实例)
 * - Sampler: Decision circuit with hysteresis
 * - CDR: Clock and Data Recovery (closed-loop with Sampler)
 * - Adaption: DE domain adaptive control (AGC, DFE tap update, threshold adaptation)
 * 
 * Key Features:
 * - 差分 DFE Summer 架构替代双 DFE 实例
 * - Adaption 模块集成 (DE 域自适应控制)
 * - DE-TDF 桥接使用 sca_tdf::sca_de::sca_in/out
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
     * @param rx_params RX parameters (CTLE, VGA, DFE Summer, Sampler, CDR)
     * @param adaption_params Adaption parameters (DE 域自适应控制)
     */
    RxTopModule(sc_core::sc_module_name nm, 
                const RxParams& rx_params,
                const AdaptionParams& adaption_params);
    
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
    
    /**
     * @brief Get CDR phase output signal (for monitoring)
     * @return Reference to CDR phase signal
     */
    const sca_tdf::sca_signal<double>& get_cdr_phase_signal() const {
        return m_sig_cdr_phase;
    }
    
    /**
     * @brief Get DFE tap coefficient signals (for monitoring)
     * @param tap_index Tap index (1-5)
     * @return Reference to DFE tap signal
     */
    sc_core::sc_signal<double>& get_dfe_tap_signal(int tap_index) {
        switch (tap_index) {
            case 1: return m_sig_dfe_tap1_de;
            case 2: return m_sig_dfe_tap2_de;
            case 3: return m_sig_dfe_tap3_de;
            case 4: return m_sig_dfe_tap4_de;
            case 5: return m_sig_dfe_tap5_de;
            default: return m_sig_dfe_tap1_de;
        }
    }

private:
    // ========================================================================
    // Sub-modules (TDF domain)
    // ========================================================================
    RxCtleTdf* m_ctle;                  ///< Continuous-Time Linear Equalizer
    RxVgaTdf* m_vga;                    ///< Variable Gain Amplifier
    RxDfeSummerTdf* m_dfe_summer;       ///< 差分 DFE Summer (替代双 DFE)
    RxSamplerTdf* m_sampler;            ///< Decision circuit
    RxCdrTdf* m_cdr;                    ///< Clock and Data Recovery
    
    // ========================================================================
    // Sub-modules (DE domain)
    // ========================================================================
    AdaptionDe* m_adaption;             ///< DE 域自适应控制模块
    
    // ========================================================================
    // Internal TDF Signals
    // ========================================================================
    
    // CTLE output → VGA input
    sca_tdf::sca_signal<double> m_sig_ctle_out_p;   ///< CTLE positive output
    sca_tdf::sca_signal<double> m_sig_ctle_out_n;   ///< CTLE negative output
    
    // VGA output → DFE Summer input
    sca_tdf::sca_signal<double> m_sig_vga_out_p;    ///< VGA positive output
    sca_tdf::sca_signal<double> m_sig_vga_out_n;    ///< VGA negative output
    
    // DFE Summer output → Sampler input
    sca_tdf::sca_signal<double> m_sig_dfe_out_p;    ///< DFE Summer positive output
    sca_tdf::sca_signal<double> m_sig_dfe_out_n;    ///< DFE Summer negative output
    
    // Sampler → CDR → Sampler (closed loop)
    sca_tdf::sca_signal<double> m_sig_sampler_out;  ///< Sampler decision output
    sca_tdf::sca_signal<double> m_sig_cdr_phase;    ///< CDR phase output (for monitoring)
    sca_tdf::sca_signal<double> m_sig_cdr_in;       ///< CDR input (from splitter)
    sca_tdf::sca_signal<bool> m_sig_sampling_trigger; ///< CDR sampling trigger → Sampler
    
    // Sampler → DFE Summer (历史判决反馈)
    sca_tdf::sca_signal<double> m_sig_data_feedback; ///< Data feedback for DFE
    
    // Dummy clock for Sampler (unused in phase-driven mode but required)
    sca_tdf::sca_signal<double> m_sig_clk;
    
    // ========================================================================
    // DE-TDF Bridge Signals (Adaption ↔ TDF modules)
    // ========================================================================
    
    // Adaption outputs (DE domain) → TDF modules
    sc_core::sc_signal<double> m_sig_vga_gain_de;       ///< VGA gain control
    sc_core::sc_signal<double> m_sig_dfe_tap1_de;       ///< DFE tap 1
    sc_core::sc_signal<double> m_sig_dfe_tap2_de;       ///< DFE tap 2
    sc_core::sc_signal<double> m_sig_dfe_tap3_de;       ///< DFE tap 3
    sc_core::sc_signal<double> m_sig_dfe_tap4_de;       ///< DFE tap 4
    sc_core::sc_signal<double> m_sig_dfe_tap5_de;       ///< DFE tap 5
    sc_core::sc_signal<double> m_sig_sampler_threshold_de;  ///< Sampler threshold
    sc_core::sc_signal<double> m_sig_sampler_hysteresis_de; ///< Sampler hysteresis
    sc_core::sc_signal<double> m_sig_phase_cmd_de;      ///< Phase command
    
    // TDF modules → Adaption inputs (DE domain)
    sc_core::sc_signal<double> m_sig_phase_error_de;    ///< Phase error from CDR
    sc_core::sc_signal<double> m_sig_amplitude_rms_de;  ///< Amplitude RMS
    sc_core::sc_signal<int> m_sig_error_count_de;       ///< Error count
    sc_core::sc_signal<double> m_sig_isi_metric_de;     ///< ISI metric
    sc_core::sc_signal<int> m_sig_mode_de;              ///< Operating mode
    sc_core::sc_signal<bool> m_sig_reset_de;            ///< Reset signal
    sc_core::sc_signal<double> m_sig_scenario_switch_de; ///< Scenario switch
    sc_core::sc_signal<bool> m_sig_sampler_data_out_de; ///< Sampler DE output
    
    // Adaption 其他输出信号
    sc_core::sc_signal<double> m_sig_ctle_zero_de;
    sc_core::sc_signal<double> m_sig_ctle_pole_de;
    sc_core::sc_signal<double> m_sig_ctle_dc_gain_de;
    sc_core::sc_signal<double> m_sig_dfe_tap6_de;
    sc_core::sc_signal<double> m_sig_dfe_tap7_de;
    sc_core::sc_signal<double> m_sig_dfe_tap8_de;
    sc_core::sc_signal<int> m_sig_update_count_de;
    sc_core::sc_signal<bool> m_sig_freeze_flag_de;
    
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
    
    /**
     * @brief Signal splitter - reads one signal and writes to two outputs
     * Used to split sampler output to CDR and data feedback
     */
    class SignalSplitter : public sca_tdf::sca_module {
    public:
        sca_tdf::sca_in<double> in;
        sca_tdf::sca_out<double> out1;
        sca_tdf::sca_out<double> out2;
        
        SignalSplitter(sc_core::sc_module_name nm)
            : sca_tdf::sca_module(nm), in("in"), out1("out1"), out2("out2") {}
        
        void set_attributes() override { 
            in.set_rate(1); 
            out1.set_rate(1); 
            out2.set_rate(1);
        }
        void processing() override { 
            double val = in.read();
            out1.write(val); 
            out2.write(val);
        }
    };
    
    ConstClockSource* m_clk_src;            ///< Dummy clock source
    SignalPassThrough* m_data_out_tap;      ///< Pass-through for external data_out
    SignalSplitter* m_sampler_splitter;     ///< Splitter for sampler output
    
    // ========================================================================
    // Parameters
    // ========================================================================
    RxParams m_params;
    AdaptionParams m_adaption_params;
};

} // namespace serdes

#endif // SERDES_RX_TOP_H
