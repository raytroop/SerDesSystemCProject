# Channel P1 实现方案对比分析
## SystemC-AMS vs 纯C++

---

## 当前 SystemC-AMS 方案的问题

### 1. 核心困难

| 问题 | 描述 | 影响 |
|------|------|------|
| **sca_ltf_nd 限制** | 必须在 `processing()` 中直接使用，不能封装 | 无法做模块化极点-留数实现 |
| **数值稳定性** | 高阶系统（50个极点）状态空间积分易发散 | 需要极小的步长或复杂离散化 |
| **时间离散化** | TDF模块固定步长，无法自适应 | 高频极点需要子步进，性能下降 |
| **调试困难** | 系统内部状态不可见 | 难以诊断数值问题 |

### 2. 当前实现状态

- ✅ Pole-residue JSON 解析（Python scikit-rf → C++）
- ✅ 状态空间矩阵构建（双线性变换离散化）
- ⚠️ 数值稳定性问题（50极点级联系统）
- ❌ 频率响应相关性验证未通过

### 3. 预估工作量

修复 SystemC-AMS 版本：
- 稳定的状态空间实现：2-3天
- 频率响应验证调优：1-2天
- **总计：3-5天**（不保证成功）

---

## 方案A：继续 SystemC-AMS

### 可能的解决方案

#### A1: 降阶简化
```cpp
// 只使用主要的极点（如8-16个）
// 牺牲精度换取稳定性
```
- 优点：保持SystemC-AMS集成
- 缺点：相关性可能 < 0.95

#### A2: 分段线性化
```cpp
// 在不同频段使用不同的低阶近似
```
- 优点：局部稳定性好
- 缺点：实现复杂，频率交界处可能有问题

#### A3: 使用 sca_ltf_nd 直接传递函数
```cpp
// 将 pole-residue 转换为多项式系数
// 直接用 sca_ltf_nd(num, den, input)
```
- 优点：利用AMS内置稳定性
- 缺点：高阶多项式数值不稳定（之前试过系数爆炸）

---

## 方案B：纯C++实现（推荐）

### 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                    SerDes Channel CPP                       │
├─────────────────────────────────────────────────────────────┤
│  Python Interface (pybind11)                                │
│  ├─ load_s4p(filename) → poles, residues                    │
│  ├─ simulate(input) → output                                │
│  └─ get_frequency_response(freqs) → H(f)                    │
├─────────────────────────────────────────────────────────────┤
│  C++ Core (Eigen/Owl)                                       │
│  ├─ PoleResidueFilter                                       │
│  │   ├─ StateSpace (controllable canonical form)            │
│  │   ├─ BilinearTransform (Tustin)                          │
│  │   └─ ProcessSample()                                     │
│  ├─ FFT Convolution (optional)                              │
│  └─ SParameterLoader (Touchstone)                           │
├─────────────────────────────────────────────────────────────┤
│  External Libraries                                         │
│  ├─ Eigen3 (线性代数)                                        │
│  ├─ FFTW (FFT加速)                                          │
│  └─ pybind11 (Python绑定)                                    │
└─────────────────────────────────────────────────────────────┘
```

### 优势

| 方面 | SystemC-AMS | 纯C++ |
|------|-------------|-------|
| **数值稳定性** | 受限 | 完全可控（可用双精度/任意精度） |
| **算法选择** | 只能用sca_ltf_nd | 任意离散化方法（ZOH、Tustin、Euler） |
| **调试能力** | 黑盒 | 完全透明（可逐点验证） |
| **测试便利** | 需编译运行 | Python直接调用验证 |
| **复用现有代码** | 需重写 | 可用scipy.signal.lti作为参考 |
| **性能优化** | 受限 | SIMD、OpenMP、GPU加速 |

### 关键技术方案

#### B1: 状态空间实现（推荐）
```cpp
class PoleResidueFilter {
    // 离散时间状态空间
    MatrixXd Ad, Bd, Cd, Dd;
    VectorXd state;
    
public:
    void init(const vector<complex<double>>& poles,
              const vector<complex<double>>& residues,
              double constant, double dt) {
        // 双线性变换构建离散状态空间
        // 级联二阶节保证稳定性
    }
    
    double process(double input) {
        // x[n+1] = Ad*x[n] + Bd*u[n]
        // y[n] = Cd*x[n] + Dd*u[n]
        double output = (Cd * state + Dd * input).value();
        state = Ad * state + Bd * input;
        return output;
    }
};
```

#### B2: FFT卷积（备选）
```cpp
class FFTConvolutionFilter {
    // 预计算IR的FFT
    // Overlap-add或Overlap-save
    // 适合长IR、批处理
};
```

### Python 接口设计

```python
import serdes_channel as ch

# 加载S参数
channel = ch.Channel.from_s4p("peters_01_0605_B12_thru.s4p", n_poles=48)

# 直接获取频率响应（C++计算）
freqs = np.linspace(0, 20e9, 1000)
H = channel.freq_response(freqs)

# 时域仿真（C++加速）
import numpy as np
prbs = np.random.choice([-1, 1], size=100000)
output = channel.simulate(prbs, fs=100e9)

# 与Python scikit-rf对比验证
import skrf as rf
ntwk = rf.Network("peters_01_0605_B12_thru.s4p")
# 验证相关性...
```

### 开发计划（纯C++）

| 阶段 | 任务 | 时间 |
|------|------|------|
| 1 | Python scikit-rf 导出 pole-residue JSON | 已完成 ✅ |
| 2 | C++ 状态空间核心 + Eigen | 1天 |
| 3 | pybind11 Python绑定 | 0.5天 |
| 4 | 频率/时域响应验证 | 0.5天 |
| 5 | 与SystemC集成（如果需要）| 1天 |
| **总计** | | **3天** |

---

## 方案C：混合架构（最佳实践）

### 设计

```
┌────────────────────────────────────────────────────────────┐
│                    SystemC-AMS 仿真顶层                     │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    │
│  │    TX       │───▶│  Channel    │───▶│    RX       │    │
│  │  (AMS)      │    │  (Wrapper)  │    │  (AMS)      │    │
│  └─────────────┘    └──────┬──────┘    └─────────────┘    │
│                            │                                │
│                            ▼                                │
│                    ┌─────────────────────┐                 │
│                    │  C++ Channel Core   │                 │
│                    │  (Eigen + pybind11) │                 │
│                    └─────────────────────┘                 │
└────────────────────────────────────────────────────────────┘
```

### 优势
- 保留SystemC-AMS生态系统（TX/RX/时钟恢复等）
- Channel模块用C++实现获得数值稳定性
- Python接口便于独立验证和调试

---

## 建议

### 短期（本周）

**推荐方案C（混合架构）**：

1. **今天**：实现C++ Channel核心（状态空间）
2. **明天**：Python绑定 + 验证
3. **后天**：SystemC-AMS Wrapper集成

### 理由

1. **风险低**：C++数值计算可控，有scipy.signal参考
2. **效率高**：Python直接调用验证，无需每次编译
3. **可维护**：Pole-residue数据格式通用，可复用
4. **未来扩展**：容易添加新的通道模型（RLCG、损耗线等）

### 放弃纯SystemC-AMS的理由

- 时间成本高（估计还需3-5天调试）
- 数值问题根源于AMS框架限制
- 即使修复，后续维护困难

---

## 下一步行动

如果您同意方案C，我将：

1. 创建 `src/cpp_channel/` 目录
2. 实现 `PoleResidueFilter` 类（Eigen）
3. 添加 `python/channel_binding.cpp`（pybind11）
4. 创建验证脚本 `tests/test_channel_cpp.py`
5. 保留现有的SystemC-AMS接口作为Wrapper

预计 **2-3天** 完成可工作的版本。
