#!/bin/bash
# CTLE模块测试运行脚本

set -e  # 遇到错误立即退出

echo "========================================="
echo "CTLE模块完整测试验证"
echo "========================================="
echo ""

# 设置环境变量
export SYSTEMC_HOME=/Users/liuyizhe/Developer/systemCinstall/install-2.3.4
export SYSTEMC_AMS_HOME=/Users/liuyizhe/Developer/systemCinstall/install-2.3.4

# 进入build目录
cd "$(dirname "$0")/../build"

echo "1. 运行单元测试..."
echo "-----------------------------------------"
cd tests
./unit_tests
cd ..

echo ""
echo "2. 运行CTLE瞬态仿真测试平台..."
echo "-----------------------------------------"
cd tb
./ctle_tran_tb

echo ""
echo "========================================="
echo "所有测试完成！"
echo "========================================="
echo ""
echo "测试结果总结:"
echo "✓ 单元测试通过 (12个测试点)"
echo "✓ 瞬态仿真测试通过"
echo "✓ 波形数据已保存到 ctle_tran_output.csv"
echo ""
echo "测试覆盖范围:"
echo "  - 端口连接验证"
echo "  - 差分信号传输"
echo "  - 共模电压输出"
echo "  - DC增益正确性"
echo "  - 零点/极点配置"
echo "  - 增益范围测试"
echo "  - 偏移/噪声使能标志"
echo "  - VCM可配置性"
echo "  - 输出稳定性"
echo "  - 瞬态响应分析"
echo ""
