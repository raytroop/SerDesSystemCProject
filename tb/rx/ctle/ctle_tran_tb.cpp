// CTLE瞬态仿真测试平台 - 支持多种测试场景
#include <systemc-ams>
#include "ams/rx_ctle.h"
#include "common/parameters.h"
#include "ctle_helpers.h"
#include <iostream>
#include <string>

using namespace serdes;
using namespace serdes::tb;

// 测试场景枚举
enum TestScenario {
    BASIC_PRBS,           // 基本PRBS测试
    FREQUENCY_RESPONSE,   // 频率响应测试
    PSRR_TEST,            // PSRR测试
    CMRR_TEST,            // CMRR测试
    SATURATION_TEST       // 饱和测试
};

// CTLE瞬态仿真顶层模块 - 支持多种测试场景
SC_MODULE(CtleTransientTestbench) {
    // 模块实例
    DiffSignalSource* src;
    VddSource* vdd_src;
    RxCtleTdf* ctle;
    SignalMonitor* monitor;
    
    // 信号连接
    sca_tdf::sca_signal<double> sig_in_p;
    sca_tdf::sca_signal<double> sig_in_n;
    sca_tdf::sca_signal<double> sig_vdd;
    sca_tdf::sca_signal<double> sig_out_p;
    sca_tdf::sca_signal<double> sig_out_n;
    
    TestScenario m_scenario;
    RxCtleParams m_params;
    
    CtleTransientTestbench(sc_core::sc_module_name nm, 
                           TestScenario scenario = BASIC_PRBS)
        : sc_module(nm)
        , m_scenario(scenario)
    {
        // 配置CTLE参数
        m_params.zeros = {2e9};       // 2 GHz zero
        m_params.poles = {30e9};      // 30 GHz pole
        m_params.dc_gain = 1.5;
        m_params.vcm_out = 0.6;
        m_params.offset_enable = false;
        m_params.noise_enable = false;
        
        // 根据测试场景配置
        configure_scenario(scenario);
        
        // 创建CTLE
        ctle = new RxCtleTdf("ctle", m_params);
        
        // 创建监测器 - 保存到文件
        std::string filename = get_output_filename(scenario);
        monitor = new SignalMonitor("monitor", filename);
        
        // 连接模块
        src->out_p(sig_in_p);
        src->out_n(sig_in_n);
        vdd_src->vdd(sig_vdd);
        
        ctle->in_p(sig_in_p);
        ctle->in_n(sig_in_n);
        ctle->vdd(sig_vdd);
        ctle->out_p(sig_out_p);
        ctle->out_n(sig_out_n);
        
        monitor->in_p(sig_out_p);
        monitor->in_n(sig_out_n);
    }
    
    void configure_scenario(TestScenario scenario) {
        switch (scenario) {
            case BASIC_PRBS:
                // 基本PRBS测试 - 标准配置
                src = new DiffSignalSource("src", 
                                           DiffSignalSource::PRBS,
                                           0.1,      // 100mV amplitude
                                           10e9,     // 10 GHz
                                           0.6);     // 0.6V common mode
                vdd_src = new VddSource("vdd_src", 1.0);
                break;
                
            case FREQUENCY_RESPONSE:
                // 频率响应测试 - 使用正弦波
                src = new DiffSignalSource("src", 
                                           DiffSignalSource::SINE,
                                           0.1,      // 100mV amplitude
                                           5e9,      // 5 GHz test frequency
                                           0.6);
                vdd_src = new VddSource("vdd_src", 1.0);
                break;
                
            case PSRR_TEST:
                // PSRR测试 - VDD有噪声
                m_params.psrr.enable = true;
                m_params.psrr.gain = 0.01;  // -40dB PSRR
                m_params.psrr.poles = {1e6};
                m_params.psrr.vdd_nom = 1.0;
                
                src = new DiffSignalSource("src", 
                                           DiffSignalSource::DC,
                                           0.0,      // 无差分信号
                                           0.0,
                                           0.6);
                // VDD有正弦波噪声
                vdd_src = new VddSource("vdd_src", 1.0, 100e9,
                                        VddSource::SINUSOIDAL,
                                        0.1,       // 100mV VDD ripple
                                        1e6);      // 1 MHz ripple frequency
                break;
                
            case CMRR_TEST:
                // CMRR测试 - 共模输入变化
                m_params.cmrr.enable = true;
                m_params.cmrr.gain = 0.001;  // -60dB CMRR
                m_params.cmrr.poles = {10e6};
                
                src = new DiffSignalSource("src", 
                                           DiffSignalSource::DC,
                                           0.1,      // 小差分信号
                                           0.0,
                                           0.6);     // 后续可扩展为共模变化
                vdd_src = new VddSource("vdd_src", 1.0);
                break;
                
            case SATURATION_TEST:
                // 饱和测试 - 大信号输入
                src = new DiffSignalSource("src", 
                                           DiffSignalSource::SQUARE,
                                           0.5,      // 500mV大幅度
                                           1e9,      // 1 GHz
                                           0.6);
                vdd_src = new VddSource("vdd_src", 1.0);
                break;
        }
    }
    
    std::string get_output_filename(TestScenario scenario) {
        switch (scenario) {
            case BASIC_PRBS:        return "ctle_tran_prbs.csv";
            case FREQUENCY_RESPONSE: return "ctle_tran_freq.csv";
            case PSRR_TEST:          return "ctle_tran_psrr.csv";
            case CMRR_TEST:          return "ctle_tran_cmrr.csv";
            case SATURATION_TEST:    return "ctle_tran_sat.csv";
            default:                 return "ctle_tran_output.csv";
        }
    }
    
    ~CtleTransientTestbench() {
        delete src;
        delete vdd_src;
        delete ctle;
        delete monitor;
    }
    
    void print_results() {
        SignalStats diff_stats = monitor->get_diff_stats();
        SignalStats cm_stats = monitor->get_cm_stats();
        
        std::cout << "\n=== CTLE瞬态仿真结果 (" << get_scenario_name() << ") ===" << std::endl;
        std::cout << "差分信号统计:" << std::endl;
        std::cout << "  均值: " << diff_stats.mean << " V" << std::endl;
        std::cout << "  RMS: " << diff_stats.rms << " V" << std::endl;
        std::cout << "  峰峰值: " << diff_stats.peak_to_peak << " V" << std::endl;
        std::cout << "  最小值: " << diff_stats.min_value << " V" << std::endl;
        std::cout << "  最大值: " << diff_stats.max_value << " V" << std::endl;
        
        std::cout << "\n共模信号统计:" << std::endl;
        std::cout << "  均值: " << cm_stats.mean << " V" << std::endl;
        std::cout << "  RMS: " << cm_stats.rms << " V" << std::endl;
        std::cout << "  峰峰值: " << cm_stats.peak_to_peak << " V" << std::endl;
        
        std::cout << "\n输出波形已保存到: " << get_output_filename(m_scenario) << std::endl;
        
        // 场景特定分析
        analyze_scenario_results(diff_stats, cm_stats);
    }
    
    const char* get_scenario_name() {
        switch (m_scenario) {
            case BASIC_PRBS:        return "基本PRBS测试";
            case FREQUENCY_RESPONSE: return "频率响应测试";
            case PSRR_TEST:          return "PSRR测试";
            case CMRR_TEST:          return "CMRR测试";
            case SATURATION_TEST:    return "饱和测试";
            default:                 return "未知场景";
        }
    }
    
    void analyze_scenario_results(const SignalStats& diff_stats, 
                                  const SignalStats& cm_stats) {
        switch (m_scenario) {
            case BASIC_PRBS:
                std::cout << "\n[分析] DC增益 ≈ " 
                          << diff_stats.peak_to_peak / 0.1 << "x" << std::endl;
                break;
                
            case PSRR_TEST:
                if (diff_stats.peak_to_peak > 0.001) {
                    std::cout << "\n[分析] PSRR测试: VDD噪声已耦合到输出" << std::endl;
                    std::cout << "  输出差分纹波: " << diff_stats.peak_to_peak * 1000 << " mV" << std::endl;
                } else {
                    std::cout << "\n[分析] PSRR测试: VDD噪声被有效抑制" << std::endl;
                }
                break;
                
            case SATURATION_TEST:
                std::cout << "\n[分析] 饱和测试:" << std::endl;
                std::cout << "  输入幅度: 500mV" << std::endl;
                std::cout << "  输出峰峰值: " << diff_stats.peak_to_peak * 1000 << " mV" << std::endl;
                if (diff_stats.peak_to_peak < 0.75 * m_params.dc_gain) {
                    std::cout << "  状态: 已进入饱和区" << std::endl;
                }
                break;
                
            default:
                break;
        }
    }
};

// SystemC主函数
int sc_main(int argc, char* argv[]) {
    // 解析命令行参数
    TestScenario scenario = BASIC_PRBS;
    
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "prbs" || arg == "0") scenario = BASIC_PRBS;
        else if (arg == "freq" || arg == "1") scenario = FREQUENCY_RESPONSE;
        else if (arg == "psrr" || arg == "2") scenario = PSRR_TEST;
        else if (arg == "cmrr" || arg == "3") scenario = CMRR_TEST;
        else if (arg == "sat" || arg == "4") scenario = SATURATION_TEST;
        else {
            std::cout << "用法: " << argv[0] << " [scenario]" << std::endl;
            std::cout << "场景选项:" << std::endl;
            std::cout << "  prbs, 0  - 基本PRBS测试 (默认)" << std::endl;
            std::cout << "  freq, 1  - 频率响应测试" << std::endl;
            std::cout << "  psrr, 2  - PSRR测试" << std::endl;
            std::cout << "  cmrr, 3  - CMRR测试" << std::endl;
            std::cout << "  sat, 4   - 饱和测试" << std::endl;
            return 1;
        }
    }
    
    // 创建测试平台
    CtleTransientTestbench tb("tb", scenario);
    
    // 运行瞬态仿真 - 100ns
    std::cout << "开始CTLE瞬态仿真 (" << tb.get_scenario_name() << ")..." << std::endl;
    sc_core::sc_start(100, sc_core::SC_NS);
    
    // 打印结果
    tb.print_results();
    
    return 0;
}
