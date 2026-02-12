#include "ams/rx_top.h"
#include <iostream>

namespace serdes {

RxTopModule::RxTopModule(sc_core::sc_module_name nm, 
                         const RxParams& rx_params,
                         const AdaptionParams& adaption_params)
    : sc_module(nm)
    , in_p("in_p")
    , in_n("in_n")
    , vdd("vdd")
    , data_out("data_out")
    // TDF signals
    , m_sig_ctle_out_p("sig_ctle_out_p")
    , m_sig_ctle_out_n("sig_ctle_out_n")
    , m_sig_vga_out_p("sig_vga_out_p")
    , m_sig_vga_out_n("sig_vga_out_n")
    , m_sig_dfe_out_p("sig_dfe_out_p")
    , m_sig_dfe_out_n("sig_dfe_out_n")
    , m_sig_sampler_out("sig_sampler_out")
    , m_sig_cdr_phase("sig_cdr_phase")
    , m_sig_cdr_in("sig_cdr_in")
    , m_sig_data_feedback("sig_data_feedback")
    , m_sig_clk("sig_clk")
    // DE-TDF bridge signals (Adaption outputs)
    , m_sig_vga_gain_de("sig_vga_gain_de")
    , m_sig_dfe_tap1_de("sig_dfe_tap1_de")
    , m_sig_dfe_tap2_de("sig_dfe_tap2_de")
    , m_sig_dfe_tap3_de("sig_dfe_tap3_de")
    , m_sig_dfe_tap4_de("sig_dfe_tap4_de")
    , m_sig_dfe_tap5_de("sig_dfe_tap5_de")
    , m_sig_sampler_threshold_de("sig_sampler_threshold_de")
    , m_sig_sampler_hysteresis_de("sig_sampler_hysteresis_de")
    , m_sig_phase_cmd_de("sig_phase_cmd_de")
    // DE-TDF bridge signals (Adaption inputs)
    , m_sig_phase_error_de("sig_phase_error_de")
    , m_sig_amplitude_rms_de("sig_amplitude_rms_de")
    , m_sig_error_count_de("sig_error_count_de")
    , m_sig_isi_metric_de("sig_isi_metric_de")
    , m_sig_mode_de("sig_mode_de")
    , m_sig_reset_de("sig_reset_de")
    , m_sig_scenario_switch_de("sig_scenario_switch_de")
    , m_sig_sampler_data_out_de("sig_sampler_data_out_de")
    // Other Adaption outputs
    , m_sig_ctle_zero_de("sig_ctle_zero_de")
    , m_sig_ctle_pole_de("sig_ctle_pole_de")
    , m_sig_ctle_dc_gain_de("sig_ctle_dc_gain_de")
    , m_sig_dfe_tap6_de("sig_dfe_tap6_de")
    , m_sig_dfe_tap7_de("sig_dfe_tap7_de")
    , m_sig_dfe_tap8_de("sig_dfe_tap8_de")
    , m_sig_update_count_de("sig_update_count_de")
    , m_sig_freeze_flag_de("sig_freeze_flag_de")
    // Parameters
    , m_params(rx_params)
    , m_adaption_params(adaption_params)
{
    // ========================================================================
    // Instantiate sub-modules (TDF domain)
    // ========================================================================
    
    // CTLE: Continuous-Time Linear Equalizer
    m_ctle = new RxCtleTdf("ctle", m_params.ctle);
    
    // VGA: Variable Gain Amplifier
    m_vga = new RxVgaTdf("vga", m_params.vga);
    
    // DFE Summer: 差分 DFE 求和器 (替代双 DFE)
    m_dfe_summer = new RxDfeSummerTdf("dfe_summer", m_params.dfe_summer);
    
    // Sampler: Decision circuit
    // Configure for phase-driven mode (CDR controls sampling phase)
    RxSamplerParams sampler_params = m_params.sampler;
    sampler_params.phase_source = "phase";  // Force phase-driven mode
    m_sampler = new RxSamplerTdf("sampler", sampler_params);
    
    // CDR: Clock and Data Recovery
    m_cdr = new RxCdrTdf("cdr", m_params.cdr);
    
    // ========================================================================
    // Instantiate sub-modules (DE domain)
    // ========================================================================
    
    // Adaption: DE 域自适应控制模块
    m_adaption = new AdaptionDe("adaption", m_adaption_params);
    
    // ========================================================================
    // Instantiate helper modules
    // ========================================================================
    
    // Clock source: Dummy clock for Sampler's clk_sample port
    // (Required for port connection but not used in phase-driven mode)
    m_clk_src = new ConstClockSource("clk_src");
    
    // Data output tap: Pass-through module to drive external data_out port
    m_data_out_tap = new SignalPassThrough("data_out_tap");
    
    // Sampler splitter: Split sampler output to CDR and data feedback
    m_sampler_splitter = new SignalSplitter("sampler_splitter");
    
    // ========================================================================
    // Connect TDF signal chain
    // ========================================================================
    
    // External input → CTLE input (differential)
    m_ctle->in_p(in_p);
    m_ctle->in_n(in_n);
    m_ctle->vdd(vdd);
    
    // CTLE output → VGA input
    m_ctle->out_p(m_sig_ctle_out_p);
    m_ctle->out_n(m_sig_ctle_out_n);
    m_vga->in_p(m_sig_ctle_out_p);
    m_vga->in_n(m_sig_ctle_out_n);
    m_vga->vdd(vdd);
    
    // VGA output → DFE Summer input (differential)
    m_vga->out_p(m_sig_vga_out_p);
    m_vga->out_n(m_sig_vga_out_n);
    m_dfe_summer->in_p(m_sig_vga_out_p);
    m_dfe_summer->in_n(m_sig_vga_out_n);
    
    // DFE Summer output → Sampler input (differential)
    m_dfe_summer->out_p(m_sig_dfe_out_p);
    m_dfe_summer->out_n(m_sig_dfe_out_n);
    m_sampler->in_p(m_sig_dfe_out_p);
    m_sampler->in_n(m_sig_dfe_out_n);
    
    // Sampler control signals
    m_clk_src->out(m_sig_clk);
    m_sampler->clk_sample(m_sig_clk);       // Dummy clock (unused in phase mode)
    // Note: sampling_trigger comes from CDR below
    m_sampler->data_out_de(m_sig_sampler_data_out_de);  // DE domain output
    
    // Sampler output → Splitter → CDR and data feedback
    m_sampler->data_out(m_sig_sampler_out);
    m_sampler_splitter->in(m_sig_sampler_out);
    m_sampler_splitter->out1(m_sig_data_feedback);  // To DFE Summer
    m_sampler_splitter->out2(m_sig_cdr_in);         // To CDR
    
    // Data feedback → DFE Summer (历史判决反馈)
    m_dfe_summer->data_in(m_sig_data_feedback);
    
    // CDR input from splitter output
    m_cdr->in(m_sig_cdr_in);
    
    // CDR outputs: phase_out (for monitoring) and sampling_trigger (controls sampler)
    m_cdr->phase_out(m_sig_cdr_phase);          // For debug/monitoring only
    m_cdr->sampling_trigger(m_sig_sampling_trigger);  // Controls when sampler captures
    
    // Connect CDR's sampling trigger to sampler
    m_sampler->sampling_trigger(m_sig_sampling_trigger);
    
    // External data_out: pass-through from sampler output
    m_data_out_tap->in(m_sig_sampler_out);
    m_data_out_tap->out(data_out);
    
    // ========================================================================
    // Connect DE-TDF bridge signals (Adaption ↔ TDF modules)
    // ========================================================================
    
    // Adaption inputs (from DE signals - need external driver or default)
    m_adaption->phase_error(m_sig_phase_error_de);
    m_adaption->amplitude_rms(m_sig_amplitude_rms_de);
    m_adaption->error_count(m_sig_error_count_de);
    m_adaption->isi_metric(m_sig_isi_metric_de);
    m_adaption->mode(m_sig_mode_de);
    m_adaption->reset(m_sig_reset_de);
    m_adaption->scenario_switch(m_sig_scenario_switch_de);
    
    // Adaption outputs (to DE signals)
    m_adaption->vga_gain(m_sig_vga_gain_de);
    m_adaption->ctle_zero(m_sig_ctle_zero_de);
    m_adaption->ctle_pole(m_sig_ctle_pole_de);
    m_adaption->ctle_dc_gain(m_sig_ctle_dc_gain_de);
    m_adaption->dfe_tap1(m_sig_dfe_tap1_de);
    m_adaption->dfe_tap2(m_sig_dfe_tap2_de);
    m_adaption->dfe_tap3(m_sig_dfe_tap3_de);
    m_adaption->dfe_tap4(m_sig_dfe_tap4_de);
    m_adaption->dfe_tap5(m_sig_dfe_tap5_de);
    m_adaption->dfe_tap6(m_sig_dfe_tap6_de);
    m_adaption->dfe_tap7(m_sig_dfe_tap7_de);
    m_adaption->dfe_tap8(m_sig_dfe_tap8_de);
    m_adaption->sampler_threshold(m_sig_sampler_threshold_de);
    m_adaption->sampler_hysteresis(m_sig_sampler_hysteresis_de);
    m_adaption->phase_cmd(m_sig_phase_cmd_de);
    m_adaption->update_count(m_sig_update_count_de);
    m_adaption->freeze_flag(m_sig_freeze_flag_de);
    
    // Connect DFE Summer DE ports (抽头更新)
    m_dfe_summer->tap1_de(m_sig_dfe_tap1_de);
    m_dfe_summer->tap2_de(m_sig_dfe_tap2_de);
    m_dfe_summer->tap3_de(m_sig_dfe_tap3_de);
    m_dfe_summer->tap4_de(m_sig_dfe_tap4_de);
    m_dfe_summer->tap5_de(m_sig_dfe_tap5_de);
    
    // Initialize DE input signals with default values
    m_sig_phase_error_de.write(0.0);
    m_sig_amplitude_rms_de.write(0.4);
    m_sig_error_count_de.write(0);
    m_sig_isi_metric_de.write(0.0);
    m_sig_mode_de.write(2);  // Data mode
    m_sig_reset_de.write(false);
    m_sig_scenario_switch_de.write(0.0);
}

RxTopModule::~RxTopModule()
{
    delete m_ctle;
    delete m_vga;
    delete m_dfe_summer;
    delete m_sampler;
    delete m_cdr;
    delete m_adaption;
    delete m_clk_src;
    delete m_data_out_tap;
    delete m_sampler_splitter;
}

double RxTopModule::get_cdr_phase() const
{
    return m_cdr->get_raw_phase();
}

double RxTopModule::get_cdr_integral_state() const
{
    return m_cdr->get_integral_state();
}

} // namespace serdes
