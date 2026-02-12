#!/bin/bash
# SystemC 环境设置脚本
# 复制此文件为 scripts/setup_env_local.sh 并修改为你的本地路径

# ============================================================================
# 请根据你的实际安装路径修改以下变量
# ============================================================================

# macOS 示例 (Homebrew 安装)
# export SYSTEMC_HOME=/opt/homebrew/opt/systemc
# export SYSTEMC_AMS_HOME=/opt/homebrew/opt/systemc-ams

# 本地编译安装示例
export SYSTEMC_HOME=$HOME/systemc-2.3.4
export SYSTEMC_AMS_HOME=$HOME/systemc-ams-2.3.4

# 或者使用相同目录（如果 SystemC-AMS 安装在 SystemC 目录中）
# export SYSTEMC_AMS_HOME=$SYSTEMC_HOME

# ============================================================================
# 验证路径
# ============================================================================
if [ ! -d "$SYSTEMC_HOME" ]; then
    echo "错误: SYSTEMC_HOME 目录不存在: $SYSTEMC_HOME"
    echo "请修改本文件中的路径为你的实际安装路径"
    return 1
fi

if [ ! -d "$SYSTEMC_AMS_HOME" ]; then
    echo "错误: SYSTEMC_AMS_HOME 目录不存在: $SYSTEMC_AMS_HOME"
    echo "请修改本文件中的路径为你的实际安装路径"
    return 1
fi

# 添加到 PATH（可选）
# export PATH=$SYSTEMC_HOME/bin:$PATH

echo "SystemC 环境已设置:"
echo "  SYSTEMC_HOME:     $SYSTEMC_HOME"
echo "  SYSTEMC_AMS_HOME: $SYSTEMC_AMS_HOME"
