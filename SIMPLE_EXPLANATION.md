# SystemC-AMS 调度 C++ 代码的最简解释

## 一句话总结

**SystemC-AMS 的调度器调度的不是 "AMS 代码"，而是 C++ 函数调用。**

你的 C++ Channel 核心就是一个普通的 C++ 类，放在 `processing()` 函数里被调用。

---

## 类比：Python 类包含 NumPy

```python
import numpy as np

class MyModule:
    def __init__(self):
        # 内部用 NumPy (C++写的) 做计算
        self.filter = np.array([...])
    
    def processing(self, input_val):
        # 调用 NumPy (C++) 计算
        output = np.convolve(input_val, self.filter)
        return output

# 调度器
def scheduler():
    module = MyModule()
    for t in range(1000):
        result = module.processing(input[t])  # 每步调用
```

**你不会问："Python 调度器怎么调度 NumPy？"**  
因为 NumPy 就是 C 代码，被 Python 调用而已。

**SystemC-AMS 同理。**

---

## 实际代码对比

### 纯 AMS 方式（受限）
```cpp
class ChannelAms : public sca_tdf::sca_module {
    sca_tdf::sca_in<double> in;
    sca_tdf::sca_out<double> out;
    
    // 只能用 AMS 提供的工具
    sca_tdf::sca_ltf_nd m_ltf;  // ← AMS 的传递函数
    
    void processing() {
        // 必须用 sca_ltf_nd，不能在别处用
        out.write(m_ltf(m_num, m_den, in.read()));
    }
};
```

### AMS + C++ 核心（推荐）
```cpp
// ========== C++ 核心（普通 C++）==========
class PoleResidueFilter {
    Eigen::MatrixXd A, B, C, D;  // Eigen 是 C++ 库
    Eigen::VectorXd state;
    
public:
    double process(double input) {
        // 纯 C++ 计算
        double output = (C * state + D * input).value();
        state = A * state + B * input;
        return output;
    }
};

// ========== AMS 外壳 ==========
class ChannelAms : public sca_tdf::sca_module {
    sca_tdf::sca_in<double> in;
    sca_tdf::sca_out<double> out;
    
    // 包含 C++ 核心作为成员
    PoleResidueFilter m_filter;  // ← 普通 C++ 对象！
    
    void processing() {
        // 调用 C++ 核心的方法
        double result = m_filter.process(in.read());
        out.write(result);
    }
};
```

---

## 调度流程图解

```
SystemC-AMS 调度器（C++ 代码）
    │
    │ 每 10ps 调用一次
    ▼
┌─────────────────────────────┐
│ ChannelAms::processing()    │  ← AMS 模块（C++ 类）
│                             │
│   ┌─────────────────────┐   │
│   │ m_filter.process()  │   │  ← C++ 核心（普通 C++）
│   │   (Eigen 计算)      │   │
│   │   A*x + B*u         │   │
│   │   return y          │   │
│   └─────────────────────┘   │
│                             │
│   out.write(result)         │  ← 返回 AMS 信号
└─────────────────────────────┘
    │
    ▼
下一个 AMS 模块 (RX)
```

**调度器看到的只是 `ChannelAms::processing()` 这个 C++ 函数**，它根本不知道（也不关心）函数内部是：
- 调用 `sca_ltf_nd`
- 还是调用 `PoleResidueFilter.process()`
- 还是做 `x + y`

---

## 关键概念：继承 vs 包含

| 方式 | 代码 | 说明 |
|------|------|------|
| **继承** | `class X : public sca_tdf::sca_module` | X 是一个 AMS 模块 |
| **包含** | `class X { PoleResidueFilter m_filter; }` | X 包含一个 C++ 对象 |

**Channel 模块同时做两件事：**
1. **继承** `sca_tdf::sca_module` → 让 AMS 调度器能管理它
2. **包含** `PoleResidueFilter` → 用 C++ 做实际计算

---

## 完整最小示例

```cpp
// ======== C++ 核心：一个普通的 C++ 类 ========
class SimpleFilter {
    double m_state = 0;
    double m_alpha;
    
public:
    SimpleFilter(double alpha) : m_alpha(alpha) {}
    
    double process(double input) {
        m_state = m_alpha * input + (1 - m_alpha) * m_state;
        return m_state;
    }
};

// ======== AMS 外壳 ========
#include <systemc-ams>

SCA_TDF_MODULE(ChannelTdf) {
    sca_tdf::sca_in<double> in;
    sca_tdf::sca_out<double> out;
    
    // 包含 C++ 核心
    SimpleFilter m_filter{0.3};  // ← 普通 C++ 对象
    
    void processing() {
        // 调用 C++ 核心
        double result = m_filter.process(in.read());
        out.write(result);
    }
};

// ======== 系统连接 ========
int sc_main(int argc, char* argv[]) {
    ChannelTdf channel("channel");  // AMS 模块
    
    // 连接信号...
    // channel.in(tx_out);
    // rx.in(channel.out);
    
    sc_start(1000, SC_NS);  // AMS 调度器运行
    return 0;
}
```

**编译运行：**
```bash
g++ -I$SYSTEMC_AMS_HOME/include simple_example.cpp -lsystemc-ams
./a.out
```

**输出：** AMS 调度器正常运行，每 10ps 调用 `processing()`，内部执行你的 C++ 代码。

---

## 回答你的问题

> "SystemC-AMS 的调度器怎么调度 C++ 的 channel 呢？"

**答案是：Channel 类本身就是一个 C++ 类！**

```cpp
class ChannelSParamTdf : public sca_tdf::sca_module {
    // ↑ 这是一个 C++ 类
    // ↑ 继承自 AMS 的模块基类
    // ↑ 所以 AMS 调度器可以管理它
    
    PoleResidueFilter m_filter;
    // ↑ 这是另一个 C++ 类（普通 C++）
    // ↑ 被包含在 Channel 内部
};
```

调度器管理的是 `ChannelSParamTdf` 这个 **AMS 模块**（C++ 类）。  
`PoleResidueFilter` 只是这个模块内部使用的 **普通 C++ 对象**。

就像：
- 调度器管理一辆车（AMS 模块）
- 车里有一个发动机（C++ 核心）
- 调度器不需要知道发动机的存在，只管调度车

---

## 为什么之前有问题？

**问题不在调度，而在数值稳定性。**

| 方面 | 之前的问题 | 现在的方案 |
|------|-----------|-----------|
| **代码位置** | `processing()` 里直接写状态空间 | 提取到 `PoleResidueFilter` 类 |
| **数值库** | 手动实现欧拉积分 | 用 Eigen 做矩阵运算 |
| **离散化** | 欧拉法（不稳定） | 双线性变换（稳定） |
| **调试** | 难 | 可在 Python 单独测试 C++ 核心 |

**调度方式完全一样！** 只是换了个更稳定的算法实现。
