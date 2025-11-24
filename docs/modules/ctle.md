# CTLE 模块文档

**级别**：AMS 子模块（RX）  
**类名**：`RxCtleTdf`  
**当前版本**：v0.3 (2025-11-23)  
**状态**：生产就绪

---

## 1. 概述

连续时间线性均衡器（CTLE）是接收端的关键模块，用于补偿信道引入的高频衰减。本模块采用**差分输入/差分输出**架构，使用多零点/多极点传递函数实现频率选择性放大。

### 核心特性

- ✅ **差分架构**：完整的差分信号路径，支持共模抑制
- ✅ **灵活传递函数**：支持任意多零点/多极点配置
- ✅ **非理想效应建模**：输入偏移、输入噪声、输出饱和
- ✅ **PSRR建模**：电源噪声通过可配置传递函数耦合到输出
- ✅ **CMFB环路**：共模反馈环路，稳定输出共模电压
- ✅ **CMRR建模**：输入共模到差分输出的泄漏路径

### 版本历史

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| v0.1 | 2025-09 | 初始版本，单端接口 |
| v0.2 | 2025-10 | 新增PSRR/CMFB/CMRR功能 |
| v0.3 | 2025-11-23 | **⚠️ 破坏性更新**：改为差分接口，统一共模控制 |

> **⚠️ 重要提示**：v0.3是破坏性更新，单端接口（`in`, `out`）已移除，必须使用差分接口（`in_p`, `in_n`, `out_p`, `out_n`）。详见第5节迁移指南。

---

## 2. 模块接口

### 2.1 端口定义（TDF域）

#### 差分输入端口

```cpp
sca_tdf::sca_in<double> in_p;   // 差分输入正端
sca_tdf::sca_in<double> in_n;   // 差分输入负端
```

#### 电源端口

```cpp
sca_tdf::sca_in<double> vdd;    // 电源电压（用于PSRR建模）
```

> ⚠️ **重要**：即使不启用PSRR功能，`vdd`端口也**必须连接**（SystemC-AMS要求所有端口均需连接）。可连接到常数电源源。

#### 差分输出端口

```cpp
sca_tdf::sca_out<double> out_p; // 差分输出正端
sca_tdf::sca_out<double> out_n; // 差分输出负端
```

### 2.2 参数配置（RxCtleParams）

#### 基本参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `dc_gain` | double | 1.0 | 直流增益（线性倍数） |
| `zeros` | vector&lt;double&gt; | [] | 零点频率列表（Hz） |
| `poles` | vector&lt;double&gt; | [] | 极点频率列表（Hz） |
| `vcm_out` | double | 0.6 | 差分输出共模电压（V） |
| `offset_enable` | bool | false | 启用输入偏移 |
| `vos` | double | 0.0 | 输入偏移电压（V） |
| `noise_enable` | bool | false | 启用输入噪声 |
| `vnoise_sigma` | double | 0.0 | 噪声标准差（V，高斯分布） |
| `sat_min` | double | -0.5 | 输出最小电压（V） |
| `sat_max` | double | 0.5 | 输出最大电压（V） |

#### PSRR子结构（psrr）

电源抑制比路径配置，建模电源纹波对差分输出的影响。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enable` | bool | false | 启用PSRR建模 |
| `gain` | double | 0.0 | PSRR路径增益（线性倍数） |
| `zeros` | vector&lt;double&gt; | [] | 零点频率（Hz） |
| `poles` | vector&lt;double&gt; | [] | 极点频率（Hz） |
| `vdd_nom` | double | 1.0 | 名义电源电压（V） |

**工作原理**：`vdd_ripple = vdd - vdd_nom` → `H_psrr(s)` → 添加到差分输出

#### CMFB子结构（cmfb）

共模反馈环路配置，稳定输出共模电压到目标值`vcm_out`。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enable` | bool | false | 启用CMFB环路 |
| `bandwidth` | double | 1e6 | 环路带宽（Hz） |
| `loop_gain` | double | 1.0 | 环路增益（线性倍数） |

**工作原理**：测量`vcm_meas = 0.5*(out_p_prev + out_n_prev)` → 计算误差 → 环路滤波器 → 调整有效共模

#### CMRR子结构（cmrr）

共模抑制比路径配置，建模输入共模到差分输出的泄漏。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enable` | bool | false | 启用CMRR建模 |
| `gain` | double | 0.0 | CM→DIFF 泄漏增益（线性倍数） |
| `zeros` | vector&lt;double&gt; | [] | 零点频率（Hz） |
| `poles` | vector&lt;double&gt; | [] | 极点频率（Hz） |

**工作原理**：`vin_cm = 0.5*(in_p + in_n)` → `H_cmrr(s)` → 添加到差分输出

---

## 3. 行为模型与实现

### 3.1 信号处理流程

以下是`processing()`方法的详细处理流程：

#### 步骤1：读取差分和共模输入

```cpp
double vin_diff = in_p.read() - in_n.read();
double vin_cm = 0.5 * (in_p.read() + in_n.read());
```

- `vin_diff`：差分输入信号
- `vin_cm`：共模输入信号

#### 步骤2：添加输入偏移（可选）

```cpp
if (m_params.offset_enable) {
    vin_diff += m_params.vos;
}
```

- 只有当`offset_enable=true`时才添加偏移电压`vos`

#### 步骤3：注入输入噪声（可选）

```cpp
if (m_params.noise_enable && m_params.vnoise_sigma > 0) {
    double noise = m_noise_dist(m_rng);  // 高斯分布 N(0, sigma)
    vin_diff += noise;
}
```

- 使用Mersenne Twister随机数生成器
- 高斯分布，标准差为`vnoise_sigma`

#### 步骤4：主CTLE滤波

```cpp
if (m_H_ctle) {
    vout_diff = m_H_ctle->estimate_nd(t, vin_diff);
} else {
    vout_diff = m_params.dc_gain * vin_diff;
}
```

- 如果配置了零极点，使用`sca_ltf_nd`滤波器
- 否则直接应用DC增益

#### 步骤5：软饱和限制

```cpp
double Vsat = 0.5 * (m_params.sat_max - m_params.sat_min);
if (Vsat > 0) {
    vout_diff_sat = tanh(vout_diff / Vsat) * Vsat;
} else {
    vout_diff_sat = vout_diff;
}
```

- 使用`tanh`函数实现平滑的软饱和
- 避免硬限幅导致的谐波失真

#### 步骤6：PSRR路径（可选）

```cpp
double vout_psrr_diff = 0.0;
if (m_params.psrr.enable && m_H_psrr) {
    double vdd_ripple = vdd.read() - m_params.psrr.vdd_nom;
    vout_psrr_diff = m_H_psrr->estimate_nd(t, vdd_ripple);
}
```

- 计算电源纹波：`vdd_ripple = vdd - vdd_nom`
- 通过PSRR传递函数滤波

#### 步骤7：CMRR路径（可选）

```cpp
double vout_cmrr_diff = 0.0;
if (m_params.cmrr.enable && m_H_cmrr) {
    vout_cmrr_diff = m_H_cmrr->estimate_nd(t, vin_cm);
}
```

- 输入共模信号通过CMRR传递函数
- 模拟共模到差分的泄漏

#### 步骤8：差分输出合成

```cpp
double vout_total_diff = vout_diff_sat + vout_psrr_diff + vout_cmrr_diff;
```

- 合并主信号、PSRR路径、CMRR路径

#### 步骤9：共模与CMFB（可选）

**无CMFB时**：
```cpp
double vcm_eff = m_params.vcm_out;
```

**有CMFB时**：
```cpp
// 测量前一周期共模（避免代数环）
double vcm_meas = 0.5 * (m_out_p_prev + m_out_n_prev);
double e_cm = m_params.vcm_out - vcm_meas;

// 通过环路滤波器
double delta_vcm = m_H_cmfb->estimate_nd(t, e_cm);

// 有效共模
double vcm_eff = m_params.vcm_out + delta_vcm;
```

- ⚠️ **关键**：使用**前一周期输出**进行测量，避免代数环

#### 步骤10：生成差分输出

```cpp
double out_p_val = vcm_eff + 0.5 * vout_total_diff;
double out_n_val = vcm_eff - 0.5 * vout_total_diff;

out_p.write(out_p_val);
out_n.write(out_n_val);
```

#### 步骤11：更新内部状态

```cpp
// 保存当前输出用于下一周期的CMFB
m_out_p_prev = out_p_val;
m_out_n_prev = out_n_val;
m_vcm_prev = vcm_eff;
```

### 3.2 传递函数构建

`build_transfer_function()`方法从零极点列表构建`sca_ltf_nd`对象：

```cpp
// 形式：H(s) = dc_gain * ∏(s/ωz + 1) / ∏(s/ωp + 1)
sca_ltf_nd* build_transfer_function(
    double dc_gain,
    const std::vector<double>& zeros,  // Hz
    const std::vector<double>& poles   // Hz
)
```

**实现方法**：
1. 初始化分子 = `[dc_gain]`，分母 = `[1]`
2. 对每个零点，分子与`[1, 1/(2π*fz)]`卷积
3. 对每个极点，分母与`[1, 1/(2π*fp)]`卷积
4. 返回`sca_create_ltf_nd(num, den)`

### 3.3 软饱和函数

```cpp
double apply_saturation(double x, double sat_min, double sat_max) {
    double Vsat = 0.5 * (sat_max - sat_min);
    if (Vsat <= 0) return x;
    
    return tanh(x / Vsat) * Vsat;
}
```

**特性**：
- 平滑的饵和曲线
- 输出范围渐近于`[−Vsat, +Vsat]`
- 当`Vsat ≤ 0`时不进行饱和处理

---

## 4. 配置指南

### 4.1 最小配置（快速开始）

基本的CTLE功能，使用默认值：

```json
{
  "rx": {
    "ctle": {
      "zeros": [2e9],
      "poles": [30e9],
      "dc_gain": 1.5,
      "vcm_out": 0.6
    }
  }
}
```

### 4.2 标准配置（常用功能）

包含偏移、噪声和饱和限制：

```json
{
  "rx": {
    "ctle": {
      "zeros": [2e9, 5e9],
      "poles": [30e9, 50e9],
      "dc_gain": 2.0,
      "vcm_out": 0.6,
      "offset_enable": true,
      "vos": 0.005,
      "noise_enable": true,
      "vnoise_sigma": 0.001,
      "sat_min": -0.4,
      "sat_max": 0.8
    }
  }
}
```

### 4.3 完整配置（所有功能）

启用PSRR、CMFB和CMRR：

```json
{
  "rx": {
    "ctle": {
      "zeros": [2e9, 5e9],
      "poles": [30e9, 50e9],
      "dc_gain": 2.0,
      "vcm_out": 0.6,
      "offset_enable": true,
      "vos": 0.005,
      "noise_enable": true,
      "vnoise_sigma": 0.001,
      "sat_min": -0.4,
      "sat_max": 0.8,
      "psrr": {
        "enable": true,
        "gain": 0.05,
        "zeros": [1e6],
        "poles": [1e3, 1e7],
        "vdd_nom": 1.0
      },
      "cmfb": {
        "enable": true,
        "bandwidth": 1e6,
        "loop_gain": 2.0
      },
      "cmrr": {
        "enable": true,
        "gain": 1e-3,
        "zeros": [],
        "poles": [1e5]
      }
    }
  }
}
```

### 4.4 代码示例

```cpp
// 创建差分信号
sca_tdf::sca_signal<double> tx_out_p, tx_out_n;
sca_tdf::sca_signal<double> vdd_supply;
sca_tdf::sca_signal<double> ctle_out_p, ctle_out_n;

// 实例化CTLE模块
RxCtleTdf ctle("ctle", ctle_params);
ctle.in_p(tx_out_p);
ctle.in_n(tx_out_n);
ctle.vdd(vdd_supply);  // 必须连接
ctle.out_p(ctle_out_p);
ctle.out_n(ctle_out_n);

// 电源源（示例）
sca_tdf::sca_source<double> vdd_src("vdd_src");
vdd_src.set_value(1.0);  // 1.0V
vdd_src.y(vdd_supply);
```

---

## 5. 从v0.1迁移到v0.3

### 5.1 破坏性更改总结

| 项目 | v0.1 (旧版) | v0.3 (新版) |
|------|------------|------------|
| 端口接口 | `in`, `out` | `in_p`, `in_n`, `out_p`, `out_n`, `vdd` |
| 信号类型 | 单端 | 差分 |
| 必须连接 | 2个端口 | 5个端口 |
| 新增参数 | - | `vcm_out`, `vos`, `vnoise_sigma`, `sat_min/max`, `psrr`, `cmfb`, `cmrr` |

### 5.2 快速迁移检查清单

- [ ] **更新端口连接**：将`in/out`改为`in_p/in_n/out_p/out_n`
- [ ] **添加vdd端口**：连接到电源源（即使不用PSRR）
- [ ] **更新信号类型**：单端信号改为差分信号
- [ ] **更新配置文件**：添加`vcm_out`等新参数
- [ ] **测试验证**：确认差分信号传输正常

### 5.3 代码修改要点

#### 修改前（v0.1）：

```cpp
// 旧版 - 单端信号
sca_tdf::sca_signal<double> tx_out;
sca_tdf::sca_signal<double> ctle_out;

RxCtleTdf ctle("ctle", ctle_params);
ctle.in(tx_out);
ctle.out(ctle_out);
```

#### 修改后（v0.3）：

```cpp
// 新版 - 差分信号
sca_tdf::sca_signal<double> tx_out_p, tx_out_n;
sca_tdf::sca_signal<double> vdd_supply;
sca_tdf::sca_signal<double> ctle_out_p, ctle_out_n;

RxCtleTdf ctle("ctle", ctle_params);
ctle.in_p(tx_out_p);
ctle.in_n(tx_out_n);
ctle.vdd(vdd_supply);  // 新增！
ctle.out_p(ctle_out_p);
ctle.out_n(ctle_out_n);
```

### 5.4 单端↔差分转换

#### 单端转差分：

```cpp
SC_MODULE(Single2Diff) {
    sca_tdf::sca_in<double> in;
    sca_tdf::sca_out<double> out_p;
    sca_tdf::sca_out<double> out_n;
    double vcm;
    
    void processing() {
        double sig = in.read();
        out_p.write(vcm + 0.5 * sig);
        out_n.write(vcm - 0.5 * sig);
    }
    
    SCA_CTOR(Single2Diff) : vcm(0.6) {}
};
```

#### 差分转单端：

```cpp
SC_MODULE(Diff2Single) {
    sca_tdf::sca_in<double> in_p;
    sca_tdf::sca_in<double> in_n;
    sca_tdf::sca_out<double> out;
    
    void processing() {
        double diff = in_p.read() - in_n.read();
        out.write(diff);
    }
    
    SCA_CTOR(Diff2Single) {}
};
```

---

## 6. 测试与验证

### 6.1 基本功能测试

- ✅ **频响一致性**：Bode幅相响应与目标零极点模型一致
- ✅ **共模正确性**：`out_p/out_n`共模为`vcm_out`
- ✅ **偏移开关**：`offset_enable`控制下，输出偏移符合`vos`
- ✅ **噪声开关**：`noise_enable`控制下，输出噪底与`vnoise_sigma`对应
- ✅ **饱和行为**：输出在`sat_min/sat_max`范围内有效限制

### 6.2 PSRR验证

**测试方法**：

1. 启用PSRR：`psrr.enable = true`
2. 配置PSRR参数（增益、零极点）
3. 在`vdd`端口注入单频或多频纹波
4. 测量差分输出的纹波幅度
5. 计算抑制比：`PSRR_dB = 20*log10(Vdd_ripple / Vout_ripple)`

**示例电源源（带纹波）**：

```cpp
SC_MODULE(VddSource) {
    sca_tdf::sca_out<double> vdd;
    
    void processing() {
        double t = get_time().to_seconds();
        // 1.0V DC + 10mV @ 1MHz 纹波
        double v = 1.0 + 0.01 * sin(2*M_PI*1e6*t);
        vdd.write(v);
    }
    
    SCA_CTOR(VddSource) {}
};
```

### 6.3 CMFB验证

**测试方法**：

1. 启用CMFB：`cmfb.enable = true`
2. 测量输出共模电压：`vcm_meas = 0.5 * (out_p + out_n)`
3. 施加阶跃或慢变扰动
4. 验证`vcm_meas`最终收敛到`vcm_out`
5. 测量建立时间与配置的`bandwidth`一致

### 6.4 CMRR验证

**测试方法**：

1. 启用CMRR：`cmrr.enable = true`
2. 在输入叠加共模扫频信号
3. 测得差分输出中的残余曲线
4. 与`cmrr.gain`及配置的`zeros/poles`对比

### 6.5 测试检查清单

- [ ] 基本差分信号传输正常
- [ ] 频响曲线与零极点配置一致
- [ ] 输出共模电压为`vcm_out`
- [ ] 偏移开关功能正常（`offset_enable`）
- [ ] 噪声开关功能正常（`noise_enable`）
- [ ] 饱和限制工作正常（大信号输入）
- [ ] PSRR功能（如果启用）
- [ ] CMFB功能（如果启用）
- [ ] CMRR功能（如果启用）
- [ ] 配置文件加载正常
- [ ] 与下游模块（VGA、Sampler）连接正常

---

## 7. 技术要点与最佳实践

### 7.1 避免代数环

**问题**：CMFB环路如果直接使用当前周期输出进行测量，会造成代数环（输出依赖于输出）。

**解决方案**：
- CMFB使用**前一周期的输出**（`m_out_p_prev`, `m_out_n_prev`）进行测量
- 这引入了一个时间步的延迟，但避免了代数环
- 对于低频CMFB（带宽通常为1MHz），这个延迟可以忽略不计

### 7.2 多零点/多极点传递函数

**实现方法**：
- 使用动态构建的`sca_ltf_nd`对象
- 支持任意数量的零点和极点
- 自动处理多项式卷积

**形式**：
```
H(s) = dc_gain * ∏(s/ωz + 1) / ∏(s/ωp + 1)
```

**注意事项**：
- 零极点总数建议 ≤ 10
- 过高阶滤波器可能导致数值不稳定

### 7.3 软饱和

**实现**：
```cpp
y = tanh(x / Vsat) * Vsat
```

**优点**：
- 平滑的饱和特性，无锹变
- 减少谐波失真
- 符合实际电路行为

### 7.4 可选功能

- PSRR、CMFB、CMRR均可**独立启用/禁用**
- 通过参数的`enable`标志控制
- 未启用时不创建对应的滤波器对象，节省内存和计算

### 7.5 时间步设置

**默认设置**：
```cpp
set_timestep(1.0 / 100e9);  // 10ps
```

**调整原则**：
- 采样率应远高于最高极点频率
- 建议：`f_sample ≥ 20-50 × f_pole_max`
- 对于>50GHz的极点，需要调整为更小的时间步

### 7.6 滤波器稳定性

**需要注意**：
- 零极点配置需保证滤波器稳定性
- 所有极点必须在左半平面（负实部）
- 采样率不足可能导致混叠效应

### 7.7 VDD端口连接

**必须连接**：
- 即使不使用PSRR功能，`vdd`端口也必须连接
- SystemC-AMS要求所有端口都必须连接
- 可连接到常数源（例如`sca_tdf::sca_source<double>`）

### 7.8 内存管理

**当前问题**：
- `build_transfer_function()`返回的指针使用`new`分配
- 当前未实现析构函数释放内存

**建议改进**：
```cpp
~RxCtleTdf() {
    if (m_H_ctle) delete m_H_ctle;
    if (m_H_psrr) delete m_H_psrr;
    if (m_H_cmrr) delete m_H_cmrr;
    if (m_H_cmfb) delete m_H_cmfb;
}
```

### 7.9 性能优化

1. **采样率设置**：
   - 默认时间步10ps适用于大多数应用
   - 超高速应用(>50GHz)需要手动调整

2. **滤波器复杂度**：
   - 零极点数量影响仿真速度
   - 建议零极点总数 ≤ 10

3. **内存使用**：
   - 每个CTLE实例最多创建4个`ltf_nd`对象
   - 大规模仿真时注意内存消耗

---

## 8. 常见问题FAQ

### Q1：旧版测试平台无法编译

**原因**：端口名称和接口已更改

**解决方案**：
- 按照第5节迁移指南更新测试平台代码
- 将单端连接改为差分连接
- 添加`vdd`端口连接

### Q2：vdd端口未连接导致编译错误

**原因**：SystemC-AMS要求所有端口必须连接

**解决方案**：
- 如果不使用PSRR，连接到常数电源源：
  ```cpp
  sca_tdf::sca_source<double> vdd_source("vdd_source");
  vdd_source.set_value(1.0);  // 1.0V
  sca_tdf::sca_signal<double> vdd_sig;
  vdd_source.y(vdd_sig);
  ctle.vdd(vdd_sig);
  ```
- 如果使用PSRR，连接到带纹波的电源源（见第6.2节）

### Q3：配置文件加载失败

**原因**：缺少新增的参数字段

**解决方案**：
- 方案A：更新配置文件，添加所有新字段（参见第4节）
- 方案B：修改config_loader支持可选字段和默认值

### Q4：如何验证PSRR功能？

**测试方法**：
1. 启用PSRR：`psrr.enable = true`
2. 配置PSRR参数（增益、零极点）
3. 在`vdd`端口注入单频或多频纹波
4. 测量差分输出的纹波幅度
5. 计算抑制比：`PSRR_dB = 20*log10(Vdd_ripple / Vout_ripple)`

详细示例见第6.2节。

### Q5：如何验证CMFB功能？

**测试方法**：
1. 启用CMFB：`cmfb.enable = true`
2. 测量输出共模电压：`vcm_meas = 0.5 * (out_p + out_n)`
3. 施加阶跃或慢变扰动
4. 验证`vcm_meas`最终收敛到`vcm_out`
5. 测量建立时间与配置的`bandwidth`一致

详细示例见第6.3节。

### Q6：差分输出如何转回单端？

**方法**：
```cpp
SC_MODULE(Diff2Single) {
    sca_tdf::sca_in<double> in_p;
    sca_tdf::sca_in<double> in_n;
    sca_tdf::sca_out<double> out;
    
    void processing() {
        double diff = in_p.read() - in_n.read();
        out.write(diff);
    }
    
    SCA_CTOR(Diff2Single) {}
};
```

详细示例见第5.4节。

---

## 9. 参考信息

### 9.1 相关文件

| 文件 | 路径 | 说明 |
|------|------|------|
| 参数定义 | `/include/common/parameters.h` | `RxCtleParams`结构体 |
| 头文件 | `/include/ams/rx_ctle.h` | `RxCtleTdf`类声明 |
| 实现文件 | `/src/ams/rx_ctle.cpp` | `RxCtleTdf`类实现 |
| 配置示例 | `/config/default.json` | JSON配置示例 |
| 配置示例 | `/config/default.yaml` | YAML配置示例 |
| 测试平台 | `/tb/simple_link_tb.cpp` | 简单链路测试 |

### 9.2 依赖项

**必需**：
- SystemC 2.3.4
- SystemC-AMS 2.3.4
- C++14 标准
- C++ 标准库：`<random>`, `<cmath>`, `<vector>`

**建模域建议**：
- CTLE/PSRR/CMRR：推荐使用TDF的`sca_tdf::sca_ltf_nd`
- CMFB闭环：如需更自然的连续实现可采用LSF/ELN

### 9.3 编译注意事项

- 需要SystemC-2.3.4和SystemC-AMS-2.3.4
- C++14标准
- 使用`<random>`头文件（C++11特性）
- 使用`<cmath>`中的`tanh`函数

### 9.4 实现与文档对应关系

| 文档章节 | 实现位置 |
|---------|----------|
| 接口 - 差分端口 | `rx_ctle.h`: in_p, in_n, out_p, out_n |
| 接口 - 电源端口 | `rx_ctle.h`: vdd |
| 配置键 | `parameters.h`: RxCtleParams |
| 参数 - 主传递函数 | `processing()`: Step 4 |
| 参数 - 偏移 | `processing()`: Step 2 |
| 参数 - 噪声 | `processing()`: Step 3 |
| 参数 - 饱和 | `processing()`: Step 5 |
| 参数 - PSRR | `processing()`: Step 6 |
| 参数 - CMFB | `processing()`: Step 9 |
| 参数 - CMRR | `processing()`: Step 7 |
| 行为模型 - 11个步骤 | `processing()`: Steps 1-11 |

### 9.5 建议的后续工作

1. **添加析构函数**：释放动态分配的滤波器内存
2. **参数验证**：在构造函数中验证参数有效性
3. **测试用例**：完善频响、PSRR、CMFB、CMRR测试
4. **配置文件示例**：更新`config/default.json`和`default.yaml`

### 9.6 技术支持

如遇到问题，请参考：
1. 本文档 - 完整设计文档
2. 项目GitHub仓库：`https://github.com/lewisliuliuliu/SerDesSystemCProject`
3. GitHub Issues

---

**文档版本**：v0.3  
**最后更新**：2025-11-23  
**作者**：SerDes Project Team
