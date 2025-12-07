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
export SYSTEMC_HOME=/path/to/systemc-2.3.4
export SYSTEMC_AMS_HOME=/path/to/systemc-ams-2.3.4
```

### 构建和运行所有测试

```bash
# 方法1：使用测试脚本（推荐）
./scripts/run_ctle_tests.sh

# 方法2：手动构建运行
cd build
cmake ..
make unit_tests
cd tests
./unit_tests
```

## 测试结构

```
tests/
├── unit/                    # 单元测试
│   └── test_ctle_basic.cpp # CTLE综合单元测试（12个测试点）
├── integration/             # 集成测试（预留）
├── test_main.cpp           # 测试主函数（sc_main）
└── CMakeLists.txt          # 测试构建配置
```

## CTLE模块测试

### 测试覆盖

- ✅ 端口连接验证
- ✅ 差分信号传输
- ✅ 共模电压输出
- ✅ DC增益正确性
- ✅ 零点/极点配置
- ✅ 增益范围测试
- ✅ 偏移/噪声使能
- ✅ VCM可配置性
- ✅ 输出稳定性
- ✅ 瞬态响应分析

### 运行特定测试

```bash
# 运行单元测试
cd build/tests
./unit_tests

# 运行瞬态仿真
cd build/tb
./ctle_tran_tb
```

## 测试结果

所有测试结果会输出到控制台。瞬态仿真会额外生成：
- `ctle_tran_output.csv` - 波形数据（时间、差分信号、共模信号）

## 添加新测试

由于SystemC的限制（仿真开始后无法重新初始化），建议：

1. **单元测试**：将新测试添加到现有TEST中
   ```cpp
   TEST(CtleBasicTest, AllBasicFunctionality) {
       // 创建testbench
       // 运行仿真
       // 添加多个EXPECT验证
   }
   ```

2. **系统仿真**：创建独立的testbench程序
   ```cpp
   int sc_main(int argc, char* argv[]) {
       // 创建testbench
       // 运行仿真
       return 0;
   }
   ```

## 故障排除

### GoogleTest下载慢
- 已配置国内镜像源，自动从GitHub下载
- 如仍有问题，检查网络连接

### 链接错误
- 确保SYSTEMC_HOME和SYSTEMC_AMS_HOME正确设置
- 确保使用C++11标准（与SystemC匹配）

### 测试失败
- 检查SystemC版本（需要2.3.4）
- 查看错误信息，可能是参数容差需要调整

## 参考文档

- [CTLE测试总结](../docs/modules/ctle_testing_summary.md)
- [CTLE模块文档](../docs/modules/ctle.md)
- [SystemC文档](http://systemc.org)

## 联系

如有问题，请参考文档或提交issue。
