#ifndef TB_RX_CTLE_HELPERS_H
#define TB_RX_CTLE_HELPERS_H

#include <systemc-ams>
#include <vector>
#include <cmath>
#include <fstream>
#include <string>
#include <random>
#include <complex>

namespace serdes {
namespace tb {

// 信号统计信息
struct SignalStats {
    double mean;
    double rms;
    double peak_to_peak;
    double min_value;
    double max_value;
};

// 差分信号源 - 支持多种波形
class DiffSignalSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out_p;
    sca_tdf::sca_out<double> out_n;
    
    enum WaveformType {
        DC,
        SINE,
        SQUARE,
        PRBS
    };
    
    DiffSignalSource(sc_core::sc_module_name nm, 
                     WaveformType type = DC,
                     double amplitude = 0.1,
                     double frequency = 1e9,
                     double vcm = 0.6,
                     double sample_rate = 100e9)
        : sca_tdf::sca_module(nm)
        , out_p("out_p")
        , out_n("out_n")
        , m_type(type)
        , m_amplitude(amplitude)
        , m_frequency(frequency)
        , m_vcm(vcm)
        , m_sample_rate(sample_rate)
        , m_timestep(1.0 / sample_rate)
        , m_step_count(0)
    {}
    
    void set_attributes() {
        out_p.set_rate(1);
        out_n.set_rate(1);
        out_p.set_timestep(m_timestep, sc_core::SC_SEC);
        out_n.set_timestep(m_timestep, sc_core::SC_SEC);
    }
    
    void processing() {
        double signal = 0.0;
        double t = m_step_count * m_timestep;
        
        switch (m_type) {
            case DC:
                signal = m_amplitude;
                break;
            case SINE:
                signal = m_amplitude * sin(2.0 * M_PI * m_frequency * t);
                break;
            case SQUARE:
                signal = m_amplitude * (sin(2.0 * M_PI * m_frequency * t) > 0 ? 1.0 : -1.0);
                break;
            case PRBS:
                // 简化的PRBS-7
                signal = m_amplitude * ((m_step_count % 127) < 64 ? 1.0 : -1.0);
                break;
        }
        
        out_p.write(m_vcm + 0.5 * signal);
        out_n.write(m_vcm - 0.5 * signal);
        m_step_count++;
    }
    
private:
    WaveformType m_type;
    double m_amplitude;
    double m_frequency;
    double m_vcm;
    double m_sample_rate;
    double m_timestep;
    unsigned long m_step_count;
};

// VDD电源模块 - 支持噪声注入
class VddSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> vdd;
    
    enum NoiseType {
        CONSTANT,      // 恒定电压
        SINUSOIDAL,    // 正弦波纹
        RANDOM         // 随机噪声
    };
    
    VddSource(sc_core::sc_module_name nm, 
              double voltage = 1.0, 
              double sample_rate = 100e9,
              NoiseType noise_type = CONSTANT,
              double noise_amplitude = 0.0,
              double noise_frequency = 1e6)
        : sca_tdf::sca_module(nm)
        , vdd("vdd")
        , m_voltage(voltage)
        , m_timestep(1.0 / sample_rate)
        , m_noise_type(noise_type)
        , m_noise_amplitude(noise_amplitude)
        , m_noise_frequency(noise_frequency)
        , m_step_count(0)
        , m_rng(std::random_device{}())
        , m_noise_dist(0.0, 1.0)
    {}
    
    void set_attributes() {
        vdd.set_rate(1);
        vdd.set_timestep(m_timestep, sc_core::SC_SEC);
    }
    
    void processing() {
        double noise = 0.0;
        double t = m_step_count * m_timestep;
        
        switch (m_noise_type) {
            case CONSTANT:
                noise = 0.0;
                break;
            case SINUSOIDAL:
                noise = m_noise_amplitude * sin(2.0 * M_PI * m_noise_frequency * t);
                break;
            case RANDOM:
                noise = m_noise_amplitude * m_noise_dist(m_rng);
                break;
        }
        
        vdd.write(m_voltage + noise);
        m_step_count++;
    }
    
private:
    double m_voltage;
    double m_timestep;
    NoiseType m_noise_type;
    double m_noise_amplitude;
    double m_noise_frequency;
    unsigned long m_step_count;
    std::mt19937 m_rng;
    std::normal_distribution<double> m_noise_dist;
};

// 信号监测模块 - 记录波形数据
class SignalMonitor : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> in_p;
    sca_tdf::sca_in<double> in_n;
    
    SignalMonitor(sc_core::sc_module_name nm, 
                  const std::string& filename = "",
                  double sample_rate = 100e9)
        : sca_tdf::sca_module(nm)
        , in_p("in_p")
        , in_n("in_n")
        , m_filename(filename)
        , m_timestep(1.0 / sample_rate)
        , m_step_count(0)
    {
        if (!m_filename.empty()) {
            m_file.open(m_filename);
            m_file << "time,diff,cm\n";
        }
    }
    
    ~SignalMonitor() {
        if (m_file.is_open()) {
            m_file.close();
        }
    }
    
    void set_attributes() {
        in_p.set_rate(1);
        in_n.set_rate(1);
        in_p.set_timestep(m_timestep, sc_core::SC_SEC);
        in_n.set_timestep(m_timestep, sc_core::SC_SEC);
    }
    
    void processing() {
        double vp = in_p.read(0);
        double vn = in_n.read(0);
        double diff = vp - vn;
        double cm = 0.5 * (vp + vn);
        
        m_diff_samples.push_back(diff);
        m_cm_samples.push_back(cm);
        
        if (m_file.is_open()) {
            double t = m_step_count * m_timestep;
            m_file << t << "," << diff << "," << cm << "\n";
        }
        
        m_step_count++;
    }
    
    SignalStats get_diff_stats() const {
        return calculate_stats(m_diff_samples);
    }
    
    SignalStats get_cm_stats() const {
        return calculate_stats(m_cm_samples);
    }
    
private:
    std::string m_filename;
    std::ofstream m_file;
    double m_timestep;
    unsigned long m_step_count;
    std::vector<double> m_diff_samples;
    std::vector<double> m_cm_samples;
    
    SignalStats calculate_stats(const std::vector<double>& samples) const {
        SignalStats stats = {0, 0, 0, 1e9, -1e9};
        
        if (samples.empty()) return stats;
        
        double sum = 0.0;
        double sum_sq = 0.0;
        
        for (double v : samples) {
            sum += v;
            sum_sq += v * v;
            if (v < stats.min_value) stats.min_value = v;
            if (v > stats.max_value) stats.max_value = v;
        }
        
        stats.mean = sum / samples.size();
        stats.rms = sqrt(sum_sq / samples.size());
        stats.peak_to_peak = stats.max_value - stats.min_value;
        
        return stats;
    }
};

// 差分信号源（支持共模变化） - 用于CMRR测试
class DiffSourceWithCMVariation : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out_p;
    sca_tdf::sca_out<double> out_n;
    
    DiffSourceWithCMVariation(sc_core::sc_module_name nm,
                              double diff_amplitude = 0.1,
                              double diff_frequency = 1e9,
                              double vcm_base = 0.6,
                              double vcm_variation = 0.0,
                              double vcm_frequency = 1e6,
                              double sample_rate = 100e9)
        : sca_tdf::sca_module(nm)
        , out_p("out_p")
        , out_n("out_n")
        , m_diff_amplitude(diff_amplitude)
        , m_diff_frequency(diff_frequency)
        , m_vcm_base(vcm_base)
        , m_vcm_variation(vcm_variation)
        , m_vcm_frequency(vcm_frequency)
        , m_timestep(1.0 / sample_rate)
        , m_step_count(0)
    {}
    
    void set_attributes() {
        out_p.set_rate(1);
        out_n.set_rate(1);
        out_p.set_timestep(m_timestep, sc_core::SC_SEC);
        out_n.set_timestep(m_timestep, sc_core::SC_SEC);
    }
    
    void processing() {
        double t = m_step_count * m_timestep;
        
        // 差分信号
        double diff_signal = m_diff_amplitude * sin(2.0 * M_PI * m_diff_frequency * t);
        
        // 共模电压（带变化）
        double vcm = m_vcm_base + m_vcm_variation * sin(2.0 * M_PI * m_vcm_frequency * t);
        
        out_p.write(vcm + 0.5 * diff_signal);
        out_n.write(vcm - 0.5 * diff_signal);
        m_step_count++;
    }
    
private:
    double m_diff_amplitude;
    double m_diff_frequency;
    double m_vcm_base;
    double m_vcm_variation;
    double m_vcm_frequency;
    double m_timestep;
    unsigned long m_step_count;
};

// 频率响应分析器 - 用于测量传递函数特性
struct FrequencyResponsePoint {
    double frequency;     // 测试频率 (Hz)
    double gain;          // 增益 (linear)
    double gain_db;       // 增益 (dB)
    double phase_deg;     // 相位 (度)
};

class FrequencyResponseAnalyzer {
public:
    // 从输入输出样本计算单一频率点的增益
    static double calculate_gain(const std::vector<double>& input_samples,
                                 const std::vector<double>& output_samples) {
        if (input_samples.empty() || output_samples.empty()) return 0.0;
        
        // 计算RMS
        double input_rms = 0.0, output_rms = 0.0;
        size_t n = std::min(input_samples.size(), output_samples.size());
        
        for (size_t i = 0; i < n; ++i) {
            input_rms += input_samples[i] * input_samples[i];
            output_rms += output_samples[i] * output_samples[i];
        }
        
        input_rms = sqrt(input_rms / n);
        output_rms = sqrt(output_rms / n);
        
        if (input_rms < 1e-12) return 0.0;
        return output_rms / input_rms;
    }
    
    // 计算理论频率响应（用于比较）
    static double theoretical_gain(double frequency,
                                   const std::vector<double>& zeros,
                                   const std::vector<double>& poles,
                                   double dc_gain) {
        std::complex<double> H(dc_gain, 0.0);
        std::complex<double> jw(0.0, 2.0 * M_PI * frequency);
        
        // 分子: prod(1 + s/wz)
        for (double fz : zeros) {
            if (fz > 0.0) {
                double wz = 2.0 * M_PI * fz;
                H *= (1.0 + jw / wz);
            }
        }
        
        // 分母: prod(1 + s/wp)
        for (double fp : poles) {
            if (fp > 0.0) {
                double wp = 2.0 * M_PI * fp;
                H /= (1.0 + jw / wp);
            }
        }
        
        return std::abs(H);
    }
    
    static double gain_to_db(double gain) {
        if (gain <= 0.0) return -100.0;
        return 20.0 * log10(gain);
    }
};

} // namespace tb
} // namespace serdes

#endif // TB_RX_CTLE_HELPERS_H
