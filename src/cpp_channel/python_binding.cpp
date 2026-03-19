/**
 * @file python_binding.cpp
 * @brief pybind11 bindings for PoleResidueFilter
 * 
 * This allows the C++ filter to be used directly from Python for:
 * - Algorithm validation against scipy.signal
 * - Frequency response analysis
 * - Batch waveform processing
 * 
 * Usage:
 *   import serdes_channel as ch
 *   filter = ch.PoleResidueFilter()
 *   filter.init(poles, residues, constant, proportional, fs)
 *   output = filter.process_block(input_array)
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/complex.h>

#include "pole_residue_filter.h"

namespace py = pybind11;
using namespace serdes::cpp;

PYBIND11_MODULE(serdes_channel, m) {
    m.doc() = "SerDes Channel Model (C++ implementation)";
    
    // PoleResidueFilter class
    py::class_<PoleResidueFilter>(m, "PoleResidueFilter")
        .def(py::init<>(), "Create a new PoleResidueFilter")
        
        .def("init", 
             static_cast<bool (PoleResidueFilter::*)(
                 const std::vector<std::complex<double>>&,
                 const std::vector<std::complex<double>>&,
                 double, double, double)>(
                 &PoleResidueFilter::init),
             py::arg("poles"), py::arg("residues"),
             py::arg("constant"), py::arg("proportional"), py::arg("fs"),
             "Initialize filter from complex pole/residue vectors")
        
        .def("init", 
             static_cast<bool (PoleResidueFilter::*)(
                 const std::vector<double>&, const std::vector<double>&,
                 const std::vector<double>&, const std::vector<double>&,
                 double, double, double)>(
                 &PoleResidueFilter::init),
             py::arg("poles_real"), py::arg("poles_imag"),
             py::arg("residues_real"), py::arg("residues_imag"),
             py::arg("constant"), py::arg("proportional"), py::arg("fs"),
             "Initialize filter from real/imag arrays")
        
        .def("process", &PoleResidueFilter::process,
             py::arg("input"),
             "Process single sample")
        
        .def("process_block", [](PoleResidueFilter& self, py::array_t<double> input) {
            auto buf = input.request();
            int n_samples = static_cast<int>(buf.size);
            
            py::array_t<double> output(n_samples);
            auto out_buf = output.request();
            
            self.process_block(static_cast<double*>(buf.ptr),
                               static_cast<double*>(out_buf.ptr),
                               n_samples);
            
            return output;
        }, py::arg("input"), "Process block of samples")
        
        .def("reset", &PoleResidueFilter::reset,
             "Reset internal state to zero")
        
        .def("get_dc_gain", &PoleResidueFilter::get_dc_gain,
             "Get DC gain (H(0))")
        
        .def("get_num_sections", &PoleResidueFilter::get_num_sections,
             "Get number of state-space sections")
        
        .def("is_initialized", &PoleResidueFilter::is_initialized,
             "Check if filter is initialized")
        
        .def("get_fs", &PoleResidueFilter::get_fs,
             "Get sampling frequency")
        
        .def("get_frequency_response", [](const PoleResidueFilter& self,
                                           py::array_t<double> frequencies) {
            auto buf = frequencies.request();
            int n_freqs = static_cast<int>(buf.size);
            
            py::array_t<double> mag(n_freqs);
            py::array_t<double> phase(n_freqs);
            
            self.get_frequency_response(static_cast<double*>(buf.ptr), n_freqs,
                                         static_cast<double*>(mag.request().ptr),
                                         static_cast<double*>(phase.request().ptr),
                                         nullptr, nullptr);
            
            return py::make_tuple(mag, phase);
        }, py::arg("frequencies"), "Get frequency response (magnitude and phase)");
    
    // Utility function to load from JSON
    m.def("load_from_json", [](const std::string& json_path, double fs) {
        // This would load a JSON file and return an initialized filter
        // Implementation omitted for brevity
        throw std::runtime_error("Not yet implemented");
    }, py::arg("json_path"), py::arg("fs"));
}
