# 重启后恢复上下文 - 复制以下内容给 kimi-cli

## 快速恢复指令（复制下面这段）

```
我正在 SerDes SystemC-AMS 项目的 feature/channel-p1-batch3-scikit-rf 分支上工作，工作目录在 .worktrees/channel-p1-batch3-scikit-rf。

**当前任务状态：**
- Channel P1 Batch 3 - scikit-rf VectorFitting 集成
- 已完成 C++ PoleResidueFilter 核心实现（双线性变换 + 级联二阶状态空间）
- C++ 核心已通过全部 4 个单元测试（阶跃响应、频率响应、复极点、能量守恒）
- 已创建 SystemC-AMS V2 包装器 (ChannelSParamV2)
- 当前在方案 C：需要验证 C++ 实现是否满足需求

**关键文件：**
- src/cpp_channel/pole_residue_filter.h/cpp - C++ 核心
- src/ams/channel_sparam_v2.h/cpp - AMS 包装器
- tests/test_cpp_channel.cpp - 已通过测试
- build/test_cpp_channel - 可执行测试文件

**待完成：**
1. 运行 C++ 测试确认状态（./build/test_cpp_channel）
2. 验证频率响应相关性 > 0.95（对比 scikit-rf）
3. 如果满足需求，构建完整 TX-Channel-RX 链路测试

请检查当前状态并继续方案 C 的验证工作。
```

---

## 极简版（如果上面太长）

```
继续 Channel P1 Batch 3 工作。在 .worktrees/channel-p1-batch3-scikit-rf 目录，C++ PoleResidueFilter 核心已完成并通过测试，现在执行方案 C：验证 C++ 实现是否满足需求（频率响应相关性 > 0.95）。检查 build/test_cpp_channel 并继续。
```

---

## 文件位置备忘

```bash
cd /mnt/d/systemCProjects/SerDesSystemCProject/.worktrees/channel-p1-batch3-scikit-rf

# 快速检查
./build/test_cpp_channel                    # 运行 C++ 测试
ls src/cpp_channel/                         # 查看核心文件
ls src/ams/channel_sparam_v2*               # 查看 AMS 包装器
```

---

## 如果需要重新开始测试

```bash
cd /mnt/d/systemCProjects/SerDesSystemCProject/.worktrees/channel-p1-batch3-scikit-rf

# 重新编译 C++ 测试
g++ -std=c++14 -I./src/cpp_channel \
    src/cpp_channel/pole_residue_filter.cpp \
    tests/test_cpp_channel.cpp \
    -o build/test_cpp_channel -lm

# 运行测试
./build/test_cpp_channel
```
