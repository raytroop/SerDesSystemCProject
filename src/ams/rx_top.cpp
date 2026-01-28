#include "ams/rx_top.h"
#include <iostream>

namespace serdes {

RxTopModule::RxTopModule(sc_core::sc_module_name nm, const RxParams& rx_params)
    : sc_module(nm)
    , in_p("in_p")
    , in_n("in_n")
    , vdd("vdd")
    , data_out("data_out")
    , m_sig_ctle_out_p("sig_ctle_out_p")
    , m_sig_ctle_out_n("sig_ctle_out_n")
    , m_sig_vga_out_p("sig_vga_out_p")
    , m_sig_vga_out_n("sig_vga_out_n")
    , m_sig_dfe_out_p("sig_dfe_out_p")
    , m_sig_dfe_out_n("sig_dfe_out_n")
    , m_sig_sampler_out("sig_sampler_out")
    , m_sig_cdr_phase("sig_cdr_phase")
    , m_sig_clk("sig_clk")
    , m_params(rx_params)
{
    // ========================================================================
    // Setup DFE parameters for N path (negated taps)
    // ========================================================================
    setup_dfe_negated_taps();
    
    // ========================================================================
    // Instantiate sub-modules
    // ========================================================================
    
    // CTLE: Continuous-Time Linear Equalizer
    m_ctle = new RxCtleTdf("ctle", m_params.ctle);
    
    // VGA: Variable Gain Amplifier
    m_vga = new RxVgaTdf("vga", m_params.vga);
    
    // DFE: Decision Feedback Equalizer (dual instances)
    m_dfe_p = new RxDfeTdf("dfe_p", m_params.dfe);      // P path: normal taps
    m_dfe_n = new RxDfeTdf("dfe_n", m_dfe_params_n);    // N path: negated taps
    
    // Sampler: Decision circuit
    // Configure for phase-driven mode (CDR controls sampling phase)
    RxSamplerParams sampler_params = m_params.sampler;
    sampler_params.phase_source = "phase";  // Force phase-driven mode
    m_sampler = new RxSamplerTdf("sampler", sampler_params);
    
    // CDR: Clock and Data Recovery
    m_cdr = new RxCdrTdf("cdr", m_params.cdr);
    
    // Clock source: Dummy clock for Sampler's clk_sample port
    // (Required for port connection but not used in phase-driven mode)
    m_clk_src = new ConstClockSource("clk_src");
    
    // Data output tap: Pass-through module to drive external data_out port
    m_data_out_tap = new SignalPassThrough("data_out_tap");
    
    // ========================================================================
    // Connect signal chain
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
    
    // VGA output → DFE input (split to P and N paths)
    m_vga->out_p(m_sig_vga_out_p);
    m_vga->out_n(m_sig_vga_out_n);
    m_dfe_p->in(m_sig_vga_out_p);   // P path: VGA out_p → DFE_P
    m_dfe_n->in(m_sig_vga_out_n);   // N path: VGA out_n → DFE_N (taps negated)
    
    // DFE output → Sampler input (differential)
    m_dfe_p->out(m_sig_dfe_out_p);
    m_dfe_n->out(m_sig_dfe_out_n);
    m_sampler->in_p(m_sig_dfe_out_p);
    m_sampler->in_n(m_sig_dfe_out_n);
    
    // Sampler control signals
    m_clk_src->out(m_sig_clk);
    m_sampler->clk_sample(m_sig_clk);       // Dummy clock (unused)
    m_sampler->phase_offset(m_sig_cdr_phase);  // CDR → Sampler (closed loop)
    
    // Sampler output → CDR input and external output
    // In SystemC-AMS TDF, we can connect multiple readers to one signal
    // Sampler drives the signal, CDR reads it, and pass-through reads it
    m_sampler->data_out(m_sig_sampler_out);
    m_cdr->in(m_sig_sampler_out);
    
    // CDR phase output → Sampler (closed loop)
    m_cdr->phase_out(m_sig_cdr_phase);
    
    // External data_out: pass-through from sampler output
    m_data_out_tap->in(m_sig_sampler_out);
    m_data_out_tap->out(data_out);
}

RxTopModule::~RxTopModule()
{
    delete m_ctle;
    delete m_vga;
    delete m_dfe_p;
    delete m_dfe_n;
    delete m_sampler;
    delete m_cdr;
    delete m_clk_src;
    delete m_data_out_tap;
}

void RxTopModule::setup_dfe_negated_taps()
{
    // Copy base DFE parameters
    m_dfe_params_n = m_params.dfe;
    
    // Clear and negate all taps for N path
    // For differential symmetry: y_n = x_n - sum((-tap[i]) * history[i])
    // This ensures: y_p - y_n = (x_p - x_n) - sum(tap[i] * (hist_p[i] - hist_n[i]))
    m_dfe_params_n.taps.clear();
    for (double t : m_params.dfe.taps) {
        m_dfe_params_n.taps.push_back(-t);
    }
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
