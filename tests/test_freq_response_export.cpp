/**
 * @file test_freq_response_export.cpp
 * @brief Export frequency response from C++ PoleResidueFilter for validation
 * 
 * This test loads a pole-residue JSON config and exports the frequency response
 * to a CSV file for comparison with scikit-rf.
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <complex>
#include <cmath>
#include <string>
#include <iomanip>
#include "pole_residue_filter.h"

using namespace serdes::cpp;

// Simple JSON parser for pole-residue config
struct PoleResidueConfig {
    std::vector<double> poles_real;
    std::vector<double> poles_imag;
    std::vector<double> residues_real;
    std::vector<double> residues_imag;
    double constant;
    double proportional;
    double fs;
};

// Parse simple JSON (assumes well-formed input)
bool parse_json(const std::string& filename, PoleResidueConfig& config) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    std::string json_str;
    while (std::getline(file, line)) {
        json_str += line;
    }
    file.close();
    
    // Simple parsing - find arrays and values
    auto find_array = [&](const std::string& key, std::vector<double>& arr) {
        size_t pos = json_str.find(key);
        if (pos == std::string::npos) return;
        pos = json_str.find('[', pos);
        if (pos == std::string::npos) return;
        size_t end = json_str.find(']', pos);
        if (end == std::string::npos) return;
        
        std::string content = json_str.substr(pos + 1, end - pos - 1);
        size_t start = 0;
        while (start < content.length()) {
            size_t comma = content.find(',', start);
            if (comma == std::string::npos) comma = content.length();
            std::string val = content.substr(start, comma - start);
            // Trim whitespace
            size_t first = val.find_first_not_of(" \t\n\r");
            size_t last = val.find_last_not_of(" \t\n\r");
            if (first != std::string::npos) {
                arr.push_back(std::stod(val.substr(first, last - first + 1)));
            }
            start = comma + 1;
        }
    };
    
    auto find_double = [&](const std::string& key, double& val) {
        size_t pos = json_str.find(key);
        if (pos == std::string::npos) return;
        pos = json_str.find(':', pos);
        if (pos == std::string::npos) return;
        // Find number start
        pos++;
        while (pos < json_str.length() && (json_str[pos] == ' ' || json_str[pos] == '\t')) pos++;
        size_t end = pos;
        while (end < json_str.length() && (std::isdigit(json_str[end]) || json_str[end] == '.' || 
               json_str[end] == 'e' || json_str[end] == 'E' || json_str[end] == '-' || json_str[end] == '+')) {
            end++;
        }
        val = std::stod(json_str.substr(pos, end - pos));
    };
    
    find_array("\"poles_real\":", config.poles_real);
    find_array("\"poles_imag\":", config.poles_imag);
    find_array("\"residues_real\":", config.residues_real);
    find_array("\"residues_imag\":", config.residues_imag);
    find_double("\"constant\":", config.constant);
    find_double("\"proportional\":", config.proportional);
    find_double("\"fs\":", config.fs);
    
    return config.poles_real.size() > 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <config.json> <output.csv> [n_points]" << std::endl;
        std::cerr << "  config.json: Pole-residue configuration file" << std::endl;
        std::cerr << "  output.csv: Output CSV file (freq_hz, mag_db, phase_deg)" << std::endl;
        std::cerr << "  n_points: Number of frequency points (default: 1000)" << std::endl;
        return 1;
    }
    
    std::string config_file = argv[1];
    std::string output_file = argv[2];
    int n_points = (argc > 3) ? std::stoi(argv[3]) : 1000;
    
    std::cout << "========================================" << std::endl;
    std::cout << "C++ Frequency Response Export" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Load configuration
    PoleResidueConfig config;
    if (!parse_json(config_file, config)) {
        std::cerr << "Failed to parse config file" << std::endl;
        return 1;
    }
    
    std::cout << "Loaded config:" << std::endl;
    std::cout << "  Poles: " << config.poles_real.size() << std::endl;
    std::cout << "  Constant: " << config.constant << std::endl;
    std::cout << "  Proportional: " << config.proportional << std::endl;
    std::cout << "  Fs: " << config.fs / 1e9 << " GHz" << std::endl;
    
    // Initialize filter
    PoleResidueFilter filter;
    if (!filter.init(config.poles_real, config.poles_imag,
                     config.residues_real, config.residues_imag,
                     config.constant, config.proportional, config.fs)) {
        std::cerr << "Failed to initialize filter" << std::endl;
        return 1;
    }
    
    std::cout << "  Sections: " << filter.get_num_sections() << std::endl;
    std::cout << "  DC gain: " << filter.get_dc_gain() << std::endl;
    
    // Generate frequency points (log scale from 1 MHz to Fs/2)
    double f_min = 1e6;
    double f_max = config.fs / 2;
    std::vector<double> freqs(n_points);
    
    for (int i = 0; i < n_points; i++) {
        double t = static_cast<double>(i) / (n_points - 1);
        freqs[i] = f_min * std::pow(f_max / f_min, t);
    }
    
    // Compute frequency response
    std::vector<double> mag(n_points);
    std::vector<double> phase(n_points);
    std::vector<double> real(n_points);
    std::vector<double> imag(n_points);
    
    filter.get_frequency_response(freqs.data(), n_points,
                                   mag.data(), phase.data(),
                                   real.data(), imag.data());
    
    // Convert magnitude to dB
    std::vector<double> mag_db(n_points);
    for (int i = 0; i < n_points; i++) {
        mag_db[i] = 20.0 * std::log10(mag[i] + 1e-20);
    }
    
    // Export to CSV
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Failed to create output file: " << output_file << std::endl;
        return 1;
    }
    
    out << "freq_hz,mag,mag_db,phase_deg,real,imag" << std::endl;
    for (int i = 0; i < n_points; i++) {
        out << std::scientific << freqs[i] << ","
            << mag[i] << ","
            << mag_db[i] << ","
            << phase[i] << ","
            << real[i] << ","
            << imag[i] << std::endl;
    }
    out.close();
    
    std::cout << std::endl << "Exported " << n_points << " points to: " << output_file << std::endl;
    std::cout << "  Frequency range: " << f_min/1e6 << " MHz to " << f_max/1e9 << " GHz" << std::endl;
    
    // Print some sample points
    std::cout << std::endl << "Sample frequency response points:" << std::endl;
    std::cout << "  Freq (GHz) | Mag (dB) | Phase (deg)" << std::endl;
    std::cout << "  -----------|----------|------------" << std::endl;
    for (int i = 0; i < n_points; i += n_points / 10) {
        std::cout << "  " << std::setw(10) << std::fixed << std::setprecision(2) 
                  << freqs[i] / 1e9 << " | "
                  << std::setw(8) << std::setprecision(2) << mag_db[i] << " | "
                  << std::setw(10) << std::setprecision(2) << phase[i] << std::endl;
    }
    
    std::cout << std::endl << "PASSED: Frequency response exported successfully" << std::endl;
    return 0;
}
