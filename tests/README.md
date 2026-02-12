# SerDes 测试框架

本目录包含 SerDes 系统的完整测试框架，包括 **139+ 个单元测试**，覆盖 TX、RX、Channel、Clock Generation、Wave Generation、Adaption 等所有模块。

---

## 快速开始

### 前置要求

- SystemC 2.3.4
- SystemC-AMS 2.3.4
- CMake >= 3.15
- C++11/C++14 编译器
- GoogleTest 1.12.1+（CMake 自动获取）

### 设置环境变量

```bash
export SYSTEMC_HOME=/path/to/systemc-2.3.4
export SYSTEMC_AMS_HOME=/path/to/systemc-ams-2.3.4
```

### 构建测试

```bash
# 1. 创建构建目录
mkdir build && cd build

# 2. 配置（启用测试）
cmake -DBUILD_TESTING=ON ..

# 3. 编译所有测试
make -j4
```

### 运行测试

由于 SystemC 的架构限制（仿真器一旦启动就无法在同一进程中重置），**必须串行运行**每个测试：

```bash
# 方法1：运行所有测试（串行）
./scripts/run_unit_tests.sh

# 方法2：运行模块特定测试
./scripts/run_adaption_tests.sh
./scripts/run_cdr_tests.sh
./scripts/run_clockgen_tests.sh

# 方法3：运行单个测试
./scripts/run_single_test.sh ctle_basic
cd build && ./tests/test_ctle_basic
```

---

## 测试结构

```
tests/
├── unit/                          # 单元测试 (139+ 个测试文件)
│   ├── adaption_test_common.h     # Adaption 模块测试公共头文件
│   ├── cdr_test_common.h          # CDR 模块测试公共头文件
│   ├── clock_generation_test_common.h
│   ├── ffe_test_common.h
│   ├── sampler_test_common.h
│   ├── tx_driver_test_common.h
│   ├── tx_top_test_common.h
│   ├── rx_top_test_common.h
│   ├── wave_generation_test_common.h
│   ├── test_adaption_*.cpp        # 20个 Adaption 测试
│   ├── test_cdr_*.cpp             # 23个 CDR 测试
│   ├── test_clock_gen_*.cpp       # 19个时钟生成测试
│   ├── test_ctle_basic.cpp        # 1个 CTLE 测试
│   ├── test_dfe_*.cpp             # 3个 DFE 测试
│   ├── test_ffe_*.cpp             # 10个 FFE 测试
│   ├── test_sampler_*.cpp         # 16个 Sampler 测试
│   ├── test_tx_driver_*.cpp       # 8个 TX Driver 测试
│   ├── test_tx_top_*.cpp          # 8个 TX Top 测试
│   ├── test_rx_top_*.cpp          # 5个 RX Top 测试
│   ├── test_vga_basic.cpp         # 1个 VGA 测试
│   ├── test_wave_gen_*.cpp        # 22个波形生成测试
│   └── test_channel_*.cpp         # 3个 Channel 测试
├── test_main.cpp                  # 测试主函数
└── CMakeLists.txt                 # 测试构建配置
```

---

## 测试覆盖统计

| 模块 | 测试数 | 覆盖内容 |
|------|--------|----------|
| **Adaption** | 20 | AGC、DFE LMS、CDR PI、阈值自适应、冻结/回滚/快照机制 |
| **CDR** | 23 | PI控制器、PAI、边沿检测、图案识别、参数验证 |
| **ClockGen** | 19 | 理想/PLL/ADPLL时钟、1-80GHz频率、相位特性 |
| **CTLE** | 1 | 端口连接、差分传输、零极点配置、饱和特性 |
| **DFE** | 3 | 抽头反馈、历史缓冲区更新 |
| **FFE** | 10 | 抽头系数、卷积、频响、预/去加重 |
| **Sampler** | 16 | 判决、迟滞、噪声/偏移、相位采样 |
| **TX Driver** | 8 | DC增益、带宽、饱和、PSRR、转换速率 |
| **TX Top** | 8 | FFE效果、Driver摆幅、差分信号、集成测试 |
| **RX Top** | 5 | CTLE效果、双DFE、CDR闭环、集成测试 |
| **VGA** | 1 | 传递函数、增益控制、共模反馈 |
| **WaveGen** | 22 | PRBS7/9/15/23/31、抖动、脉冲、稳定性 |
| **Channel** | 3 | S参数配置、VF/IR一致性、信号处理 |
| **总计** | **139** | 全面覆盖所有模块 |

---

## 运行特定测试

### 使用测试脚本（推荐）

```bash
# 运行单个测试
./scripts/run_single_test.sh ctle_basic
./scripts/run_single_test.sh vga_basic
./scripts/run_single_test.sh test_cdr_basic_functionality

# 运行模块测试套件
./scripts/run_adaption_tests.sh      # 运行所有 Adaption 测试
./scripts/run_cdr_tests.sh           # 运行所有 CDR 测试
./scripts/run_clockgen_tests.sh      # 运行所有时钟测试
```

### 直接运行测试可执行文件

```bash
cd build
export SYSTEMC_HOME=/path/to/systemc-2.3.4
export SYSTEMC_AMS_HOME=/path/to/systemc-ams-2.3.4

# 运行特定测试（每个测试是独立的可执行文件）
./tests/test_ctle_basic
./tests/test_vga_basic
./tests/test_cdr_pi_config
```

### 查看可用测试列表

```bash
# 列出所有测试可执行文件
ls build/tests/test_*

# 或使用 ctest（仅列出测试名）
cd build && ctest -N
```

---

## 添加新测试

### SystemC 测试限制（重要）

由于 SystemC 架构限制，**仿真器一旦启动就无法重置**。因此：

#### 1. 配置验证测试（推荐，运行快速）

```cpp
TEST(ModuleTest, ParameterValidation) {
    ModuleParams params;
    params.gain = 2.0;
    EXPECT_DOUBLE_EQ(params.gain, 2.0);
    // 不调用 sc_start()
}
```

#### 2. 仿真测试（每个测试必须是独立可执行文件）

```cpp
TEST(ModuleBasicTest, Functionality) {
    // 创建 testbench
    auto* tb = new ModuleTestbench("tb", params);
    
    // 运行仿真
    sc_core::sc_start(10, sc_core::SC_NS);
    
    // 验证结果
    EXPECT_NEAR(tb->get_output(), expected, 0.01);
    
    // 重要：调用 sc_stop()
    sc_core::sc_stop();
    
    // 不要手动 delete tb - SystemC 管理模块生命周期
}
```

### 在 CMakeLists.txt 中添加测试

```cmake
set(NEW_MODULE_TESTS
    new_module_basic        # 对应 test_new_module_basic.cpp
    new_module_advanced     # 对应 test_new_module_advanced.cpp
)

create_test_executables("${NEW_MODULE_TESTS}")
```

---

## 故障排除

### "SystemC not found" 错误

确保环境变量已设置：
```bash
export SYSTEMC_HOME=/path/to/systemc-2.3.4
export SYSTEMC_AMS_HOME=/path/to/systemc-ams-2.3.4
```

### "simulation running" 错误

这是 SystemC 的正常限制。使用脚本串行运行：
```bash
./scripts/run_single_test.sh test_name
```

### 链接错误

- 确保使用 C++11/C++14 标准（与 SystemC 匹配）
- 检查 SystemC 库是否正确编译
- 验证 `SYSTEMC_HOME` 指向包含 `include/` 和 `lib/` 的目录

### 测试失败：数值容差

SystemC-AMS 是模拟仿真，存在数值误差是正常的。使用合适的容差：
```cpp
EXPECT_NEAR(actual, expected, 0.01);  // 1% 容差
```

---

## 参考文档

- [项目主 README](../README.md)
- [模块文档](../docs/modules/)
- [SystemC 文档](https://systemc.org)
- [GoogleTest 文档](https://google.github.io/googletest/)

---

## 更新记录

- **2024-02**: 更新测试统计（139+ 测试），统一 CMake 配置
