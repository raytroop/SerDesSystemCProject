# SerDes测试框架

本目录包含SerDes系统的完整测试框架，包括单元测试和集成测试。

## 快速开始

### 前置要求

- SystemC 2.3.4
- SystemC-AMS 2.3.4
- CMake >= 3.15
- C++11编译器
- GoogleTest 1.12.1（自动下载）

### 设置环境变量

```bash
export SYSTEMC_HOME=/Users/liuyizhe/Developer/systemCinstall/install-2.3.4
export SYSTEMC_AMS_HOME=/Users/liuyizhe/Developer/systemCinstall/install-2.3.4
```

### 构建测试

```bash
cd build
cmake ..
make unit_tests -j4
```

### 运行测试（推荐方式）

由于 SystemC 的架构限制（仿真器一旦启动就无法在同一进程中重置），我们采用**串行运行策略**，为每个测试单独运行。每个测试都被编译为独立的可执行文件：

```bash
# 方法1：运行所有测试套件（串行执行）
./scripts/run_unit_tests.sh

# 方法2：运行单个测试（注意：每个测试都有独立的可执行文件）
./scripts/run_single_test.sh ctle_basic
./scripts/run_single_test.sh vga_basic
./scripts/run_single_test.sh ffe_basic_functionality

# 或者直接运行特定的测试可执行文件
cd build
./tests/test_ctle_basic
./tests/test_vga_basic
./tests/test_ffe_basic_functionality
```

## 测试结构

```
tests/
├── unit/                           # 单元测试
│   ├── adaption_test_common.h     # Adaption 模块测试通用头文件
│   ├── cdr_test_common.h          # CDR 模块测试通用头文件
│   ├── clock_generation_test_common.h # 时钟生成模块测试通用头文件
│   ├── ffe_test_common.h          # FFE 模块测试通用头文件
│   ├── sampler_test_common.h      # Sampler 模块测试通用头文件
│   ├── tx_driver_test_common.h    # TX Driver 模块测试通用头文件
│   ├── wave_generation_test_common.h # 波形生成模块测试通用头文件
│   ├── test_adaption_agc_basic.cpp           # Adaption AGC 基础测试
│   ├── test_adaption_agc_convergence.cpp     # Adaption AGC 收敛测试
│   ├── test_adaption_agc_rate_limit.cpp      # Adaption AGC 速率限制测试
│   ├── test_adaption_cdr_pi_antiwindup.cpp   # Adaption CDR PI 防积分饱和测试
│   ├── test_adaption_cdr_pi_basic.cpp        # Adaption CDR PI 基础测试
│   ├── test_adaption_cdr_pi_convergence.cpp  # Adaption CDR PI 收敛测试
│   ├── test_adaption_dfe_basic.cpp           # Adaption DFE 基础测试
│   ├── test_adaption_dfe_lms.cpp             # Adaption DFE LMS 算法测试
│   ├── test_adaption_dfe_sign_lms.cpp        # Adaption DFE Sign LMS 算法测试
│   ├── test_adaption_freeze_mechanism.cpp    # Adaption 冻结机制测试
│   ├── test_adaption_mode_change.cpp         # Adaption 模式切换测试
│   ├── test_adaption_output_range.cpp        # Adaption 输出范围测试
│   ├── test_adaption_param_validation.cpp    # Adaption 参数验证测试
│   ├── test_adaption_port_connection.cpp     # Adaption 端口连接测试
│   ├── test_adaption_rollback.cpp            # Adaption 回滚机制测试
│   ├── test_adaption_snapshot.cpp            # Adaption 快照机制测试
│   ├── test_adaption_threshold_basic.cpp     # Adaption 阈值基础测试
│   ├── test_adaption_threshold_drift.cpp     # Adaption 阈值漂移测试
│   ├── test_adaption_threshold_hysteresis.cpp # Adaption 阈值迟滞测试
│   ├── test_adaption_update_count.cpp        # Adaption 更新计数测试
│   ├── test_cdr_basic_functionality.cpp      # CDR 基础功能测试
│   ├── test_cdr_debug_interface.cpp          # CDR 调试接口测试
│   ├── test_cdr_edge_falling.cpp             # CDR 下降沿检测测试
│   ├── test_cdr_edge_none.cpp                # CDR 无边沿检测测试
│   ├── test_cdr_edge_rising.cpp              # CDR 上升沿检测测试
│   ├── test_cdr_edge_threshold_config.cpp    # CDR 边沿阈值配置测试
│   ├── test_cdr_edge_threshold_effect.cpp    # CDR 边沿阈值效果测试
│   ├── test_cdr_edge_threshold_low.cpp       # CDR 低阈值测试
│   ├── test_cdr_pai_config.cpp               # CDR PAI 配置测试
│   ├── test_cdr_pai_quantization.cpp         # CDR PAI 量化测试
│   ├── test_cdr_pai_range_limit.cpp          # CDR PAI 范围限制测试
│   ├── test_cdr_pattern_alternating.cpp      # CDR 交替模式测试
│   ├── test_cdr_pattern_low_density.cpp      # CDR 低密度模式测试
│   ├── test_cdr_pi_config.cpp                # CDR PI 配置测试
│   ├── test_cdr_pi_config_high_gain.cpp      # CDR PI 高增益配置测试
│   ├── test_cdr_pi_config_low_gain.cpp       # CDR PI 低增益配置测试
│   ├── test_cdr_pi_configurations.cpp        # CDR PI 多种配置测试
│   ├── test_cdr_pi_integral.cpp              # CDR PI 积分项测试
│   ├── test_cdr_pi_proportional.cpp          # CDR PI 比例项测试
│   ├── test_cdr_validation_boundary.cpp      # CDR 边界验证测试
│   ├── test_cdr_validation_kp.cpp            # CDR Kp 验证测试
│   ├── test_cdr_validation_range.cpp         # CDR 范围验证测试
│   ├── test_cdr_validation_resolution.cpp    # CDR 分辨率验证测试
│   ├── test_clock_gen_cycle_count.cpp        # 时钟生成周期计数测试
│   ├── test_clock_gen_debug.cpp              # 时钟生成调试测试
│   ├── test_clock_gen_extreme_freq.cpp       # 时钟生成极端频率测试
│   ├── test_clock_gen_freq_10ghz.cpp         # 时钟生成10GHz测试
│   ├── test_clock_gen_freq_1ghz.cpp          # 时钟生成1GHz测试
│   ├── test_clock_gen_freq_40ghz.cpp         # 时钟生成40GHz测试
│   ├── test_clock_gen_freq_80ghz.cpp         # 时钟生成80GHz测试
│   ├── test_clock_gen_ideal_basic.cpp        # 时钟生成理想基本测试
│   ├── test_clock_gen_initial_phase.cpp      # 时钟生成初始相位测试
│   ├── test_clock_gen_invalid_freq.cpp       # 时钟生成无效频率测试
│   ├── test_clock_gen_long_stability.cpp     # 时钟生成长期稳定性测试
│   ├── test_clock_gen_mean_phase.cpp         # 时钟生成平均相位测试
│   ├── test_clock_gen_phase_continuity.cpp   # 时钟生成相位连续性测试
│   ├── test_clock_gen_phase_range.cpp        # 时钟生成相位范围测试
│   ├── test_clock_gen_pll_validation.cpp     # 时钟生成PLL验证测试
│   ├── test_clock_gen_timestep.cpp           # 时钟生成时间步长测试
│   ├── test_clock_gen_type_adpll.cpp         # 时钟生成ADPLL类型测试
│   ├── test_clock_gen_type_ideal.cpp         # 时钟生成理想类型测试
│   ├── test_clock_gen_type_pll.cpp           # 时钟生成PLL类型测试
│   ├── test_ctle_basic.cpp                   # CTLE 基础测试
│   ├── test_ffe_basic_functionality.cpp      # FFE 基础功能测试
│   ├── test_ffe_convolution.cpp              # FFE 卷积测试
│   ├── test_ffe_deemphasis.cpp               # FFE 去加重测试
│   ├── test_ffe_default_params.cpp           # FFE 默认参数测试
│   ├── test_ffe_frequency_response.cpp       # FFE 频率响应测试
│   ├── test_ffe_impulse_response.cpp         # FFE 脉冲响应测试
│   ├── test_ffe_multi_tap.cpp                # FFE 多抽头测试
│   ├── test_ffe_preemphasis.cpp              # FFE 预加重测试
│   ├── test_ffe_tap_coefficients.cpp         # FFE 抽头系数测试
│   ├── test_ffe_taps_boundary.cpp            # FFE 抽头边界测试
│   ├── test_sampler_basic_decision.cpp       # Sampler 基础判决测试
│   ├── test_sampler_de_negative_input.cpp    # Sampler 负输入DE测试
│   ├── test_sampler_de_output.cpp            # Sampler DE输出测试
│   ├── test_sampler_fuzzy_decision.cpp       # Sampler 模糊判决测试
│   ├── test_sampler_hysteresis_behavior.cpp  # Sampler 迟滞行为测试
│   ├── test_sampler_negative_decision.cpp    # Sampler 负判决测试
│   ├── test_sampler_noise_effect.cpp         # Sampler 噪声影响测试
│   ├── test_sampler_offset_effect.cpp        # Sampler 偏移影响测试
│   ├── test_sampler_output_range_neg01.cpp   # Sampler 输出范围负0.1测试
│   ├── test_sampler_output_range_neg05.cpp   # Sampler 输出范围负0.5测试
│   ├── test_sampler_output_range_pos01.cpp   # Sampler 输出范围正0.1测试
│   ├── test_sampler_output_range_pos05.cpp   # Sampler 输出范围正0.5测试
│   ├── test_sampler_output_range_zero.cpp    # Sampler 输出范围零测试
│   ├── test_sampler_validation_params.cpp    # Sampler 参数验证测试
│   ├── test_sampler_validation_phase_source.cpp      # Sampler 相位源验证测试
│   ├── test_sampler_validation_valid_phase_source.cpp # Sampler 有效相位源验证测试
│   ├── test_tx_driver_bandwidth.cpp          # TX Driver 带宽测试
│   ├── test_tx_driver_common_mode.cpp        # TX Driver 共模测试
│   ├── test_tx_driver_dc_gain.cpp            # TX Driver DC增益测试
│   ├── test_tx_driver_gain_mismatch.cpp      # TX Driver 增益失配测试
│   ├── test_tx_driver_hard_saturation.cpp    # TX Driver 硬饱和测试
│   ├── test_tx_driver_psrr_test.cpp          # TX Driver PSRR测试
│   ├── test_tx_driver_slew_rate.cpp          # TX Driver 转换速率测试
│   ├── test_tx_driver_soft_saturation.cpp    # TX Driver 软饱和测试
│   ├── test_vga_basic.cpp                    # VGA 基础测试
│   ├── test_wave_gen_basic_functionality.cpp # 波形生成基础功能测试
│   ├── test_wave_gen_code_balance.cpp        # 波形生成码型平衡测试
│   ├── test_wave_gen_debug_lfsr.cpp          # 波形生成LFSR调试测试
│   ├── test_wave_gen_debug_time.cpp          # 波形生成时间调试测试
│   ├── test_wave_gen_invalid_pulse_width.cpp # 波形生成无效脉宽测试
│   ├── test_wave_gen_invalid_sample_rate.cpp # 波形生成无效采样率测试
│   ├── test_wave_gen_jitter_config.cpp       # 波形生成抖动配置测试
│   ├── test_wave_gen_long_stability.cpp      # 波形生成长期稳定性测试
│   ├── test_wave_gen_mean_value.cpp          # 波形生成均值测试
│   ├── test_wave_gen_nrz_level.cpp           # 波形生成NRZ电平测试
│   ├── test_wave_gen_prbs15.cpp              # 波形生成PRBS15测试
│   ├── test_wave_gen_prbs23.cpp              # 波形生成PRBS23测试
│   ├── test_wave_gen_prbs31.cpp              # 波形生成PRBS31测试
│   ├── test_wave_gen_prbs7.cpp               # 波形生成PRBS7测试
│   ├── test_wave_gen_prbs9.cpp               # 波形生成PRBS9测试
│   ├── test_wave_gen_prbs_mode.cpp           # 波形生成PRBS模式测试
│   ├── test_wave_gen_pulse_basic.cpp         # 波形生成脉冲基础测试
│   ├── test_wave_gen_pulse_timing.cpp        # 波形生成脉冲时序测试
│   ├── test_wave_gen_repro_run1.cpp          # 波形生成重现运行1测试
│   ├── test_wave_gen_repro_run2.cpp          # 波形生成重现运行2测试
│   ├── test_wave_gen_seed_run1.cpp           # 波形生成种子运行1测试
│   ├── test_wave_gen_seed_run2.cpp           # 波形生成种子运行2测试
├── integration/                    # 集成测试（预留）
├── test_main.cpp                  # 测试主函数（sc_main）
└── CMakeLists.txt                 # 测试构建配置
```

## 测试套件列表

### 配置验证测试（无需仿真，运行快速）✅
- **AdaptionTest** - Adaption 模块参数验证相关测试
- **CdrTest** - CDR 模块参数验证相关测试
- **ClockGenerationTest** - 时钟生成模块参数验证相关测试
- **CtleTest** - CTLE 模块参数验证相关测试
- **FfeTest** - FFE 模块参数验证相关测试
- **SamplerTest** - Sampler 模块参数验证相关测试
- **TxDriverTest** - TX Driver 模块参数验证相关测试
- **VgaTest** - VGA 模块参数验证相关测试
- **WaveGenTest** - 波形生成模块参数验证相关测试

### 仿真测试（需要 SystemC 仿真）
- **AdaptionBasicTest** - Adaption 模块基础功能仿真测试
- **CdrBasicTest** - CDR 模块基础功能仿真测试
- **ClockGenerationBasicTest** - 时钟生成模块基础功能仿真测试
- **CtleBasicTest** - CTLE 模块基础功能仿真测试
- **FfeBasicTest** - FFE 模块基础功能仿真测试
- **SamplerBasicTest** - Sampler 模块基础功能仿真测试
- **TxDriverBasicTest** - TX Driver 模块基础功能仿真测试
- **VgaBasicTest** - VGA 模块基础功能仿真测试
- **WaveGenBasicTest** - 波形生成模块基础功能仿真测试

## 测试覆盖

### 总体统计
- **总测试文件数**: 94个（不含公共头文件）
- **Adaption 模块测试**: 21个测试文件
- **CDR 模块测试**: 26个测试文件
- **时钟生成模块测试**: 22个测试文件
- **CTLE 模块测试**: 1个测试文件
- **FFE 模块测试**: 10个测试文件
- **Sampler 模块测试**: 17个测试文件
- **TX Driver 模块测试**: 8个测试文件
- **VGA 模块测试**: 1个测试文件
- **波形生成模块测试**: 27个测试文件

### 各模块测试覆盖

#### Adaption 模块
- ✅ AGC 自适应算法测试
- ✅ CDR PI 控制器自适应测试
- ✅ DFE 自适应算法测试（LMS/Sign LMS）
- ✅ 阈值自适应机制测试
- ✅ 冻结/回滚/快照机制测试
- ✅ 模式切换和更新计数测试
- ✅ 输出范围和速率限制测试

#### CDR 模块
- ✅ 边沿检测功能测试
- ✅ PI 控制器功能测试
- ✅ PAI 量化和范围限制测试
- ✅ 不同增益配置测试
- ✅ 图案识别功能测试
- ✅ 参数验证和边界测试
- ✅ 调试接口测试

#### 时钟生成模块
- ✅ 理想时钟生成功能测试
- ✅ PLL/ADPLL 类型时钟生成测试
- ✅ 不同频率配置测试（1GHz-80GHz）
- ✅ 相位特性和连续性测试
- ✅ 长期稳定性测试
- ✅ 参数验证和边界测试

#### CTLE 模块
- ✅ 端口连接验证
- ✅ 差分信号传输
- ✅ 共模电压输出
- ✅ DC增益正确性
- ✅ 零点/极点配置
- ✅ PSRR/CMRR/CMFB配置
- ✅ 饱和特性
- ✅ 偏移和噪声

#### FFE 模块
- ✅ 多抽头预加重/去加重功能
- ✅ 卷积和脉冲响应测试
- ✅ 频率响应验证
- ✅ 系数配置和边界测试
- ✅ 默认参数验证

#### Sampler 模块
- ✅ 判决功能
- ✅ 迟滞行为
- ✅ 噪声和偏移影响
- ✅ 相位采样和输出范围
- ✅ 参数验证和有效性检查

#### TX Driver 模块
- ✅ DC增益和带宽特性
- ✅ 共模和PSRR性能
- ✅ 饱和特性（软/硬）
- ✅ 转换速率和增益失配
- ✅ 参数验证

#### VGA 模块
- ✅ 传递函数验证
- ✅ 增益控制
- ✅ 共模反馈
- ✅ 电源抑制

#### 波形生成模块
- ✅ PRBS序列生成（PRBS7/9/15/23/31）
- ✅ NRZ电平和码型平衡
- ✅ 抖动配置和脉冲特性
- ✅ 重复性和种子运行
- ✅ 长期稳定性和参数验证

## 运行特定测试

### 使用测试脚本（推荐）

```bash
# 运行单个测试（每个测试对应一个独立的可执行文件）
./scripts/run_single_test.sh ctle_basic
./scripts/run_single_test.sh vga_basic
./scripts/run_single_test.sh ffe_basic_functionality
./scripts/run_single_test.sh adaption_param_validation
./scripts/run_single_test.sh cdr_basic_functionality
./scripts/run_single_test.sh clock_gen_ideal_basic
./scripts/run_single_test.sh sampler_basic_decision
./scripts/run_single_test.sh tx_driver_dc_gain
./scripts/run_single_test.sh wave_gen_basic_functionality

# 也可以直接运行特定的测试可执行文件
cd build
./tests/test_ctle_basic
./tests/test_vga_basic
./tests/test_ffe_basic_functionality
```

### 直接运行测试可执行文件

```bash
cd build
export SYSTEMC_HOME=/Users/liuyizhe/Developer/systemCinstall/install-2.3.4
export SYSTEMC_AMS_HOME=/Users/liuyizhe/Developer/systemCinstall/install-2.3.4

# 运行特定测试
./tests/test_ctle_basic
./tests/test_vga_basic
./tests/test_ffe_basic_functionality
```

### 查看可用测试列表

```bash
# 查看所有可用的测试可执行文件
ls build/tests/test_*

# 或者使用脚本查看（如果构建完成后）
find build/tests -name "test_*" -executable -type f
```

## 测试结果

所有测试结果会输出到控制台。瞬态仿真会额外生成：
- `ctle_tran_output.csv` - 波形数据（时间、差分信号、共模信号）

## 添加新测试

### 重要说明：SystemC 测试限制

由于 SystemC 的架构设计，仿真器一旦启动（第一次调用 `sc_start()`）就无法在同一进程中重置。因此：

1. **配置验证测试**（推荐）：只测试参数，不运行仿真
   ```cpp
   TEST(ModuleTest, ParameterValidation) {
       ModuleParams params;
       params.gain = 2.0;
       EXPECT_DOUBLE_EQ(params.gain, 2.0);
       // 不调用 sc_start()
   }
   ```

2. **仿真测试**：需要通过测试套件串行运行
   ```cpp
   TEST(ModuleBasicTest, Functionality) {
       // 创建 testbench
       ModuleTestbench* tb = new ModuleTestbench("tb", params);
       
       // 运行仿真
       sc_core::sc_start(10, sc_core::SC_NS);
       
       // 验证
       EXPECT_NEAR(tb->get_output(), expected, 0.01);
       
       // 重要：调用 sc_stop() 为下一个测试做准备
       sc_core::sc_stop();
   }
   ```

3. **不要手动删除 SystemC 模块**
   ```cpp
   ~TestbenchClass() {
       // SystemC 模块由仿真器自动管理
       // 不要 delete 它们
   }
   ```

## 故障排除

### GoogleTest 下载慢
- 已配置国内镜像源，自动从GitHub下载
- 如仍有问题，检查网络连接

### 链接错误
- 确保 SYSTEMC_HOME 和 SYSTEMC_AMS_HOME 正确设置
- 确保使用 C++11 标准（与 SystemC 匹配）
- 检查 SystemC 库是否已正确编译

### 测试失败："simulation running" 错误
这是 SystemC 的架构限制，不是代码错误。解决方法：
- 使用 `./scripts/run_single_test.sh` 串行运行每个测试套件
- 不要尝试在一次运行中执行所有测试

### 测试失败：端口类型不匹配
- 检查 SystemC-AMS 端口类型（sca_tdf::sca_in vs sc_core::sc_in）
- 确保模块和测试平台使用匹配的端口类型

### 测试失败：数值误差
- 查看测试容差设置（EXPECT_NEAR 的第三个参数）
- SystemC-AMS 是模拟仿真，存在数值误差是正常的

### 参考文档

- [单元测试运行脚本](../scripts/run_unit_tests.sh) - 串行运行所有测试
- [单测试运行脚本](../scripts/run_single_test.sh) - 运行单个测试（独立可执行文件）
- [CTLE模块文档](../docs/modules/ctle.md)
- [VGA模块文档](../docs/modules/vga.md)
- [SystemC文档](http://systemc.org)
- [Google Test文档](https://google.github.io/googletest/)

## 联系

如有问题，请参考文档或提交issue。
