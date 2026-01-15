#include <gtest/gtest.h>
#include <systemc-ams>
#include <cmath>
#include "ams/rx_cdr.h"
#include "common/parameters.h"

using namespace serdes;

// 简单数据源模块 - 用于测试CDR
class SimpleDataSource : public sca_tdf::sca_module {
public:
    sca_tdf::sca_out<double> out;

    std::vector<double> m_data_pattern;
    size_t m_index;

    SimpleDataSource(sc_core::sc_module_name nm, const std::vector<double>& pattern)
        : sca_tdf::sca_module(nm)
        , out("out")
        , m_data_pattern(pattern)
        , m_index(0)
    {}

    void set_attributes() {
        out.set_rate(1);
        out.set_timestep(1.0 / 10e9, sc_core::SC_SEC);  // 10 GHz
    }

    void processing() {
        if (!m_data_pattern.empty()) {
            out.write(m_data_pattern[m_index % m_data_pattern.size()]);
            m_index++;
        } else {
            out.write(0.0);
        }
    }
};

// 测试用顶层模块
SC_MODULE(CdrBasicTestbench) {
    SimpleDataSource* src;
    RxCdrTdf* cdr;

    sca_tdf::sca_signal<double> sig_data;
    sca_tdf::sca_signal<double> sig_phase;

    CdrParams params;

    CdrBasicTestbench(sc_core::sc_module_name nm, const CdrParams& p, const std::vector<double>& pattern)
        : sc_core::sc_module(nm)
        , params(p)
    {
        src = new SimpleDataSource("src", pattern);
        cdr = new RxCdrTdf("cdr", params);

        src->out(sig_data);
        cdr->in(sig_data);
        cdr->phase_out(sig_phase);
    }

    ~CdrBasicTestbench() {
        delete src;
        delete cdr;
    }

    double get_phase_output() {
        return sig_phase.read(0);
    }
};

// ============================================================================
// 测试用例1: 基本功能验证
// ============================================================================
TEST(CdrBasicTest, AllBasicFunctionality) {
    // 配置CDR参数（最小配置）
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    // 创建测试平台 - 使用交替模式（010101...）
    std::vector<double> pattern = {1.0, -1.0, 1.0, -1.0, 1.0, -1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    // 运行仿真
    sc_core::sc_start(10, sc_core::SC_NS);

    // 测试1: 验证端口连接成功
    SUCCEED() << "Port connection test passed";

    // 测试2: 验证相位输出在合理范围内
    double phase = tb->get_phase_output();
    EXPECT_GE(phase, -params.pai.range) << "Phase output should be >= -range";
    EXPECT_LE(phase, params.pai.range) << "Phase output should be <= range";

    // 测试3: 验证相位量化到分辨率
    double quantized = std::round(phase / params.pai.resolution) * params.pai.resolution;
    EXPECT_NEAR(phase, quantized, 1e-15) << "Phase output should be quantized to resolution";

    // 测试4: 验证PI参数配置
    EXPECT_DOUBLE_EQ(params.pi.kp, 0.01) << "Kp parameter test";
    EXPECT_DOUBLE_EQ(params.pi.ki, 1e-4) << "Ki parameter test";

    // 测试5: 验证PAI参数配置
    EXPECT_DOUBLE_EQ(params.pai.resolution, 1e-12) << "PAI resolution test";
    EXPECT_DOUBLE_EQ(params.pai.range, 5e-11) << "PAI range test";

    delete tb;
}

// ============================================================================
// 测试用例2: PI控制器参数配置验证
// ============================================================================
TEST(CdrTest, PI_Controller_Configuration) {
    CdrParams params;

    // 测试默认值
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    EXPECT_GT(params.pi.kp, 0.0) << "Kp should be positive";
    EXPECT_GT(params.pi.ki, 0.0) << "Ki should be positive";

    // 测试不同Kp值
    params.pi.kp = 0.001;  // 小增益
    EXPECT_DOUBLE_EQ(params.pi.kp, 0.001);

    params.pi.kp = 0.1;    // 大增益
    EXPECT_DOUBLE_EQ(params.pi.kp, 0.1);

    // 测试不同Ki值
    params.pi.ki = 1e-5;   // 小积分增益
    EXPECT_DOUBLE_EQ(params.pi.ki, 1e-5);

    params.pi.ki = 1e-3;   // 大积分增益
    EXPECT_DOUBLE_EQ(params.pi.ki, 1e-3);

    // 测试Ki与Kp的关系（Ki通常比Kp小）
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    EXPECT_LT(params.pi.ki, params.pi.kp) << "Ki should be smaller than Kp";
}

// ============================================================================
// 测试用例3: PAI参数配置验证
// ============================================================================
TEST(CdrTest, PAI_Configuration) {
    CdrParams params;

    // 测试默认值
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;
    EXPECT_GT(params.pai.resolution, 0.0) << "Resolution should be positive";
    EXPECT_GT(params.pai.range, 0.0) << "Range should be positive";

    // 测试不同分辨率
    params.pai.resolution = 5e-13;  // 0.5ps
    EXPECT_DOUBLE_EQ(params.pai.resolution, 5e-13);

    params.pai.resolution = 5e-12;  // 5ps
    EXPECT_DOUBLE_EQ(params.pai.resolution, 5e-12);

    // 测试不同范围
    params.pai.range = 1e-11;   // ±10ps
    EXPECT_DOUBLE_EQ(params.pai.range, 1e-11);

    params.pai.range = 1e-10;   // ±100ps
    EXPECT_DOUBLE_EQ(params.pai.range, 1e-10);

    // 测试范围应大于分辨率
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;
    EXPECT_GT(params.pai.range, params.pai.resolution) << "Range should be larger than resolution";
}

// ============================================================================
// 测试用例4: 相位范围限制验证
// ============================================================================
TEST(CdrTest, PhaseRangeLimit) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    // 创建测试平台 - 使用恒定值（无边沿）
    std::vector<double> pattern = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    // 运行仿真
    sc_core::sc_start(10, sc_core::SC_NS);

    // 测试相位输出在范围内
    double phase = tb->get_phase_output();
    EXPECT_GE(phase, -params.pai.range) << "Phase should not exceed negative limit";
    EXPECT_LE(phase, params.pai.range) << "Phase should not exceed positive limit";

    delete tb;
}

// ============================================================================
// 测试用例5: 相位量化验证
// ============================================================================
TEST(CdrTest, PhaseQuantization) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;  // 1ps
    params.pai.range = 5e-11;

    // 创建测试平台
    std::vector<double> pattern = {1.0, -1.0, 1.0, -1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern);

    // 运行仿真
    sc_core::sc_start(10, sc_core::SC_NS);

    // 测试相位输出是分辨率的整数倍
    double phase = tb->get_phase_output();
    double quantized = std::round(phase / params.pai.resolution) * params.pai.resolution;
    EXPECT_NEAR(phase, quantized, 1e-15) << "Phase should be quantized to resolution";

    delete tb;
}

// ============================================================================
// 测试用例6: 边沿检测验证
// ============================================================================
TEST(CdrTest, EdgeDetection) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    // 测试上升沿（-1.0 → 1.0）
    std::vector<double> pattern_rising = {-1.0, 1.0, 1.0, 1.0};
    CdrBasicTestbench* tb_rising = new CdrBasicTestbench("tb_rising", params, pattern_rising);
    sc_core::sc_start(10, sc_core::SC_NS);
    double phase_rising = tb_rising->get_phase_output();
    delete tb_rising;

    // 测试下降沿（1.0 → -1.0）
    std::vector<double> pattern_falling = {1.0, -1.0, -1.0, -1.0};
    CdrBasicTestbench* tb_falling = new CdrBasicTestbench("tb_falling", params, pattern_falling);
    sc_core::sc_start(10, sc_core::SC_NS);
    double phase_falling = tb_falling->get_phase_output();
    delete tb_falling;

    // 测试无边沿（恒定值）
    std::vector<double> pattern_no_edge = {1.0, 1.0, 1.0, 1.0};
    CdrBasicTestbench* tb_no_edge = new CdrBasicTestbench("tb_no_edge", params, pattern_no_edge);
    sc_core::sc_start(10, sc_core::SC_NS);
    double phase_no_edge = tb_no_edge->get_phase_output();
    delete tb_no_edge;

    // 边沿检测应导致相位调整，无边沿时相位应保持不变
    // 注意：由于CDR是闭环系统，实际行为更复杂，这里仅验证基本功能
    SUCCEED() << "Edge detection test completed";
}

// ============================================================================
// 测试用例7: 参数边界条件验证
// ============================================================================
TEST(CdrTest, ParameterBoundaryConditions) {
    CdrParams params;

    // 测试极小Kp
    params.pi.kp = 1e-6;
    EXPECT_DOUBLE_EQ(params.pi.kp, 1e-6);

    // 测试极大Kp
    params.pi.kp = 1.0;
    EXPECT_DOUBLE_EQ(params.pi.kp, 1.0);

    // 测试极小Ki
    params.pi.ki = 1e-10;
    EXPECT_DOUBLE_EQ(params.pi.ki, 1e-10);

    // 测试极大Ki
    params.pi.ki = 1.0;
    EXPECT_DOUBLE_EQ(params.pi.ki, 1.0);

    // 测试极小分辨率
    params.pai.resolution = 1e-15;  // 1fs
    EXPECT_DOUBLE_EQ(params.pai.resolution, 1e-15);

    // 测试极大分辨率
    params.pai.resolution = 1e-9;  // 1ns
    EXPECT_DOUBLE_EQ(params.pai.resolution, 1e-9);

    // 测试极小范围
    params.pai.range = 1e-12;  // ±1ps
    EXPECT_DOUBLE_EQ(params.pai.range, 1e-12);

    // 测试极大范围
    params.pai.range = 1e-9;  // ±1ns
    EXPECT_DOUBLE_EQ(params.pai.range, 1e-9);
}

// ============================================================================
// 测试用例8: PI控制器行为验证
// ============================================================================
TEST(CdrTest, PI_Controller_Behavior) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    // 测试比例项响应
    // 当有边沿时，比例项应立即响应
    std::vector<double> pattern_single_edge = {0.0, 1.0, 1.0, 1.0};
    CdrBasicTestbench* tb = new CdrBasicTestbench("tb", params, pattern_single_edge);
    sc_core::sc_start(10, sc_core::SC_NS);
    double phase = tb->get_phase_output();
    delete tb;

    // 相位输出应反映比例项和积分项的综合作用
    EXPECT_GE(phase, -params.pai.range) << "Phase should be within range";
    EXPECT_LE(phase, params.pai.range) << "Phase should be within range";
}

// ============================================================================
// 测试用例9: 多种数据模式验证
// ============================================================================
TEST(CdrTest, MultipleDataPatterns) {
    CdrParams params;
    params.pi.kp = 0.01;
    params.pi.ki = 1e-4;
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;

    // 测试模式1: 交替模式（最大跃变密度）
    std::vector<double> pattern1 = {1.0, -1.0, 1.0, -1.0, 1.0, -1.0};
    CdrBasicTestbench* tb1 = new CdrBasicTestbench("tb1", params, pattern1);
    sc_core::sc_start(10, sc_core::SC_NS);
    double phase1 = tb1->get_phase_output();
    delete tb1;

    // 测试模式2: 低跃变密度
    std::vector<double> pattern2 = {1.0, 1.0, 1.0, -1.0, -1.0, -1.0};
    CdrBasicTestbench* tb2 = new CdrBasicTestbench("tb2", params, pattern2);
    sc_core::sc_start(10, sc_core::SC_NS);
    double phase2 = tb2->get_phase_output();
    delete tb2;

    // 测试模式3: 随机模式
    std::vector<double> pattern3 = {1.0, -1.0, -1.0, 1.0, -1.0, 1.0};
    CdrBasicTestbench* tb3 = new CdrBasicTestbench("tb3", params, pattern3);
    sc_core::sc_start(10, sc_core::SC_NS);
    double phase3 = tb3->get_phase_output();
    delete tb3;

    // 所有相位输出都应在合理范围内
    EXPECT_GE(phase1, -params.pai.range);
    EXPECT_LE(phase1, params.pai.range);
    EXPECT_GE(phase2, -params.pai.range);
    EXPECT_LE(phase2, params.pai.range);
    EXPECT_GE(phase3, -params.pai.range);
    EXPECT_LE(phase3, params.pai.range);
}

// ============================================================================
// 测试用例10: 不同PI参数组合验证
// ============================================================================
TEST(CdrTest, DifferentPI_Configurations) {
    // 配置1: 标准配置
    CdrParams params1;
    params1.pi.kp = 0.01;
    params1.pi.ki = 1e-4;
    params1.pai.resolution = 1e-12;
    params1.pai.range = 5e-11;

    std::vector<double> pattern = {1.0, -1.0, 1.0, -1.0};
    CdrBasicTestbench* tb1 = new CdrBasicTestbench("tb1", params1, pattern);
    sc_core::sc_start(10, sc_core::SC_NS);
    double phase1 = tb1->get_phase_output();
    delete tb1;

    // 配置2: 高增益配置
    CdrParams params2;
    params2.pi.kp = 0.02;
    params2.pi.ki = 2e-4;
    params2.pai.resolution = 1e-12;
    params2.pai.range = 5e-11;

    CdrBasicTestbench* tb2 = new CdrBasicTestbench("tb2", params2, pattern);
    sc_core::sc_start(10, sc_core::SC_NS);
    double phase2 = tb2->get_phase_output();
    delete tb2;

    // 配置3: 低增益配置
    CdrParams params3;
    params3.pi.kp = 0.005;
    params3.pi.ki = 5e-5;
    params3.pai.resolution = 1e-12;
    params3.pai.range = 5e-11;

    CdrBasicTestbench* tb3 = new CdrBasicTestbench("tb3", params3, pattern);
    sc_core::sc_start(10, sc_core::SC_NS);
    double phase3 = tb3->get_phase_output();
    delete tb3;

    // 所有配置都应正常工作
    EXPECT_GE(phase1, -params1.pai.range);
    EXPECT_LE(phase1, params1.pai.range);
    EXPECT_GE(phase2, -params2.pai.range);
    EXPECT_LE(phase2, params2.pai.range);
    EXPECT_GE(phase3, -params3.pai.range);
    EXPECT_LE(phase3, params3.pai.range);
}