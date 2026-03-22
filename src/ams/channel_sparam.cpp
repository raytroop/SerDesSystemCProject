#include "ams/channel_sparam.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <complex>

// Use nlohmann json for parsing (included in third_party)
#include "../third_party/json.hpp"

using json = nlohmann::json;

namespace serdes {

// ============================================================================
// Constructors and Destructor
// ============================================================================

ChannelSParamTdf::ChannelSParamTdf(sc_core::sc_module_name nm, const ChannelParams& params)
    : sca_tdf::sca_module(nm)
    , in("in")
    , out("out")
    , m_params(params)
    , m_filter_state(0.0)
    , m_alpha(0.3)
    , m_delay_idx(0)
    , m_use_fft(false)
    , m_fft_size(0)
    , m_block_size(0)
    , m_block_idx(0)
    , m_config_loaded(false)
    , m_initialized(false)
{
    // Default to simple model for backward compatibility
    m_ext_params.method = ChannelMethod::SIMPLE;
    // Initialize with SISO ports (will be resized after config load)
    in.init(1);
    out.init(1);
}

ChannelSParamTdf::ChannelSParamTdf(sc_core::sc_module_name nm, 
                                   const ChannelParams& params,
                                   const ChannelExtendedParams& ext_params)
    : sca_tdf::sca_module(nm)
    , in("in")
    , out("out")
    , m_params(params)
    , m_ext_params(ext_params)
    , m_filter_state(0.0)
    , m_alpha(0.3)
    , m_delay_idx(0)
    , m_use_fft(false)
    , m_fft_size(0)
    , m_block_size(0)
    , m_block_idx(0)
    , m_config_loaded(false)
    , m_initialized(false)
{
    std::cout << "[DEBUG] ChannelSParamTdf: Constructor called, method=" 
              << static_cast<int>(m_ext_params.method) 
              << ", config_file=" << ext_params.config_file << std::endl;
    
    // Load configuration if specified
    if (!ext_params.config_file.empty()) {
        load_config(ext_params.config_file);
        std::cout << "[DEBUG] ChannelSParamTdf: After load_config, method=" 
                  << static_cast<int>(m_ext_params.method) << std::endl;
    }
    
    // Initialize ports based on config (or default SISO)
    int n_in = m_port_config.active_inputs.empty() ? 1 : m_port_config.active_inputs.size();
    int n_out = m_port_config.active_outputs.empty() ? 1 : m_port_config.active_outputs.size();
    in.init(n_in);
    out.init(n_out);
    
    std::cout << "[DEBUG] ChannelSParamTdf: Ports initialized: " 
              << n_in << " inputs, " << n_out << " outputs" << std::endl;
}

ChannelSParamTdf::~ChannelSParamTdf() {
    // Cleanup if needed
}

// ============================================================================
// SystemC-AMS Lifecycle Methods
// ============================================================================

void ChannelSParamTdf::set_attributes() {
    // Set rate for all input and output ports
    for (unsigned int i = 0; i < in.size(); ++i) {
        in[i].set_rate(1);
    }
    for (unsigned int i = 0; i < out.size(); ++i) {
        out[i].set_rate(1);
    }
    
    // Set timestep for TDF module
    double timestep = 1.0 / m_ext_params.fs;
    set_timestep(timestep, sc_core::SC_SEC);
    
    // Set delay based on method
    if (m_ext_params.method == ChannelMethod::IMPULSE && m_use_fft) {
        // FFT convolution introduces block delay
        for (unsigned int i = 0; i < out.size(); ++i) {
            out[i].set_delay(m_block_size);
        }
    }
}

void ChannelSParamTdf::initialize() {
    if (m_initialized) return;
    
    std::cout << "[DEBUG] ChannelSParamTdf: initialize() called, method=" 
              << static_cast<int>(m_ext_params.method) << std::endl;
    
    switch (m_ext_params.method) {
        case ChannelMethod::SIMPLE:
            init_simple_model();
            break;
        case ChannelMethod::RATIONAL:
            init_rational_model();
            break;
        case ChannelMethod::IMPULSE:
            init_impulse_model();
            break;
        case ChannelMethod::STATE_SPACE:
            init_state_space_model();
            break;
    }
    
    m_initialized = true;
}

void ChannelSParamTdf::processing() {
    switch (m_ext_params.method) {
        case ChannelMethod::SIMPLE: {
            double x_in = in[0].read();
            double y_out = process_simple(x_in);
            out[0].write(y_out);
            break;
        }
        case ChannelMethod::RATIONAL: {
            double x_in = in[0].read();
            double y_out = process_rational(x_in);
            out[0].write(y_out);
            break;
        }
        case ChannelMethod::IMPULSE: {
            double x_in = in[0].read();
            double y_out;
            if (m_use_fft) {
                y_out = process_impulse_fft(x_in);
            } else {
                y_out = process_impulse(x_in);
            }
            out[0].write(y_out);
            break;
        }
        case ChannelMethod::STATE_SPACE: {
            // MIMO processing
            process_state_space_mimo();
            break;
        }
    }
}

// ============================================================================
// Configuration Loading
// ============================================================================

bool ChannelSParamTdf::load_config(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "ChannelSParamTdf: Cannot open config file: " << config_path << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return parse_json_config(buffer.str());
}

bool ChannelSParamTdf::parse_json_config(const std::string& json_content) {
    try {
        json config = json::parse(json_content);
        
        // Get sampling frequency
        if (config.contains("fs")) {
            m_ext_params.fs = config["fs"].get<double>();
        }
        
        // Get method (case-insensitive comparison)
        std::string method_str = config.value("method", "simple");
        std::cout << "[DEBUG] ChannelSParamTdf: Loading method: " << method_str << std::endl;
        
        // Convert to lowercase for comparison
        std::string method_lower = method_str;
        std::transform(method_lower.begin(), method_lower.end(), method_lower.begin(), ::tolower);
        std::cout << "[DEBUG] ChannelSParamTdf: method_lower: " << method_lower << std::endl;
        
        if (method_lower == "rational") {
            m_ext_params.method = ChannelMethod::RATIONAL;
        } else if (method_lower == "impulse") {
            m_ext_params.method = ChannelMethod::IMPULSE;
        } else if (method_lower == "state_space" || method_lower == "state-space" ||
                   method_lower == "state_space_mimo") {
            m_ext_params.method = ChannelMethod::STATE_SPACE;
        } else {
            m_ext_params.method = ChannelMethod::SIMPLE;
        }
        
        // Parse filters (rational method)
        if (config.contains("filters")) {
            int filter_count = 0;
            for (auto it = config["filters"].begin(); it != config["filters"].end(); ++it) {
                filter_count++;
            }
            std::cout << "[DEBUG] ChannelSParamTdf: Found " << filter_count << " filters" << std::endl;
        }
        if (config.contains("filters") && !config["filters"].empty()) {
            // Use first filter (typically S21)
            for (auto it = config["filters"].begin(); it != config["filters"].end(); ++it) {
                const auto& filter = it.value();
                m_rational_data.num_coeffs.clear();
                m_rational_data.den_coeffs.clear();
                
                for (auto& v : filter["num"]) {
                    m_rational_data.num_coeffs.push_back(v.get<double>());
                }
                for (auto& v : filter["den"]) {
                    m_rational_data.den_coeffs.push_back(v.get<double>());
                }
                
                m_rational_data.order = filter.value("order", 0);
                m_rational_data.dc_gain = filter.value("dc_gain", 1.0);
                m_rational_data.mse = filter.value("mse", 0.0);
                
                std::cout << "[DEBUG] ChannelSParamTdf: Parsed rational filter '" << it.key() 
                          << "': num=" << m_rational_data.num_coeffs.size() 
                          << ", den=" << m_rational_data.den_coeffs.size() 
                          << ", order=" << m_rational_data.order << std::endl;
                
                break; // Only use first filter for now
            }
        }
        
        // Parse impulse responses
        if (config.contains("impulse_responses")) {
            int ir_count = 0;
            for (auto it = config["impulse_responses"].begin(); it != config["impulse_responses"].end(); ++it) {
                ir_count++;
            }
            std::cout << "[DEBUG] ChannelSParamTdf: Found " << ir_count << " impulse responses" << std::endl;
        }
        if (config.contains("impulse_responses") && !config["impulse_responses"].empty()) {
            for (auto it = config["impulse_responses"].begin(); it != config["impulse_responses"].end(); ++it) {
                const auto& ir = it.value();
                m_impulse_data.impulse.clear();
                m_impulse_data.time.clear();
                
                for (auto& v : ir["impulse"]) {
                    m_impulse_data.impulse.push_back(v.get<double>());
                }
                if (ir.contains("time")) {
                    for (auto& v : ir["time"]) {
                        m_impulse_data.time.push_back(v.get<double>());
                    }
                }
                
                m_impulse_data.length = ir.value("length", static_cast<int>(m_impulse_data.impulse.size()));
                m_impulse_data.dt = ir.value("dt", 1.0 / m_ext_params.fs);
                m_impulse_data.energy = ir.value("energy", 0.0);
                m_impulse_data.peak_time = ir.value("peak_time", 0.0);
                
                std::cout << "[DEBUG] ChannelSParamTdf: Parsed impulse response '" << it.key() 
                          << "': impulse=" << m_impulse_data.impulse.size() 
                          << ", time=" << m_impulse_data.time.size() 
                          << ", dt=" << m_impulse_data.dt << std::endl;
                
                break; // Only use first IR for now
            }
        }
        
        std::cout << "[DEBUG] ChannelSParamTdf: Configuration loaded successfully (fs=" 
                  << m_ext_params.fs << ", method=" << static_cast<int>(m_ext_params.method) 
                  << ")" << std::endl;
        m_config_loaded = true;
        return true;
        
    } catch (const json::exception& e) {
        std::cerr << "ChannelSParamTdf: JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// Model Initialization
// ============================================================================

void ChannelSParamTdf::init_simple_model() {
    // Calculate filter coefficient from bandwidth
    double omega_c = 2.0 * M_PI * m_params.bandwidth_hz;
    double dt = 1.0 / m_ext_params.fs;
    
    // First-order IIR filter coefficient
    // Using bilinear transform approximation
    m_alpha = omega_c * dt / (1.0 + omega_c * dt);
    
    m_filter_state = 0.0;
}

void ChannelSParamTdf::init_rational_model() {
    if (m_rational_data.num_coeffs.empty() || m_rational_data.den_coeffs.empty()) {
        std::cerr << "ChannelSParamTdf: Rational filter coefficients not loaded" << std::endl;
        // Fall back to simple model
        m_ext_params.method = ChannelMethod::SIMPLE;
        init_simple_model();
        return;
    }
    
    // Initialize sca_vector for LTF
    // VF generates coefficients in DESCENDING order (np.polyval format): [bn, ..., b1, b0]
    // Test both interpretations to find the correct one
    int num_size = static_cast<int>(m_rational_data.num_coeffs.size());
    int den_size = static_cast<int>(m_rational_data.den_coeffs.size());
    
    m_num_vec.resize(num_size);
    m_den_vec.resize(den_size);
    
    // Use coefficients as-is (DESCENDING order like np.polyval)
    // H(s) = (bn*s^n + ... + b1*s + b0) / (an*s^n + ... + a1*s + a0)
    for (int i = 0; i < num_size; ++i) {
        m_num_vec(i) = m_rational_data.num_coeffs[i];
    }
    for (int i = 0; i < den_size; ++i) {
        m_den_vec(i) = m_rational_data.den_coeffs[i];
    }
    
    // DC gain = b0/a0 = last element in DESCENDING array
    if (num_size > 0 && den_size > 0) {
        m_rational_data.dc_gain = m_rational_data.num_coeffs[num_size-1] / m_rational_data.den_coeffs[den_size-1];
    }
    
    std::cout << "[DEBUG] ChannelSParamTdf: Rational filter initialized (DESCENDING order)" << std::endl;
    std::cout << "[DEBUG]   Order: " << m_rational_data.order << std::endl;
    std::cout << "[DEBUG]   DC gain: " << m_rational_data.dc_gain << std::endl;
    std::cout << "[DEBUG]   num[0]=" << m_num_vec(0) << " (highest power)" << std::endl;
    std::cout << "[DEBUG]   num[last]=" << m_num_vec(num_size-1) << " (constant term)" << std::endl;
}

void ChannelSParamTdf::init_impulse_model() {
    if (m_impulse_data.impulse.empty()) {
        std::cerr << "ChannelSParamTdf: Impulse response not loaded" << std::endl;
        // Fall back to simple model
        m_ext_params.method = ChannelMethod::SIMPLE;
        init_simple_model();
        return;
    }
    
    int L = static_cast<int>(m_impulse_data.impulse.size());
    
    // Decide whether to use FFT convolution
    m_use_fft = (L > m_ext_params.impulse.fft_threshold) && m_ext_params.impulse.use_fft;
    
    if (m_use_fft) {
        init_fft_convolution();
    } else {
        // Direct convolution: allocate delay line
        m_delay_line.resize(L, 0.0);
        m_delay_idx = 0;
    }
}

void ChannelSParamTdf::init_state_space_model() {
    try {
        // Access JSON data from config
        if (m_ext_params.config_file.empty()) {
            std::cerr << "ChannelSParamTdf: State-space method requires config file" << std::endl;
            m_ext_params.method = ChannelMethod::SIMPLE;
            init_simple_model();
            return;
        }
        
        // Load and parse the config file
        std::ifstream file(m_ext_params.config_file);
        if (!file.is_open()) {
            std::cerr << "ChannelSParamTdf: Cannot open config file: " << m_ext_params.config_file << std::endl;
            m_ext_params.method = ChannelMethod::SIMPLE;
            init_simple_model();
            return;
        }
        
        json config;
        file >> config;
        file.close();
        
        // Parse new format: full_model + port_config
        if (config.contains("full_model")) {
            // New MIMO format
            const auto& fm = config["full_model"];
            
            m_full_model.n_diff_ports = fm.value("n_diff_ports", 1);
            m_full_model.n_outputs = fm.value("n_outputs", 1);
            m_full_model.n_states = fm.value("n_states", 0);
            
            // Parse port_pairs
            if (fm.contains("port_pairs")) {
                for (const auto& pp : fm["port_pairs"]) {
                    m_full_model.port_pairs.push_back({pp[0].get<int>(), pp[1].get<int>()});
                }
            }
            
            // Parse delays
            if (fm.contains("delay_s")) {
                for (const auto& d : fm["delay_s"]) {
                    m_full_model.delays.push_back(d.get<double>());
                }
            }
            
            // Parse state_space matrices
            if (!fm.contains("state_space")) {
                std::cerr << "ChannelSParamTdf: No state_space in full_model" << std::endl;
                m_ext_params.method = ChannelMethod::SIMPLE;
                init_simple_model();
                return;
            }
            
            const auto& ss = fm["state_space"];
            int n_states = m_full_model.n_states;
            int n_inputs = m_full_model.n_diff_ports;
            int n_outputs = m_full_model.n_outputs;
            
            m_full_model.state_space.n_states = n_states;
            m_full_model.state_space.n_inputs = n_inputs;
            m_full_model.state_space.n_outputs = n_outputs;
            
            // Resize matrices
            m_full_model.state_space.A.resize(n_states, n_states);
            m_full_model.state_space.B.resize(n_states, n_inputs);
            m_full_model.state_space.C.resize(n_outputs, n_states);
            m_full_model.state_space.D.resize(n_outputs, n_inputs);
            m_full_model.state_space.E.resize(n_outputs, n_inputs);
            
            // Fill A matrix
            const auto& A_json = ss["A"];
            for (int i = 0; i < n_states; ++i) {
                for (int j = 0; j < n_states; ++j) {
                    m_full_model.state_space.A(i+1, j+1) = A_json[i][j].get<double>();
                }
            }
            
            // Fill B matrix (n_states x n_inputs)
            const auto& B_json = ss["B"];
            for (int i = 0; i < n_states; ++i) {
                for (int j = 0; j < n_inputs; ++j) {
                    m_full_model.state_space.B(i+1, j+1) = B_json[i][j].get<double>();
                }
            }
            
            // Fill C matrix (n_outputs x n_states)
            const auto& C_json = ss["C"];
            for (int i = 0; i < n_outputs; ++i) {
                for (int j = 0; j < n_states; ++j) {
                    m_full_model.state_space.C(i+1, j+1) = C_json[i][j].get<double>();
                }
            }
            
            // Fill D matrix (n_outputs x n_inputs)
            const auto& D_json = ss["D"];
            for (int i = 0; i < n_outputs; ++i) {
                for (int j = 0; j < n_inputs; ++j) {
                    m_full_model.state_space.D(i+1, j+1) = D_json[i][j].get<double>();
                }
            }
            
            // Fill E matrix (n_outputs x n_inputs)
            if (ss.contains("E")) {
                const auto& E_json = ss["E"];
                for (int i = 0; i < n_outputs; ++i) {
                    for (int j = 0; j < n_inputs; ++j) {
                        m_full_model.state_space.E(i+1, j+1) = E_json[i][j].get<double>();
                    }
                }
            }
            
            // Parse port_config
            if (config.contains("port_config")) {
                const auto& pc = config["port_config"];
                for (const auto& inp : pc["active_inputs"]) {
                    m_port_config.active_inputs.push_back(inp.get<int>());
                }
                for (const auto& outp : pc["active_outputs"]) {
                    m_port_config.active_outputs.push_back(outp.get<int>());
                }
            } else {
                // Default: use all inputs and outputs
                for (int i = 0; i < n_inputs; ++i) {
                    m_port_config.active_inputs.push_back(i);
                }
                for (int i = 0; i < n_outputs; ++i) {
                    m_port_config.active_outputs.push_back(i);
                }
            }
            
            // Extract active matrices
            extract_active_matrices();
            
            std::cout << "[DEBUG] ChannelSParamTdf: MIMO State-space model initialized" << std::endl;
            std::cout << "[DEBUG]   Full model: " << n_states << " states, " 
                      << n_inputs << " inputs, " << n_outputs << " outputs" << std::endl;
            std::cout << "[DEBUG]   Active: " << m_port_config.active_inputs.size() << " inputs, "
                      << m_port_config.active_outputs.size() << " outputs" << std::endl;
            
        } else if (config.contains("state_space")) {
            // Legacy format (backward compatibility)
            const auto& ss = config["state_space"];
            
            const auto& A_json = ss["A"];
            int n_states = A_json.size();
            const auto& C_json = ss["C"];
            int n_outputs = C_json.size();
            
            m_full_model.n_states = n_states;
            m_full_model.n_diff_ports = 1;
            m_full_model.n_outputs = n_outputs;
            
            m_full_model.state_space.n_states = n_states;
            m_full_model.state_space.n_inputs = 1;
            m_full_model.state_space.n_outputs = n_outputs;
            
            m_full_model.state_space.A.resize(n_states, n_states);
            m_full_model.state_space.B.resize(n_states, 1);
            m_full_model.state_space.C.resize(n_outputs, n_states);
            m_full_model.state_space.D.resize(n_outputs, 1);
            m_full_model.state_space.E.resize(n_outputs, 1);
            
            for (int i = 0; i < n_states; ++i) {
                for (int j = 0; j < n_states; ++j) {
                    m_full_model.state_space.A(i+1, j+1) = A_json[i][j].get<double>();
                }
            }
            
            const auto& B_json = ss["B"];
            for (int i = 0; i < n_states; ++i) {
                m_full_model.state_space.B(i+1, 1) = B_json[i][0].get<double>();
            }
            
            for (int i = 0; i < n_outputs; ++i) {
                for (int j = 0; j < n_states; ++j) {
                    m_full_model.state_space.C(i+1, j+1) = C_json[i][j].get<double>();
                }
            }
            
            const auto& D_json = ss["D"];
            for (int i = 0; i < n_outputs; ++i) {
                m_full_model.state_space.D(i+1, 1) = D_json[i][0].get<double>();
            }
            
            if (ss.contains("E")) {
                const auto& E_json = ss["E"];
                for (int i = 0; i < n_outputs; ++i) {
                    m_full_model.state_space.E(i+1, 1) = E_json[i][0].get<double>();
                }
            }
            
            // Default SISO config
            m_port_config.active_inputs = {0};
            m_port_config.active_outputs = {0};
            
            extract_active_matrices();
            
            std::cout << "[DEBUG] ChannelSParamTdf: Legacy state-space model loaded" << std::endl;
        } else {
            std::cerr << "ChannelSParamTdf: No state_space or full_model in config" << std::endl;
            m_ext_params.method = ChannelMethod::SIMPLE;
            init_simple_model();
            return;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "ChannelSParamTdf: Error initializing state-space model: " << e.what() << std::endl;
        m_ext_params.method = ChannelMethod::SIMPLE;
        init_simple_model();
    }
}

void ChannelSParamTdf::extract_active_matrices() {
    int n_states = m_full_model.n_states;
    int n_active_in = m_port_config.active_inputs.size();
    int n_active_out = m_port_config.active_outputs.size();
    
    m_active_ss.n_states = n_states;
    m_active_ss.n_inputs = n_active_in;
    m_active_ss.n_outputs = n_active_out;
    
    // Resize active matrices
    m_active_ss.A.resize(n_states, n_states);
    m_active_ss.B.resize(n_states, n_active_in);
    m_active_ss.C.resize(n_active_out, n_states);
    m_active_ss.D.resize(n_active_out, n_active_in);
    m_active_ss.E.resize(n_active_out, n_active_in);
    
    // A is shared (copy directly)
    m_active_ss.A = m_full_model.state_space.A;
    
    // Extract B columns for active inputs
    for (int j = 0; j < n_active_in; ++j) {
        int src_col = m_port_config.active_inputs[j] + 1;  // 1-based for sca_matrix
        for (int i = 1; i <= n_states; ++i) {
            m_active_ss.B(i, j + 1) = m_full_model.state_space.B(i, src_col);
        }
    }
    
    // Extract C rows for active outputs
    for (int i = 0; i < n_active_out; ++i) {
        int src_row = m_port_config.active_outputs[i] + 1;  // 1-based
        for (int j = 1; j <= n_states; ++j) {
            m_active_ss.C(i + 1, j) = m_full_model.state_space.C(src_row, j);
        }
    }
    
    // Extract D submatrix
    for (int i = 0; i < n_active_out; ++i) {
        int src_row = m_port_config.active_outputs[i] + 1;
        for (int j = 0; j < n_active_in; ++j) {
            int src_col = m_port_config.active_inputs[j] + 1;
            m_active_ss.D(i + 1, j + 1) = m_full_model.state_space.D(src_row, src_col);
        }
    }
    
    // Extract E submatrix
    for (int i = 0; i < n_active_out; ++i) {
        int src_row = m_port_config.active_outputs[i] + 1;
        for (int j = 0; j < n_active_in; ++j) {
            int src_col = m_port_config.active_inputs[j] + 1;
            m_active_ss.E(i + 1, j + 1) = m_full_model.state_space.E(src_row, src_col);
        }
    }
    
    // Initialize state vector
    m_ss_state.resize(n_states);
    for (int i = 1; i <= n_states; ++i) {
        m_ss_state(i) = 0.0;
    }
    
    std::cout << "[DEBUG] ChannelSParamTdf: Active matrices extracted" << std::endl;
    std::cout << "[DEBUG]   B: " << n_states << "x" << n_active_in << std::endl;
    std::cout << "[DEBUG]   C: " << n_active_out << "x" << n_states << std::endl;
}

void ChannelSParamTdf::process_state_space_mimo() {
    int n_in = m_port_config.active_inputs.size();
    int n_out = m_port_config.active_outputs.size();
    
    // Read inputs into vector
    sca_util::sca_vector<double> u(n_in);
    for (int i = 0; i < n_in; ++i) {
        u(i + 1) = in[i].read();
    }
    
    // State-space computation using sca_ss
    // y = C*x + D*u + E*du/dt
    sca_util::sca_vector<double> y = m_ss_filter(
        m_active_ss.A, m_active_ss.B, m_active_ss.C,
        m_active_ss.D, m_ss_state, u, get_timestep());
    
    // Write outputs
    for (int i = 0; i < n_out; ++i) {
        out[i].write(y(i + 1));
    }
}


void ChannelSParamTdf::init_fft_convolution() {
    int L = static_cast<int>(m_impulse_data.impulse.size());
    
    // Choose FFT size (next power of 2 >= 2*L)
    m_fft_size = 1;
    while (m_fft_size < 2 * L) {
        m_fft_size *= 2;
    }
    
    m_block_size = m_fft_size - L + 1;
    
    // Pre-compute FFT of impulse response (zero-padded)
    std::vector<double> h_padded(m_fft_size, 0.0);
    for (int i = 0; i < L; ++i) {
        h_padded[i] = m_impulse_data.impulse[i];
    }
    
    fft_real(h_padded, m_H_fft_real, m_H_fft_imag);
    
    // Allocate input block buffer
    m_input_block.resize(m_fft_size, 0.0);
    m_block_idx = 0;
    
    // Clear output queue
    while (!m_output_queue.empty()) {
        m_output_queue.pop_front();
    }
}

// ============================================================================
// Signal Processing Methods
// ============================================================================

double ChannelSParamTdf::process_simple(double x) {
    // Attenuation
    double attenuation_linear = std::pow(10.0, -m_params.attenuation_db / 20.0);
    
    // First-order low-pass filter
    m_filter_state = m_alpha * x + (1.0 - m_alpha) * m_filter_state;
    
    return attenuation_linear * m_filter_state;
}

double ChannelSParamTdf::process_rational(double x) {
    // Use sca_ltf_nd for rational function filtering
    // H(s) = num(s) / den(s)
    
    // The sca_ltf_nd operator computes the Laplace transfer function
    // Correct usage: pass numerator, denominator coefficients and input only
    double y = m_ltf_filter(m_num_vec, m_den_vec, x);
    return y;
}

double ChannelSParamTdf::process_impulse(double x) {
    // Direct convolution using circular buffer
    // y[n] = sum_{k=0}^{L-1} h[k] * x[n-k]
    
    int L = static_cast<int>(m_impulse_data.impulse.size());
    
    // Store new input in delay line
    m_delay_line[m_delay_idx] = x;
    
    // Compute convolution
    double y = 0.0;
    for (int k = 0; k < L; ++k) {
        int buf_pos = (m_delay_idx - k + L) % L;
        y += m_impulse_data.impulse[k] * m_delay_line[buf_pos];
    }
    
    // Update delay line index
    m_delay_idx = (m_delay_idx + 1) % L;
    
    return y;
}

double ChannelSParamTdf::get_dc_gain() const {
    switch (m_ext_params.method) {
        case ChannelMethod::SIMPLE:
            return std::pow(10.0, -m_params.attenuation_db / 20.0);
        case ChannelMethod::RATIONAL:
            return m_rational_data.dc_gain;
        case ChannelMethod::IMPULSE:
            // DC gain is sum of impulse response
            {
                double sum = 0.0;
                for (double h : m_impulse_data.impulse) {
                    sum += h;
                }
                return sum * m_impulse_data.dt;
            }
        case ChannelMethod::STATE_SPACE:
            // DC gain for state-space: D - C * inv(A) * B
            if (m_active_ss.n_states > 0 && m_active_ss.n_outputs > 0) {
                // Compute DC gain = D - C * A^-1 * B
                // For single-input single-output: compute directly
                // Solve A * x = B for x, then DC = D - C * x
                try {
                    // Simple implementation for small matrices
                    // Compute A^-1 * B using Gaussian elimination
                    int n = m_active_ss.n_states;
                    std::vector<std::vector<double>> A_inv(n, std::vector<double>(n, 0.0));
                    
                    // Initialize identity matrix
                    for (int i = 0; i < n; ++i) {
                        A_inv[i][i] = 1.0;
                    }
                    
                    // Create augmented matrix [A | I]
                    std::vector<std::vector<double>> aug(n, std::vector<double>(2 * n, 0.0));
                    for (int i = 0; i < n; ++i) {
                        for (int j = 0; j < n; ++j) {
                            aug[i][j] = m_active_ss.A(i + 1, j + 1);
                        }
                        aug[i][n + i] = 1.0;
                    }
                    
                    // Gaussian elimination
                    for (int i = 0; i < n; ++i) {
                        // Partial pivoting
                        double max_val = std::abs(aug[i][i]);
                        int max_row = i;
                        for (int k = i + 1; k < n; ++k) {
                            if (std::abs(aug[k][i]) > max_val) {
                                max_val = std::abs(aug[k][i]);
                                max_row = k;
                            }
                        }
                        std::swap(aug[i], aug[max_row]);
                        
                        // Check for singular matrix
                        if (std::abs(aug[i][i]) < 1e-15) {
                            return m_active_ss.D(1, 1);  // Return D if A is singular
                        }
                        
                        // Normalize row
                        double pivot = aug[i][i];
                        for (int j = i; j < 2 * n; ++j) {
                            aug[i][j] /= pivot;
                        }
                        
                        // Eliminate column
                        for (int k = 0; k < n; ++k) {
                            if (k != i) {
                                double factor = aug[k][i];
                                for (int j = i; j < 2 * n; ++j) {
                                    aug[k][j] -= factor * aug[i][j];
                                }
                            }
                        }
                    }
                    
                    // Extract inverse
                    for (int i = 0; i < n; ++i) {
                        for (int j = 0; j < n; ++j) {
                            A_inv[i][j] = aug[i][n + j];
                        }
                    }
                    
                    // Compute A_inv * B (first column of B for SISO)
                    std::vector<double> A_inv_B(n, 0.0);
                    for (int i = 0; i < n; ++i) {
                        for (int j = 0; j < n; ++j) {
                            A_inv_B[i] += A_inv[i][j] * m_active_ss.B(j + 1, 1);
                        }
                    }
                    
                    // Compute C * A_inv * B (first output for SISO)
                    double C_Ainv_B = 0.0;
                    for (int j = 0; j < n; ++j) {
                        C_Ainv_B += m_active_ss.C(1, j + 1) * A_inv_B[j];
                    }
                    
                    return m_active_ss.D(1, 1) - C_Ainv_B;
                } catch (...) {
                    return m_active_ss.D(1, 1);  // Return D on error
                }
            }
            return 1.0;
        default:
            return 1.0;
    }
}

} // namespace serdes


namespace serdes {

// ============================================================================
// FFT Implementation - Cooley-Tukey algorithm
// ============================================================================

// Helper: Bit reversal permutation
static void bit_reverse_copy(const std::vector<std::complex<double>>& in,
                              std::vector<std::complex<double>>& out) {
    int N = static_cast<int>(in.size());
    out.resize(N);
    int logN = static_cast<int>(std::log2(N));
    
    for (int i = 0; i < N; ++i) {
        int j = 0;
        for (int k = 0; k < logN; ++k) {
            j = (j << 1) | ((i >> k) & 1);
        }
        out[j] = in[i];
    }
}

// Cooley-Tukey iterative FFT
static void fft_cooley_tukey(std::vector<std::complex<double>>& data, bool inverse = false) {
    int N = static_cast<int>(data.size());
    if (N <= 1) return;
    
    // Check if N is power of 2
    if ((N & (N - 1)) != 0) {
        // Fall back to DFT for non-power-of-2
        std::vector<std::complex<double>> result(N, 0.0);
        for (int k = 0; k < N; ++k) {
            for (int n = 0; n < N; ++n) {
                double sign = inverse ? 1.0 : -1.0;
                double angle = sign * 2.0 * M_PI * k * n / N;
                result[k] += data[n] * std::complex<double>(std::cos(angle), std::sin(angle));
            }
        }
        if (inverse) {
            for (int k = 0; k < N; ++k) {
                result[k] /= N;
            }
        }
        data = result;
        return;
    }
    
    // Bit reversal permutation
    std::vector<std::complex<double>> temp;
    bit_reverse_copy(data, temp);
    
    // Iterative FFT
    double sign = inverse ? 1.0 : -1.0;
    for (int len = 2; len <= N; len <<= 1) {
        double angle = sign * 2.0 * M_PI / len;
        std::complex<double> wlen(std::cos(angle), std::sin(angle));
        
        for (int i = 0; i < N; i += len) {
            std::complex<double> w(1.0);
            for (int j = 0; j < len / 2; ++j) {
                std::complex<double> u = temp[i + j];
                std::complex<double> v = temp[i + j + len/2] * w;
                temp[i + j] = u + v;
                temp[i + j + len/2] = u - v;
                w *= wlen;
            }
        }
    }
    
    if (inverse) {
        for (int i = 0; i < N; ++i) {
            temp[i] /= N;
        }
    }
    
    data = temp;
}

double ChannelSParamTdf::process_impulse_fft(double x) {
    // Overlap-save FFT convolution
    
    // Add input to block
    m_input_block[m_block_idx++] = x;
    
    // Process when block is full
    if (m_block_idx == m_block_size) {
        // Shift old samples
        int L = static_cast<int>(m_impulse_data.impulse.size());
        for (int i = 0; i < L - 1; ++i) {
            m_input_block[m_fft_size - L + 1 + i] = m_input_block[i];
        }
        
        // FFT of input block
        std::vector<double> X_real, X_imag;
        fft_real(m_input_block, X_real, X_imag);
        
        // Frequency domain multiplication
        std::vector<double> Y_real(m_fft_size), Y_imag(m_fft_size);
        for (int i = 0; i < m_fft_size; ++i) {
            // Complex multiplication: (a+bi)(c+di) = (ac-bd) + (ad+bc)i
            Y_real[i] = X_real[i] * m_H_fft_real[i] - X_imag[i] * m_H_fft_imag[i];
            Y_imag[i] = X_real[i] * m_H_fft_imag[i] + X_imag[i] * m_H_fft_real[i];
        }
        
        // IFFT
        std::vector<double> y_block;
        ifft_real(Y_real, Y_imag, y_block);
        
        // Add valid samples to output queue (discard first L-1 samples)
        int L_minus_1 = L - 1;
        for (int i = L_minus_1; i < m_fft_size; ++i) {
            m_output_queue.push_back(y_block[i]);
        }
        
        m_block_idx = 0;
    }
    
    // Return output from queue
    if (!m_output_queue.empty()) {
        double y = m_output_queue.front();
        m_output_queue.pop_front();
        return y;
    }
    
    return 0.0;
}

void ChannelSParamTdf::fft_real(const std::vector<double>& in,
                                 std::vector<double>& out_real,
                                 std::vector<double>& out_imag) {
    int N = static_cast<int>(in.size());
    out_real.resize(N);
    out_imag.resize(N);
    
    // Use Cooley-Tukey FFT (O(N log N))
    std::vector<std::complex<double>> data(N);
    for (int i = 0; i < N; ++i) {
        data[i] = std::complex<double>(in[i], 0.0);
    }
    
    fft_cooley_tukey(data, false);
    
    for (int i = 0; i < N; ++i) {
        out_real[i] = data[i].real();
        out_imag[i] = data[i].imag();
    }
}

void ChannelSParamTdf::ifft_real(const std::vector<double>& in_real,
                                  const std::vector<double>& in_imag,
                                  std::vector<double>& out) {
    int N = static_cast<int>(in_real.size());
    out.resize(N);
    
    // Use Cooley-Tukey IFFT (O(N log N))
    std::vector<std::complex<double>> data(N);
    for (int i = 0; i < N; ++i) {
        data[i] = std::complex<double>(in_real[i], in_imag[i]);
    }
    
    fft_cooley_tukey(data, true);
    
    for (int i = 0; i < N; ++i) {
        out[i] = data[i].real();
    }
}

} // namespace serdes
