// CDR瞬态仿真测试平台 - 支持多种测试场景
#include <systemc-ams>
#include "ams/rx_cdr.h"
#include "ams/rx_sampler.h"
#include "common/parameters.h"
#include "cdr_helpers.h"
#include <iostream>
#include <string>

using namespace serdes;
using namespace serdes::tb;

// 测试场景枚举
enum TestScenario {
    PHASE_LOCK_BASIC,     // 基本相位锁定测试
    FREQUENCY_OFFSET,     // 频率偏移捕获测试
    JITTER_TOLERANCE,     // 抖动容限测试
    PHASE_TRACKING,       // 动态相位跟踪测试
    LOOP_BANDWIDTH        // 环路带宽测量
};

// CDR瞬态仿真顶层模块 - 支持多种测试场景
SC_MODULE(CdrTransientTestbench) {
    // 模块实例
    DataSource* src;
    SimpleSampler* sampler;
    RxCdrTdf* cdr;
    CdrMonitor* monitor;

    // 信号连接
    sca_tdf::sca_signal<double> sig_data;
    sca_tdf::sca_signal<double> sig_phase_offset;
    sca_tdf::sca_signal<double> sig_sampled;
    sca_tdf::sca_signal<double> sig_phase_out;

    TestScenario m_scenario;
    CdrParams m_params;
    double m_data_rate;
    double m_ui;

    CdrTransientTestbench(sc_core::sc_module_name nm,
                         TestScenario scenario = PHASE_LOCK_BASIC)
        : sc_module(nm)
        , m_scenario(scenario)
        , m_data_rate(10e9)  // 默认10Gbps
        , m_ui(1.0 / m_data_rate)
    {
        // 配置CDR参数（默认值）
        m_params.pi.kp = 0.01;
        m_params.pi.ki = 1e-4;
        m_params.pai.resolution = 1e-12;
        m_params.pai.range = 5e-11;

        // 根据测试场景配置
        configure_scenario(scenario);

        // 创建模块
        cdr = new RxCdrTdf("cdr", m_params);
        sampler = new SimpleSampler("sampler", m_data_rate);
        monitor = new CdrMonitor("monitor", get_output_filename(scenario), m_data_rate);

        // 连接模块
        src->out(sig_data);

        sampler->in(sig_data);
        sampler->phase_offset(sig_phase_offset);
        sampler->out(sig_sampled);

        cdr->in(sig_sampled);  // CDR从采样器输出获取数据
        cdr->phase_out(sig_phase_out);

        // 反馈连接：CDR相位输出 → 采样器相位偏移
        sig_phase_offset(sig_phase_out);

        monitor->phase_in(sig_phase_out);
        monitor->data_in(sig_sampled);
    }

    void configure_scenario(TestScenario scenario) {
        switch (scenario) {
            case PHASE_LOCK_BASIC:
                // 基本相位锁定测试 - PRBS-15
                src = new DataSource("src",
                                     DataSource::PRBS,
                                     1.0,      // 幅度
                                     m_data_rate,
                                     100e9,    // 采样率
                                     0.0,      // 无随机抖动
                                     0.0,      // 无周期抖动
                                     0.0);     // 无频偏
                break;

            case FREQUENCY_OFFSET:
                // 频率偏移捕获测试 - PRBS-7 + 频偏
                src = new DataSource("src",
                                     DataSource::PRBS,
                                     1.0,
                                     m_data_rate,
                                     100e9,
                                     0.0,
                                     0.0,
                                     100.0);   // 100ppm频偏
                break;

            case JITTER_TOLERANCE:
                // 抖动容限测试 - PRBS-31 + 抖动
                src = new DataSource("src",
                                     DataSource::PRBS,
                                     1.0,
                                     m_data_rate,
                                     100e9,
                                     2e-12,    // 2ps随机抖动
                                     1e6,      // 1MHz周期抖动
                                     0.0);
                break;

            case PHASE_TRACKING:
                // 动态相位跟踪测试 - 交替模式 + 相位调制
                src = new DataSource("src",
                                     DataSource::ALTERNATING,
                                     1.0,
                                     m_data_rate,
                                     100e9,
                                     0.0,
                                     10e6,     // 10MHz相位调制
                                     0.0);
                break;

            case LOOP_BANDWIDTH:
                // 环路带宽测量 - 正弦波 + 小信号调制
                src = new DataSource("src",
                                     DataSource::SINE,
                                     1.0,
                                     m_data_rate,
                                     100e9,
                                     0.0,
                                     5e6,      // 5MHz调制（用于测量带宽）
                                     0.0);
                break;
        }
    }

    std::string get_output_filename(TestScenario scenario) {
        switch (scenario) {
            case PHASE_LOCK_BASIC:   return "cdr_tran_lock.csv";
            case FREQUENCY_OFFSET:   return "cdr_tran_freq.csv";
            case JITTER_TOLERANCE:   return "cdr_tran_jtol.csv";
            case PHASE_TRACKING:     return "cdr_tran_track.csv";
            case LOOP_BANDWIDTH:     return "cdr_tran_bw.csv";
            default:                 return "cdr_tran_output.csv";
        }
    }

    ~CdrTransientTestbench() {
        delete src;
        delete sampler;
        delete cdr;
        delete monitor;
    }

    void print_results() {
        PhaseStats stats = monitor->get_phase_stats(m_ui);

        std::cout << "\n=== CDR瞬态仿真结果 (" << get_scenario_name() << ") ===" << std::endl;
        std::cout << "相位调整统计:" << std::endl;
        std::cout << "  均值: " << stats.mean * 1e12 << " ps" << std::endl;
        std::cout << "  RMS: " << stats.rms * 1e12 << " ps" << std::endl;
        std::cout << "  峰峰值: " << stats.peak_to_peak * 1e12 << " ps" << std::endl;
        std::cout << "  最小值: " << stats.min_value * 1e12 << " ps" << std::endl;
        std::cout << "  最大值: " << stats.max_value * 1e12 << " ps" << std::endl;
        std::cout << "  锁定时间: " << stats.lock_time * 1e9 << " ns" << std::endl;
        std::cout << "  稳态RMS: " << stats.steady_state_rms * 1e12 << " ps" << std::endl;
        std::cout << "  锁定状态: " << (monitor->is_locked() ? "已锁定" : "未锁定") << std::endl;

        std::cout << "\nCDR参数:" << std::endl;
        std::cout << "  Kp: " << m_params.pi.kp << std::endl;
        std::cout << "  Ki: " << m_params.pi.ki << std::endl;
        std::cout << "  PAI分辨率: " << m_params.pai.resolution * 1e12 << " ps" << std::endl;
        std::cout << "  PAI范围: " << m_params.pai.range * 1e12 << " ps" << std::endl;

        // 计算理论环路参数
        double bw_theory = LoopBandwidthAnalyzer::calculate_theoretical_bandwidth(
            m_params.pi.kp, m_params.pi.ki, m_data_rate);
        double zeta_theory = LoopBandwidthAnalyzer::calculate_damping_factor(
            m_params.pi.kp, m_params.pi.ki, m_data_rate);
        double pm_theory = LoopBandwidthAnalyzer::calculate_phase_margin(
            m_params.pi.kp, m_params.pi.ki, m_data_rate);

        std::cout << "\n理论环路参数:" << std::endl;
        std::cout << "  环路带宽: " << bw_theory / 1e6 << " MHz" << std::endl;
        std::cout << "  阻尼系数: " << zeta_theory << std::endl;
        std::cout << "  相位裕度: " << pm_theory << " 度" << std::endl;

        std::cout << "\n输出波形已保存到: " << get_output_filename(m_scenario) << std::endl;

        // 场景特定分析
        analyze_scenario_results(stats);
    }

    const char* get_scenario_name() {
        switch (m_scenario) {
            case PHASE_LOCK_BASIC:   return "基本相位锁定测试";
            case FREQUENCY_OFFSET:   return "频率偏移捕获测试";
            case JITTER_TOLERANCE:   return "抖动容限测试";
            case PHASE_TRACKING:     return "动态相位跟踪测试";
            case LOOP_BANDWIDTH:     return "环路带宽测量";
            default:                 return "未知场景";
        }
    }

    void analyze_scenario_results(const PhaseStats& stats) {
        switch (m_scenario) {
            case PHASE_LOCK_BASIC:
                std::cout << "\n[分析] 相位锁定测试:" << std::endl;
                if (monitor->is_locked()) {
                    std::cout << "  ✓ CDR成功锁定" << std::endl;
                    std::cout << "  锁定时间: " << stats.lock_time / m_ui << " UI" << std::endl;
                    if (stats.lock_time / m_ui < 5000) {
                        std::cout << "  ✓ 锁定时间符合预期（< 5000 UI）" << std::endl;
                    } else {
                        std::cout << "  ⚠ 锁定时间较长（> 5000 UI）" << std::endl;
                    }
                } else {
                    std::cout << "  ✗ CDR未锁定" << std::endl;
                }
                if (stats.steady_state_rms * 1e12 < 5.0) {
                    std::cout << "  ✓ 稳态抖动符合预期（< 5ps RMS）" << std::endl;
                } else {
                    std::cout << "  ⚠ 稳态抖动较大（> 5ps RMS）" << std::endl;
                }
                break;

            case FREQUENCY_OFFSET:
                std::cout << "\n[分析] 频率偏移捕获测试:" << std::endl;
                if (monitor->is_locked()) {
                    std::cout << "  ✓ CDR成功跟踪频偏" << std::endl;
                    // 检查相位是否在范围内
                    if (std::abs(stats.mean) < m_params.pai.range) {
                        std::cout << "  ✓ 相位调整在范围内" << std::endl;
                    } else {
                        std::cout << "  ✗ 相位调整超出范围" << std::endl;
                    }
                } else {
                    std::cout << "  ⚠ CDR可能仍在跟踪频偏" << std::endl;
                }
                break;

            case JITTER_TOLERANCE:
                std::cout << "\n[分析] 抖动容限测试:" << std::endl;
                if (monitor->is_locked()) {
                    std::cout << "  ✓ CDR能够容忍注入的抖动" << std::endl;
                }
                std::cout << "  相位抖动RMS: " << stats.rms * 1e12 << " ps" << std::endl;
                std::cout << "  注入抖动: 2ps RJ + 1MHz SJ" << std::endl;
                break;

            case PHASE_TRACKING:
                std::cout << "\n[分析] 动态相位跟踪测试:" << std::endl;
                double bw_theory = LoopBandwidthAnalyzer::calculate_theoretical_bandwidth(
                    m_params.pi.kp, m_params.pi.ki, m_data_rate);
                std::cout << "  调制频率: 10MHz" << std::endl;
                std::cout << "  理论环路带宽: " << bw_theory / 1e6 << " MHz" << std::endl;
                if (10e6 > bw_theory) {
                    std::cout << "  ⚠ 调制频率高于环路带宽，CDR可能无法完全跟踪" << std::endl;
                } else {
                    std::cout << "  ✓ 调制频率在环路带宽内" << std::endl;
                }
                break;

            case LOOP_BANDWIDTH:
                std::cout << "\n[分析] 环路带宽测量:" << std::endl;
                double bw_theory = LoopBandwidthAnalyzer::calculate_theoretical_bandwidth(
                    m_params.pi.kp, m_params.pi.ki, m_data_rate);
                std::cout << "  理论带宽: " << bw_theory / 1e6 << " MHz" << std::endl;
                std::cout << "  调制频率: 5MHz" << std::endl;
                std::cout << "  注: 实际带宽需要通过频率扫描测量" << std::endl;
                std::cout << "  建议使用后处理脚本分析cdr_tran_bw.csv" << std::endl;
                break;

            default:
                break;
        }
    }
};

// SystemC主函数
int sc_main(int argc, char* argv[]) {
    // 解析命令行参数
    TestScenario scenario = PHASE_LOCK_BASIC;

    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "lock" || arg == "0") scenario = PHASE_LOCK_BASIC;
        else if (arg == "freq" || arg == "1") scenario = FREQUENCY_OFFSET;
        else if (arg == "jtol" || arg == "2") scenario = JITTER_TOLERANCE;
        else if (arg == "track" || arg == "3") scenario = PHASE_TRACKING;
        else if (arg == "bw" || arg == "4") scenario = LOOP_BANDWIDTH;
        else {
            std::cout << "用法: " << argv[0] << " [scenario]" << std::endl;
            std::cout << "场景选项:" << std::endl;
            std::cout << "  lock, 0  - 基本相位锁定测试 (默认)" << std::endl;
            std::cout << "  freq, 1  - 频率偏移捕获测试" << std::endl;
            std::cout << "  jtol, 2  - 抖动容限测试" << std::endl;
            std::cout << "  track, 3 - 动态相位跟踪测试" << std::endl;
            std::cout << "  bw, 4    - 环路带宽测量" << std::endl;
            return 1;
        }
    }

    // 创建测试平台
    CdrTransientTestbench tb("tb", scenario);

    // 确定仿真时间
    double sim_time = 1e-6;  // 默认1μs (10,000 UI @ 10Gbps)
    switch (scenario) {
        case PHASE_LOCK_BASIC:
            sim_time = 1e-6;  // 10,000 UI
            break;
        case FREQUENCY_OFFSET:
            sim_time = 5e-6;  // 50,000 UI
            break;
        case JITTER_TOLERANCE:
            sim_time = 1e-5;  // 100,000 UI
            break;
        case PHASE_TRACKING:
            sim_time = 5e-6;  // 50,000 UI
            break;
        case LOOP_BANDWIDTH:
            sim_time = 1e-6;  // 10,000 UI
            break;
    }

    // 运行瞬态仿真
    std::cout << "开始CDR瞬态仿真 (" << tb.get_scenario_name() << ")..." << std::endl;
    std::cout << "仿真时间: " << sim_time * 1e6 << " μs (" << sim_time / tb.m_ui << " UI)" << std::endl;
    sc_core::sc_start(sim_time, sc_core::SC_SEC);

    // 打印结果
    tb.print_results();

    std::cout << "\n仿真完成！" << std::endl;
    return 0;
}