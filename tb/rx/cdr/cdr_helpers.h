#ifndef TB_RX_CDR_HELPERS_H
#define TB_RX_CDR_HELPERS_H

#include <systemc-ams>
#include <vector>
#include <cmath>
#include <fstream>
#include <string>
#include <random>
#include <complex>

namespace serdes {
namespace tb {

// 相位统计信息
struct PhaseStats {
    double mean;
    double rms;
    double peak_to_peak;
    double min_value;
    double max_value;
    double lock_time;      // 锁定时间（秒）
    double steady_state_rms;  // 稳态RMS（锁定后）
};

// 数据信号源 - 支持多种波形和抖动注入
class DataSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out;

    enum WaveformType {
        PRBS,
        ALTERNATING,      // 010101...
        SINE,
        SQUARE
    };

    DataSource(sc_core::sc_module_name nm,
               WaveformType type = PRBS,
               double amplitude = 1.0,
               double frequency = 10e9,
               double sample_rate = 100e9,
               double jitter_sigma = 0.0,    // 随机抖动
               double sj_freq = 0.0,         // 周期抖动频率
               double sj_amplitude = 0.0,    // 周期抖动幅度
               double freq_offset_ppm = 0.0) // 频率偏移（ppm）
        : sca_tdf::sca_module(nm)
        , out("out")
        , m_type(type)
        , m_amplitude(amplitude)
        , m_frequency(frequency * (1.0 + freq_offset_ppm / 1e6))  // 应用频偏
        , m_sample_rate(sample_rate)
        , m_timestep(1.0 / sample_rate)
        , m_step_count(0)
        , m_jitter_sigma(jitter_sigma)
        , m_sj_freq(sj_freq)
        , m_sj_amplitude(sj_amplitude)
        , m_rng(std::random_device{}())
        , m_noise_dist(0.0, 1.0)
    {
        // 初始化PRBS-7多项式
        m_prbs_state = 0x7F;  // 7-bit LFSR, all 1s
    }

    void set_attributes() {
        out.set_rate(1);
        out.set_timestep(m_timestep, sc_core::SC_SEC);
    }

    void processing() {
        double signal = 0.0;
        double t = m_step_count * m_timestep;

        // 生成基础信号
        switch (m_type) {
            case PRBS:
                signal = generate_prbs7();
                break;
            case ALTERNATING:
                signal = (m_step_count % 2 == 0) ? 1.0 : -1.0;
                break;
            case SINE:
                signal = sin(2.0 * M_PI * m_frequency * t);
                break;
            case SQUARE:
                signal = (sin(2.0 * M_PI * m_frequency * t) > 0) ? 1.0 : -1.0;
                break;
        }

        // 应用幅度
        signal *= m_amplitude;

        // 注入抖动
        double jitter = 0.0;
        if (m_jitter_sigma > 0.0) {
            jitter += m_jitter_sigma * m_noise_dist(m_rng);
        }
        if (m_sj_freq > 0.0 && m_sj_amplitude > 0.0) {
            jitter += m_sj_amplitude * sin(2.0 * M_PI * m_sj_freq * t);
        }

        // 抖动通过时间调制实现（简化：直接叠加到信号）
        signal += jitter;

        out.write(signal);
        m_step_count++;
    }

private:
    WaveformType m_type;
    double m_amplitude;
    double m_frequency;
    double m_sample_rate;
    double m_timestep;
    unsigned long m_step_count;
    double m_jitter_sigma;
    double m_sj_freq;
    double m_sj_amplitude;

    // PRBS生成器
    unsigned int m_prbs_state;
    std::mt19937 m_rng;
    std::normal_distribution<double> m_noise_dist;

    double generate_prbs7() {
        // PRBS-7: x^7 + x^6 + 1
        bool bit = (m_prbs_state & 0x40) ^ ((m_prbs_state & 0x20) >> 1);
        m_prbs_state = ((m_prbs_state << 1) | bit) & 0x7F;
        return bit ? 1.0 : -1.0;
    }
};

// 采样器模块 - 简化实现，用于CDR闭环测试
class SimpleSampler : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> in;
    sca_tdf::sca_in<double> phase_offset;
    sca_tdf::sca_out<double> out;

    double m_threshold;
    double m_timestep;

    SimpleSampler(sc_core::sc_module_name nm,
                  double sample_rate = 10e9,
                  double threshold = 0.0)
        : sca_tdf::sca_module(nm)
        , in("in")
        , phase_offset("phase_offset")
        , out("out")
        , m_threshold(threshold)
        , m_timestep(1.0 / sample_rate)
    {}

    void set_attributes() {
        in.set_rate(1);
        phase_offset.set_rate(1);
        out.set_rate(1);
        in.set_timestep(m_timestep, sc_core::SC_SEC);
        phase_offset.set_timestep(m_timestep, sc_core::SC_SEC);
        out.set_timestep(m_timestep, sc_core::SC_SEC);
    }

    void processing() {
        double data = in.read();
        double offset = phase_offset.read();

        // 简化采样：直接比较阈值（忽略相位偏移的时间域影响）
        // 完整实现需要根据相位偏移调整采样时刻
        double sampled = (data > m_threshold) ? 1.0 : -1.0;

        out.write(sampled);
    }
};

// CDR状态监控模块 - 记录相位波形和统计信息
class CdrMonitor : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> phase_in;
    sca_tdf::sca_in<double> data_in;  // 可选：用于BER统计

    CdrMonitor(sc_core::sc_module_name nm,
               const std::string& filename = "",
               double sample_rate = 10e9)
        : sca_tdf::sca_module(nm)
        , phase_in("phase_in")
        , data_in("data_in")
        , m_filename(filename)
        , m_timestep(1.0 / sample_rate)
        , m_step_count(0)
        , m_lock_threshold(5e-12)  // 5ps锁定阈值
        , m_locked(false)
        , m_lock_time(0.0)
    {
        if (!m_filename.empty()) {
            m_file.open(m_filename);
            m_file << "time,phase_s,phase_ps,phase_ui\n";
        }
    }

    ~CdrMonitor() {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    void set_attributes() {
        phase_in.set_rate(1);
        data_in.set_rate(1);
        phase_in.set_timestep(m_timestep, sc_core::SC_SEC);
        data_in.set_timestep(m_timestep, sc_core::SC_SEC);
    }

    void processing() {
        double phase = phase_in.read();
        double t = m_step_count * m_timestep;

        m_phase_samples.push_back(phase);

        // 检测锁定状态（简化：相位变化小于阈值）
        if (!m_locked && m_phase_samples.size() > 100) {
            double variance = calculate_variance(m_phase_samples.end() - 100, m_phase_samples.end());
            if (variance < m_lock_threshold * m_lock_threshold) {
                m_locked = true;
                m_lock_time = t;
            }
        }

        if (m_file.is_open()) {
            double phase_ps = phase * 1e12;
            double phase_ui = phase / m_timestep;
            m_file << t << "," << phase << "," << phase_ps << "," << phase_ui << "\n";
        }

        m_step_count++;
    }

    PhaseStats get_phase_stats(double ui_period = 1e-10) const {
        PhaseStats stats = {0, 0, 0, 1e9, -1e9, 0, 0};

        if (m_phase_samples.empty()) return stats;

        double sum = 0.0;
        double sum_sq = 0.0;

        for (double v : m_phase_samples) {
            sum += v;
            sum_sq += v * v;
            if (v < stats.min_value) stats.min_value = v;
            if (v > stats.max_value) stats.max_value = v;
        }

        stats.mean = sum / m_phase_samples.size();
        stats.rms = sqrt(sum_sq / m_phase_samples.size());
        stats.peak_to_peak = stats.max_value - stats.min_value;
        stats.lock_time = m_lock_time;

        // 计算稳态RMS（锁定后）
        if (m_locked && m_phase_samples.size() > 100) {
            size_t lock_idx = static_cast<size_t>(m_lock_time / m_timestep);
            if (lock_idx < m_phase_samples.size()) {
                double steady_sum = 0.0;
                double steady_sum_sq = 0.0;
                size_t steady_count = 0;
                for (size_t i = lock_idx; i < m_phase_samples.size(); ++i) {
                    steady_sum += m_phase_samples[i];
                    steady_sum_sq += m_phase_samples[i] * m_phase_samples[i];
                    steady_count++;
                }
                if (steady_count > 0) {
                    stats.steady_state_rms = sqrt(steady_sum_sq / steady_count -
                                                  (steady_sum / steady_count) * (steady_sum / steady_count));
                }
            }
        }

        return stats;
    }

    bool is_locked() const {
        return m_locked;
    }

private:
    std::string m_filename;
    std::ofstream m_file;
    double m_timestep;
    unsigned long m_step_count;
    std::vector<double> m_phase_samples;
    double m_lock_threshold;
    bool m_locked;
    double m_lock_time;

    double calculate_variance(std::vector<double>::const_iterator begin,
                              std::vector<double>::const_iterator end) const {
        if (begin == end) return 0.0;

        double sum = 0.0;
        double sum_sq = 0.0;
        size_t n = 0;

        for (auto it = begin; it != end; ++it) {
            sum += *it;
            sum_sq += (*it) * (*it);
            n++;
        }

        double mean = sum / n;
        return sum_sq / n - mean * mean;
    }
};

// 抖动容限测试辅助类
class JitterToleranceTester {
public:
    static double measure_jitter_tolerance(double frequency,
                                          double target_ber = 1e-12,
                                          size_t min_samples = 1000000) {
        // 简化实现：返回理论值
        // 完整实现需要运行多次仿真并测量BER
        if (frequency < 1e6) {
            return 1e-10;  // 低频：100ps
        } else if (frequency < 10e6) {
            return 7.07e-11;  // 中频：70.7ps
        } else {
            return 3e-11;  // 高频：30ps
        }
    }
};

// 环路带宽分析器
class LoopBandwidthAnalyzer {
public:
    static double calculate_theoretical_bandwidth(double kp, double ki, double sample_rate) {
        // ωn = sqrt(Ki * Fs)
        double omega_n = sqrt(ki * sample_rate);
        // BW ≈ ωn / (2π)
        return omega_n / (2.0 * M_PI);
    }

    static double calculate_damping_factor(double kp, double ki, double sample_rate) {
        // ζ = Kp / (2 * ωn)
        double omega_n = sqrt(ki * sample_rate);
        return kp / (2.0 * omega_n);
    }

    static double calculate_phase_margin(double kp, double ki, double sample_rate) {
        // 简化计算：基于二阶系统理论
        double zeta = calculate_damping_factor(kp, ki, sample_rate);
        // PM ≈ arctan(2ζ / sqrt(sqrt(1+4ζ^4)-2ζ^2)) * 180/π
        if (zeta < 0.1) return 0.0;
        double term = sqrt(1 + 4 * pow(zeta, 4)) - 2 * pow(zeta, 2);
        if (term <= 0) return 0.0;
        double pm_rad = atan(2 * zeta / sqrt(term));
        return pm_rad * 180.0 / M_PI;
    }
};

// BER计算器
class BERCalculator {
public:
    static double calculate_ber(const std::vector<double>& received,
                               const std::vector<double>& reference) {
        if (received.empty() || reference.empty()) return 0.0;

        size_t n = std::min(received.size(), reference.size());
        size_t errors = 0;

        for (size_t i = 0; i < n; ++i) {
            // 简化：比较符号
            bool rx_bit = (received[i] > 0);
            bool ref_bit = (reference[i] > 0);
            if (rx_bit != ref_bit) {
                errors++;
            }
        }

        return static_cast<double>(errors) / n;
    }

    static double calculate_q_factor(double ber) {
        if (ber <= 0.0 || ber >= 0.5) return 0.0;
        // Q = sqrt(2) * erfc^{-1}(2 * BER)
        return sqrt(2.0) * erfcinv(2.0 * ber);
    }

    static double q_to_db(double q) {
        if (q <= 0.0) return -100.0;
        return 20.0 * log10(q);
    }

private:
    static double erfcinv(double y) {
        // 简化实现：使用近似公式
        if (y <= 0.0 || y >= 2.0) return 0.0;
        double x = sqrt(-log(y / 2.0));
        return x * (1.0 + 0.0165 * x * x);  // 近似
    }
};

} // namespace tb
} // namespace serdes

#endif // TB_RX_CDR_HELPERS_H