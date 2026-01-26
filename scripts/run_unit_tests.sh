#!/bin/bash
# ============================================================================
# 单元测试运行脚本 - 串行执行每个测试套件
# ============================================================================

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_EXEC="$BUILD_DIR/tests/unit_tests"

# 设置 SystemC 环境变量
export SYSTEMC_HOME="/Users/liuyizhe/Developer/systemCinstall/install-2.3.4"
export SYSTEMC_AMS_HOME="/Users/liuyizhe/Developer/systemCinstall/install-2.3.4"

echo "============================================"
echo "SerDes 单元测试 - 串行执行"
echo "============================================"
echo ""

# 检查测试可执行文件是否存在
if [ ! -f "$TEST_EXEC" ]; then
    echo "❌ 错误: 测试可执行文件不存在: $TEST_EXEC"
    echo "请先运行: cd build && make unit_tests"
    exit 1
fi

# 定义所有测试套件
TEST_SUITES=(
    "AdaptionBasicTest"
    "CdrBasicTest"
    "CdrTest"
    "ClockGenerationBasicTest"
    "CtleBasicTest"
    "CtleTest"
    "FfeBasicTest"
    "FfeTest"
    "SamplerBasicTest"
    "TxDriverTest"
    "VgaBasicTest"
    "VgaTest"
    "WaveGenBasicTest"
)

# 统计变量
TOTAL_SUITES=${#TEST_SUITES[@]}
PASSED_SUITES=0
FAILED_SUITES=0

# 临时结果文件
RESULTS_FILE="/tmp/serdes_test_results.txt"
> "$RESULTS_FILE"

echo "共有 $TOTAL_SUITES 个测试套件"
echo ""

# 逐个运行测试套件
for suite in "${TEST_SUITES[@]}"; do
    echo "----------------------------------------"
    echo "运行测试套件: $suite"
    echo "----------------------------------------"
    
    # 运行测试并捕获输出和退出码
    "$TEST_EXEC" --gtest_filter="${suite}.*" 2>&1 | tee /tmp/current_test.log
    EXIT_CODE=${PIPESTATUS[0]}
    
    # 检查是否有测试失败
    PASSED_COUNT=$(grep -c "\[  PASSED  \]" /tmp/current_test.log | head -1 || echo "0")
    FAILED_COUNT=$(grep "FAILED" /tmp/current_test.log | grep -c "test" || echo "0")
    
    if [ "$FAILED_COUNT" -eq 0 ]; then
        echo "✅ $suite - 通过 ($PASSED_COUNT 个测试)" | tee -a "$RESULTS_FILE"
        ((PASSED_SUITES++))
    else
        if [ "$PASSED_COUNT" -gt 0 ]; then
            echo "⚠️  $suite - 部分通过 ($PASSED_COUNT 通过, $FAILED_COUNT 失败)" | tee -a "$RESULTS_FILE"
        else
            echo "❌ $suite - 失败 ($FAILED_COUNT 个测试失败)" | tee -a "$RESULTS_FILE"
        fi
        ((FAILED_SUITES++))
    fi
    
    echo ""
    
    # 短暂暂停，避免系统资源问题
    sleep 0.5
done

# 输出总结
echo "============================================"
echo "测试总结"
echo "============================================"
cat "$RESULTS_FILE"
echo ""
echo "总计: $TOTAL_SUITES 个测试套件"
echo "通过: $PASSED_SUITES"
echo "失败: $FAILED_SUITES"
echo ""

if [ $FAILED_SUITES -eq 0 ]; then
    echo "🎉 所有测试套件通过！"
    exit 0
else
    echo "⚠️  有 $FAILED_SUITES 个测试套件失败"
    echo ""
    echo "提示: 可以单独运行失败的测试进行调试:"
    echo "  cd $BUILD_DIR"
    echo "  ./tests/unit_tests --gtest_filter=\"TestSuiteName.*\""
    exit 1
fi
