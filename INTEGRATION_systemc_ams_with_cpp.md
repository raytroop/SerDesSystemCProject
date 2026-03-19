# TX(AMS) + Channel(C++) + RX(AMS) 集成方案

## 核心结论

**完全可以一起工作！** 而且有多种集成方式，从简单到高性能。

---

## 方案1: C++ Channel 包装成 AMS TDF 模块（推荐）

这是最简单的方式 - 把C++ Channel核心包装在 AMS TDF 模块内部：

```cpp
// include/ams/channel_sparam.h
#include <systemc-ams>
#include "cpp_channel/pole_residue_filter.h"  // C++核心

namespace serdes {

class ChannelSParamTdf : public sca_tdf::sca_module {
public:
    sca_tdf::sca_in<double> in;   // 来自TX
    sca_tdf::sca_out<double> out; // 去往RX
    
private:
    // C++ Channel核心（不是AMS，是普通C++）
    serdes::cpp::PoleResidueFilter m_filter;
    
    void processing() override {
        double x_in = in.read();
        // 调用C++核心计算
        double y_out = m_filter.process(x_in);
        out.write(y_out);
    }
};

}
```

### 工作流程

```
┌─────────────────────────────────────────────────────────────────┐
│                     SystemC-AMS 仿真环境                         │
│                                                                  │
│  ┌──────────────┐      ┌──────────────────────┐     ┌──────────┐│
│  │   TX (TDF)   │──┐   │  Channel (TDF外壳)   │     │ RX (TDF) ││
│  │              │  │   │  ┌────────────────┐  │     │          ││
│  │ SystemC-AMS  │  └──▶│  │ C++核心        │  │────▶│SystemC-AMS│
│  │ sca_tdf模块   │      │  │ PoleResidueFilter│  │     │ 模块      ││
│  └──────────────┘      │  │ (纯Eigen/C++)   │  │     └──────────┘│
│                        │  └────────────────┘  │                 │
│                        │       ↑ 包装层        │                 │
│                        └──────────────────────┘                 │
│                                                                  │
│  时间步长: 统一由 SystemC-AMS 调度器控制 (如 10ps)                │
│  信号类型: sca_tdf::sca_signal<double> 连接所有模块               │
└─────────────────────────────────────────────────────────────────┘
```

### 关键特性

| 特性 | 说明 |
|------|------|
| **时间步长** | 由AMS统一调度，C++核心每步调用一次 `process()` |
| **信号传递** | 标准 `sca_tdf::sca_signal<double>` 连接TX-Channel-RX |
| **状态保存** | C++核心内部保存滤波器状态（跨时间步）|
| **零额外开销** | 每步只是C++函数调用，无数据拷贝 |

---

## 方案2: 直接替换内部实现（当前代码的方向）

实际上，您现在的代码结构已经是这种方案：

```cpp
// 当前 channel_sparam.cpp 中的 processing()
void ChannelSParamTdf::processing() {
    double x_in = in.read();
    double y_out = 0.0;
    
    switch (m_ext_params.method) {
        case ChannelMethod::POLE_RESIDUE:
            // 这里可以是纯C++实现！
            y_out = m_cpp_filter.process(x_in);
            break;
        case ChannelMethod::SIMPLE:
            y_out = process_simple(x_in);
            break;
        // ...
    }
    
    out.write(y_out);
}
```

**区别只是**：把不稳定的 `sca_ltf_nd` + 状态空间 换成 稳定的 C++ `Eigen` 实现。

---

## 方案3: 高性能批量处理（仿真加速）

如果需要仿真大量数据（如眼图），可以用块处理：

```cpp
class ChannelSParamTdf : public sca_tdf::sca_module {
private:
    serdes::cpp::PoleResidueFilter m_filter;
    std::vector<double> m_input_buffer;
    std::vector<double> m_output_buffer;
    int m_block_size = 1024;
    int m_buffer_idx = 0;
    
public:
    void processing() override {
        m_input_buffer[m_buffer_idx++] = in.read();
        
        if (m_buffer_idx >= m_block_size) {
            // C++批量处理（可用SIMD/多线程）
            m_filter.process_block(m_input_buffer, m_output_buffer);
            m_buffer_idx = 0;
        }
        
        out.write(m_output_buffer[m_buffer_idx]);
    }
};
```

---

## 方案4: Python验证 + C++部署 双模式

```cpp
#ifdef PYTHON_BINDING
    // pybind11导出，用于Python验证
    PYBIND11_MODULE(serdes_channel, m) {
        m.def("create_channel", &create_channel);
    }
#else
    // SystemC-AMS模块，用于系统仿真
    class ChannelSParamTdf : public sca_tdf::sca_module { ... };
#endif
```

**开发流程**:
1. Python验证算法正确性（可视化、对比scipy）
2. 直接编译进SystemC-AMS系统，无需修改

---

## 完整系统连接示例

```cpp
// tb/top/serdes_link_tb.cpp
#include <systemc-ams>
#include "ams/tx_driver.h"
#include "ams/channel_sparam.h"   // 内部是C++
#include "ams/rx_ctle.h"

SC_MODULE(SerDesLink) {
    // 信号
    sca_tdf::sca_signal<double> tx_out;
    sca_tdf::sca_signal<double> channel_out;
    sca_tdf::sca_signal<double> rx_out;
    
    // 模块
    TxDriverTdf* tx;
    ChannelSParamTdf* channel;  // 内部C++，外部AMS接口
    RxCtleTdf* rx;
    
    SC_CTOR(SerDesLink) {
        // TX (AMS) → Channel (AMS外壳+C++核心) → RX (AMS)
        tx = new TxDriverTdf("tx", tx_params);
        tx->out(tx_out);
        
        channel = new ChannelSParamTdf("channel", params, ext_params);
        channel->in(tx_out);      // 接收AMS信号
        channel->out(channel_out); // 输出AMS信号
        
        rx = new RxCtleTdf("rx", rx_params);
        rx->in(channel_out);
        rx->out(rx_out);
    }
};

int sc_main(int argc, char* argv[]) {
    SerDesLink top("top");
    sc_core::sc_start(1000, sc_core::SC_NS);
    return 0;
}
```

---

## 对比：纯SystemC-AMS vs C++核心

| 方面 | 纯SystemC-AMS | C++核心+AMS外壳 |
|------|--------------|----------------|
| **代码位置** | `src/ams/channel_sparam.cpp` | `src/cpp_channel/` + `src/ams/` |
| **数值计算** | 受限（只能用sca_ltf_nd） | 自由（Eigen, FFTW, 任意精度） |
| **TX/RX接口** | `sca_tdf::sca_signal<double>` | 完全相同 |
| **系统连接** | 标准AMS连接 | 标准AMS连接 |
| **仿真速度** | 一般 | 更快（SIMD优化） |
| **调试** | 困难 | Python验证 + C++部署 |

---

## 常见误区澄清

### ❌ 误区1: "C++代码不能和SystemC-AMS通信"
**✅ 事实**: C++代码**在**AMS模块内部运行，通过成员变量保存状态。

### ❌ 误区2: "需要时间同步代码"
**✅ 事实**: AMS调度器调用 `processing()`，C++核心被动执行，天然同步。

### ❌ 误区3: "需要数据格式转换"
**✅ 事实**: 都是 `double` 类型，直接传递，无转换开销。

---

## 推荐实现结构

```
serdes/
├── src/
│   ├── ams/
│   │   └── channel_sparam.cpp      # AMS外壳（不变）
│   └── cpp_channel/                # NEW: C++核心
│       ├── pole_residue_filter.h   # 状态空间滤波器
│       ├── pole_residue_filter.cpp # Eigen实现
│       └── python_binding.cpp      # pybind11绑定（可选）
├── include/ams/channel_sparam.h    # 包含C++核心头文件
└── tests/
    ├── test_channel_cpp.py         # Python验证
    └── test_channel_ams.cpp        # AMS集成测试
```

---

## 总结

**Channel用C++实现 ≠ 不能和AMS的TX/RX连接**

就像您可以用C++写算法，然后在Python中调用一样：
- C++核心 = 算法实现（稳定、高效）
- AMS外壳 = 接口适配（兼容SystemC生态）

**系统仍然是**: TX(AMS) → Channel(AMS+C++) → RX(AMS)

所有信号连接、时间调度、波形记录都保持不变。
