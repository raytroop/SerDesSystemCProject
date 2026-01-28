#include "ams/serdes_link_top.h"
#include <iostream>

namespace serdes {

SerdesLinkTopModule::SerdesLinkTopModule(sc_core::sc_module_name nm, 
                                         const SerdesLinkParams& params)
    : sc_module(nm)
    , vdd("vdd")
    , data_out("data_out")
    , m_sig_wavegen_out("sig_wavegen_out")
    , m_sig_tx_out_p("sig_tx_out_p")
    , m_sig_tx_out_n("sig_tx_out_n")
    , m_sig_channel_in("sig_channel_in")
    , m_sig_channel_out("sig_channel_out")
    , m_sig_rx_in_p("sig_rx_in_p")
    , m_sig_rx_in_n("sig_rx_in_n")
    , m_sig_data_out("sig_data_out")
    , m_params(params)
{
    // ========================================================================
    // Create Sub-modules
    // ========================================================================
    
    // Wave Generator
    m_wavegen = new WaveGenerationTdf("wavegen", 
                                      m_params.wave, 
                                      m_params.sample_rate, 
                                      m_params.seed);
    
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
}

SerdesLinkTopModule::~SerdesLinkTopModule() {
    delete m_wavegen;
    delete m_tx;
    delete m_d2s;
    delete m_channel;
    delete m_s2d;
    delete m_rx;
    delete m_data_out_tap;
}

} // namespace serdes
