// Simple test for STATE_SPACE channel method
#include <systemc-ams>
#include <iostream>
#include <fstream>
#include "ams/channel_sparam.h"

using namespace sc_core;
using namespace sca_tdf;
using namespace serdes;

int sc_main(int argc, char* argv[]) {
    std::cout << "Testing STATE_SPACE channel method..." << std::endl;
    
    // Create simple testbench
    ChannelParams params;
    params.attenuation_db = 0.0;
    params.delay_ps = 0.0;
    
    ChannelExtendedParams ext_params;
    ext_params.method = ChannelMethod::STATE_SPACE;
    ext_params.fs = 80e9;  // 80 GHz sampling
    ext_params.config_file = "test_config_state_space.json";
    
    // Check if config file exists
    std::ifstream check_file(ext_params.config_file);
    if (!check_file.is_open()) {
        std::cerr << "Config file not found: " << ext_params.config_file << std::endl;
        std::cerr << "Run: python scripts/test_vf_integration.py first" << std::endl;
        return 1;
    }
    check_file.close();
    
    // Create channel
    ChannelSParamTdf channel("channel", params, ext_params);
    
    // Create test source and sink
    sca_tdf::sca_source<double> src("src", 1.0, 0.0, 1.0e-9, 80e9, sca_tdf::SCA_SINE);
    sca_tdf::sca_sink<double> sink("sink", "state_space_output.dat");
    
    // Connect
    src.out(channel.in);
    channel.out(sink.in);
    
    // Run for short time
    sc_start(10.0, SC_NS);
    
    std::cout << "STATE_SPACE test completed successfully!" << std::endl;
    std::cout << "Output written to: state_space_output.dat" << std::endl;
    
    return 0;
}
