#include "ams/serdes_link_top.h"
#include <iostream>

namespace serdes {

SerdesLinkTopModule::SerdesLinkTopModule(sc_core::sc_module_name nm, 
                                         const SerdesLinkParams& params)
    : sc_module(nm)
    , vdd("vdd")
    , data_out("data_out")
    , mon_tx_out_p("mon_tx_out_p")
    , mon_tx_out_n("mon_tx_out_n")
    , mon_dfe_out_p("mon_dfe_out_p")
    , mon_dfe_out_n("mon_dfe_out_n")
    , mon_vga_out_p("mon_vga_out_p")
    , mon_vga_out_n("mon_vga_out_n")
    , mon_cdr_phase("mon_cdr_phase")
    , m_sig_wavegen_out("sig_wavegen_out")
    , m_sig_tx_out_p("sig_tx_out_p")
    , m_sig_tx_out_n("sig_tx_out_n")
    , m_sig_channel_in("sig_channel_in")
    , m_sig_channel_out("sig_channel_out")
    , m_sig_rx_in_p("sig_rx_in_p")
    , m_sig_rx_in_n("sig_rx_in_n")
    , m_sig_data_out("sig_data_out")
    , m_params(params)
    , m_dfe_tap_p(nullptr)
    , m_dfe_tap_n(nullptr)
    , m_vga_tap_p(nullptr)
    , m_vga_tap_n(nullptr)
    , m_cdr_tap(nullptr)
{
    // ========================================================================
    // Create Sub-modules
    // ========================================================================
    
    std::cout << "    [Link] Creating WaveGen..." << std::endl;
    // Wave Generator - data rate determined by UI (1 / data_rate)
    double ui = 1.0 / m_params.data_rate;
    m_wavegen = new WaveGenerationTdf("wavegen", 
                                      m_params.wave, 
                                      m_params.sample_rate, 
                                      ui,           // UI parameter
                                      m_params.seed);
    
    std::cout << "    [Link] Creating TX..." << std::endl;
    
    // TX Top Module
    m_tx = new TxTopModule("tx", m_params.tx);
    
    // Differential to Single-ended Converter
    m_d2s = new DiffToSingleTdf("d2s");
    
    // Channel Model
    m_channel = new ChannelSParamTdf("channel", m_params.channel);
    
    // Single-ended to Differential Converter
    m_s2d = new SingleToDiffTdf("s2d");
    
    // RX Top Module
    m_rx = new RxTopModule("rx", m_params.rx, m_params.adaption);
    
    // Pass-through for data_out port
    m_data_out_tap = new SignalPassThrough("data_out_tap");
    
    // Signal taps for TX output monitoring (analog)
    m_tx_tap_p = new SignalPassThrough("tx_tap_p");
    m_tx_tap_n = new SignalPassThrough("tx_tap_n");
    
    // Signal taps for DFE output monitoring
    m_dfe_tap_p = new SignalPassThrough("dfe_tap_p");
    m_dfe_tap_n = new SignalPassThrough("dfe_tap_n");
    
    // Signal taps for VGA output monitoring
    m_vga_tap_p = new SignalPassThrough("vga_tap_p");
    m_vga_tap_n = new SignalPassThrough("vga_tap_n");
    
    // Signal tap for CDR phase output monitoring
    m_cdr_tap = new SignalPassThrough("cdr_tap");
    
    // ========================================================================
    // Connect Signal Chain
    // ========================================================================
    
    // WaveGen output -> TX input
    m_wavegen->out(m_sig_wavegen_out);
    m_tx->in(m_sig_wavegen_out);
    
    // TX vdd connection
    m_tx->vdd(vdd);
    
    // TX differential output -> DiffToSingle
    m_tx->out_p(m_sig_tx_out_p);
    m_tx->out_n(m_sig_tx_out_n);
    m_d2s->in_p(m_sig_tx_out_p);
    m_d2s->in_n(m_sig_tx_out_n);
    
    // TX output monitoring (for eye diagram analysis)
    m_tx_tap_p->in(m_sig_tx_out_p);
    m_tx_tap_p->out(mon_tx_out_p);
    m_tx_tap_n->in(m_sig_tx_out_n);
    m_tx_tap_n->out(mon_tx_out_n);
    
    // DiffToSingle output -> Channel input
    m_d2s->out(m_sig_channel_in);
    m_channel->in(m_sig_channel_in);
    
    // Channel output -> SingleToDiff input
    m_channel->out(m_sig_channel_out);
    m_s2d->in(m_sig_channel_out);
    
    // SingleToDiff output -> RX differential input
    m_s2d->out_p(m_sig_rx_in_p);
    m_s2d->out_n(m_sig_rx_in_n);
    m_rx->in_p(m_sig_rx_in_p);
    m_rx->in_n(m_sig_rx_in_n);
    
    // RX vdd connection
    m_rx->vdd(vdd);
    
    // RX data output -> internal signal -> external port via pass-through
    m_rx->data_out(m_sig_data_out);
    m_data_out_tap->in(m_sig_data_out);
    m_data_out_tap->out(data_out);
    
    // DFE output monitoring (Sampler input - for Sampler-centric eye diagram)
    // This shows the analog waveform that the Sampler actually sees
    m_dfe_tap_p->in(const_cast<sca_tdf::sca_signal<double>&>(m_rx->get_dfe_out_p_signal()));
    m_dfe_tap_p->out(mon_dfe_out_p);
    
    m_dfe_tap_n->in(const_cast<sca_tdf::sca_signal<double>&>(m_rx->get_dfe_out_n_signal()));
    m_dfe_tap_n->out(mon_dfe_out_n);
    
    // VGA output monitoring (before DFE - for traditional RX eye diagram)
    m_vga_tap_p->in(const_cast<sca_tdf::sca_signal<double>&>(m_rx->get_vga_out_p_signal()));
    m_vga_tap_p->out(mon_vga_out_p);
    
    m_vga_tap_n->in(const_cast<sca_tdf::sca_signal<double>&>(m_rx->get_vga_out_n_signal()));
    m_vga_tap_n->out(mon_vga_out_n);
    
    // CDR phase output monitoring
    m_cdr_tap->in(const_cast<sca_tdf::sca_signal<double>&>(m_rx->get_cdr_phase_signal()));
    m_cdr_tap->out(mon_cdr_phase);
}

SerdesLinkTopModule::~SerdesLinkTopModule() {
    delete m_wavegen;
    delete m_tx;
    delete m_d2s;
    delete m_channel;
    delete m_s2d;
    delete m_rx;
    delete m_data_out_tap;
    delete m_tx_tap_p;
    delete m_tx_tap_n;
    delete m_dfe_tap_p;
    delete m_dfe_tap_n;
    delete m_vga_tap_p;
    delete m_vga_tap_n;
    delete m_cdr_tap;
}

double SerdesLinkTopModule::get_dfe_tap(int tap_index) const {
    if (!m_rx || tap_index < 1 || tap_index > 5) return 0.0;
    // Access through RxTop's getter
    return const_cast<SerdesLinkTopModule*>(this)->m_rx->get_dfe_tap_signal(tap_index).read();
}

} // namespace serdes
