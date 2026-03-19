# C++ Channel 实现方案总结

## 文件结构

```
serdes/
├── src/
│   ├── cpp_channel/                      # NEW: C++ Channel 核心
│   │   ├── pole_residue_filter.h         # 滤波器头文件
│   │   ├── pole_residue_filter.cpp       # 滤波器实现
│   │   └── python_binding.cpp            # Python 绑定 (可选)
│   └── ams/
│       ├── channel_sparam_v2.h           # NEW: AMS V2 包装器
│       └── channel_sparam_v2.cpp         # NEW: AMS V2 实现
├── include/ams/channel_sparam_v2.h       # 公开头文件
├── tb/channel/
│   └── channel_sparam_v2_tb.cpp          # NEW: V2 测试平台
├── tests/test_cpp_channel.cpp            # NEW: C++ 独立测试
└── CMakeLists.txt                        # 已更新
```

---

## 核心组件

### 1. PoleResidueFilter (纯 C++)

**文件**: `src/cpp_channel/pole_residue_filter.h/cpp`

**功能**:
- 从极点-留数表示构建滤波器
- 双线性变换（Tustin）离散化 → 数值稳定
- 级联二阶状态空间节 → 避免高阶系统不稳定
- 支持单样本和块处理
- 内置频率响应计算

**关键算法**:
```cpp
// 连续→离散变换（双线性）
Ad = (I - A*dt/2)^-1 * (I + A*dt/2)
Bd = (I - A*dt/2)^-1 * B * dt
Cd = C * (I - A*dt/2)^-1
Dd = D + C * (I - A*dt/2)^-1 * B * dt/2

// 处理循环
for each section:
    y = Cd * x + Dd * u
    x = Ad * x + Bd * u
```

---

### 2. ChannelSParamV2 (AMS 包装器)

**文件**: `include/ams/channel_sparam_v2.h`, `src/ams/channel_sparam_v2.cpp`

**功能**:
- 继承 `sca_tdf::sca_module` → AMS 调度器可以管理
- 内部包含 `PoleResidueFilter` → 调用 C++ 核心
- 完全兼容现有 TX/RX 接口

**连接方式**:
```cpp
// TX (AMS) → Channel (AMS+C++) → RX (AMS)
ChannelSParamV2 channel("channel", params);
channel.in(tx_out);      // sca_tdf::sca_signal<double>
channel.out(rx_in);      // sca_tdf::sca_signal<double>
```

---

### 3. 测试验证

#### 独立 C++ 测试
```bash
./build/test_cpp_channel
```
测试内容:
- 一阶低通阶跃响应
- 频率响应精度
- 复极点对谐振器
- 能量守恒

#### AMS 集成测试
```bash
./build/bin/channel_sparam_v2_tb channel_pole_residue.json 500
```
输出:
- 波形 CSV 文件
- RMS 增益分析
- DC 增益验证

---

## 构建步骤

```bash
cd /mnt/d/systemCProjects/SerDesSystemCProject/.worktrees/channel-p1-batch3-scikit-rf/build

# 配置
cmake ..

# 构建
make -j4

# 运行 C++ 测试
./test_cpp_channel

# 生成 pole-residue JSON
cd ..
python3 scripts/export_skrf_vf.py \
    /mnt/d/systemCProjects/SerDesSystemCProject/peters_01_0605_B12_thru.s4p \
    build/channel_pole_residue.json \
    --n-poles 48 --verify

# 运行 AMS 测试
cd build
./bin/channel_sparam_v2_tb channel_pole_residue.json 500
```

---

## 与原有系统对比

| 特性 | V1 (纯 AMS) | V2 (C++ 核心) |
|------|------------|--------------|
| **实现方式** | `sca_ltf_nd` + 状态空间 | C++ `PoleResidueFilter` |
| **离散化** | 欧拉（不稳定） | 双线性（稳定） |
| **高阶系统** | 数值发散 | 级联二阶节，稳定 |
| **调试** | 困难 | Python/C++ 独立测试 |
| **TX/RX 接口** | 标准 AMS | **相同** |
| **仿真速度** | 一般 | 更快（SIMD 潜力） |

---

## 验证方法

### 1. Python 参考验证
```python
import numpy as np
from scipy.signal import lti, lsim

# 从 scikit-rf 获取 poles, residues
# 用 scipy.signal.lti 仿真
# 对比 C++ 输出
```

### 2. 频率响应对比
```python
# C++ 内置 get_frequency_response()
# 与原始 S21 参数对比
# 相关性应 > 0.95
```

### 3. 系统集成测试
```cpp
// 完整 TX → Channel → RX 链路
// 眼图、抖动分析
```

---

## 下一步工作

1. **构建测试** (今天)
   ```bash
   cd build && cmake .. && make -j4
   ```

2. **验证 C++ 核心** (今天)
   ```bash
   ./test_cpp_channel
   ```

3. **验证 AMS 集成** (今天)
   ```bash
   ./bin/channel_sparam_v2_tb channel_pole_residue.json
   ```

4. **频率响应对比** (明天)
   - Python 脚本对比 C++ 和 scipy
   - 确保相关性 > 0.95

5. **完整链路测试** (明天)
   - TX → Channel → RX
   - 眼图分析

---

## 方案优势总结

✅ **数值稳定**: 双线性变换 + 级联二阶节  
✅ **调试友好**: C++ 核心可独立测试  
✅ **完全兼容**: 现有 TX/RX 代码无需修改  
✅ **性能优化**: C++ 实现，SIMD 潜力  
✅ **验证方便**: Python 绑定支持快速验证  

---

需要我开始构建和测试这个方案吗？
