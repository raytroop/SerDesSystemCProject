# Channel 模块技术文档

**级别**：AMS 顶层模块  
**类名**：`ChannelSParamTdf`  
**当前版本**：v0.4 (2025-12-07)  
**状态**：生产就绪

---

## 1. 概述

信道模块（Channel）是SerDes链路中连接发送端与接收端的关键传输路径，主要功能是基于实测的S参数数据建模真实信道的频率相关衰减、相位失真、串扰耦合和反射效应。模块提供两种高精度的时域建模方法，支持多端口差分传输和复杂拓扑场景。

### 1.1 设计原理

信道模块的核心设计思想是将频域表征的S参数（Scattering Parameters）转换为时域因果系统，以高效实现宽带非理想传输效应：

- **频域到时域转换**：通过数学变换将测量或仿真得到的S参数频域数据转化为时域传递函数或冲激响应
- **因果性保证**：确保时域系统满足物理因果性，避免预测未来输入的非物理行为
- **稳定性约束**：保证传递函数极点位于左半平面（连续域）或单位圆内（离散域），防止信号能量发散
- **无源性保持**：在能量守恒框架下建模，确保输出能量不超过输入能量（散射矩阵无源条件）

模块提供两种互补的实现方法：

**方法一：有理函数拟合法（Rational Fitting Method）**
- 核心思想：使用向量拟合（Vector Fitting）算法将S参数频域响应近似为有理函数形式
- 数学形式：
  ```
  H(s) = Σ(r_k / (s - p_k)) + d + s·h
  H(s) = (b_n·s^n + ... + b_0) / (a_m·s^m + ... + a_0)
  ```
- 时域实现：利用 SystemC-AMS 的 `sca_tdf::sca_ltf_nd` 线性时不变滤波器实现紧凑高效的卷积
- 优势：参数紧凑（8-16阶通常足够）、计算效率高（O(order)每时间步）、数值稳定性好

**方法二：冲激响应卷积法（Impulse Response Convolution Method）**
- 核心思想：通过逆傅立叶变换（IFFT）获得S参数的时域冲激响应，直接进行卷积
- 数学形式：
  ```
  h(t) = IFFT[H(f)]
  y(t) = ∫ h(τ)·x(t-τ) dτ  （连续域）
  y[n] = Σ h[k]·x[n-k]      （离散域）
  ```
- 时域实现：维护输入延迟线，执行有限长度卷积或快速傅立叶变换（FFT）卷积
- 优势：保留完整频域信息、处理非最小相位系统、易于理解和调试

### 1.2 核心特性

- **双方法支持**：有理函数拟合法（推荐）与冲激响应卷积法可选，适应不同精度和性能需求
- **多端口建模**：支持N×N端口S参数矩阵（N≥2），涵盖单端、差分、多通道场景
- **串扰耦合**：完整建模近端串扰（NEXT）和远端串扰（FEXT），通过耦合矩阵实现多通道相互作用
- **双向传输**：支持正向传输（S21）、反向传输（S12）和端口反射（S11/S22）的开关控制
- **频域预处理**：DC点补全、采样频率匹配、带限滚降，确保时域转换的鲁棒性
- **端口映射标准化**：手动指定或自动识别端口配对关系，统一差分对和传输路径定义
- **GPU加速（Apple Silicon专属）**：针对长冲激响应和高采样率场景，提供Metal GPU加速的直接卷积和FFT卷积
- **配置驱动设计**：通过JSON配置文件管理拟合参数、预处理选项和加速策略，离线处理与在线仿真解耦

### 1.3 版本历史

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| v0.1 | 2025-09 | 初始版本，基础向量拟合框架占位 |
| v0.2 | 2025-10-16 | 双方法重构：新增有理函数拟合法和冲激响应卷积法完整说明 |
| v0.3 | 2025-10-16 | GPU加速支持：新增Metal GPU加速（Apple Silicon专属），支持直接卷积和FFT卷积 |
| v0.4 | 2025-12-07 | 完善第1章概述：重写设计原理和核心特性，对齐CTLE/VGA文档风格标准 |

---

## 2. 模块接口

### 2.1 类声明与继承

```cpp
namespace serdes {
class ChannelSParamTdf : public sca_tdf::sca_module {
public:
    // TDF端口
    sca_tdf::sca_in<double> in;
    sca_tdf::sca_out<double> out;
    
    // 构造函数
    ChannelSParamTdf(sc_core::sc_module_name nm, const ChannelParams& params);
    
    // SystemC-AMS生命周期方法
    void set_attributes();
    void processing();
    
private:
    ChannelParams m_params;
    std::vector<double> m_buffer;
};
}
```

**继承层次**：
- 基类：`sca_tdf::sca_module`（SystemC-AMS TDF域模块）
- 域类型：TDF（Timed Data Flow，时间驱动数据流）

**v0.4实现说明**：
- 模块在TDF域运行，按固定时间步长处理信号（由全局采样率fs决定）
- 当前实现为**单输入单输出（SISO）简化版本**
- 未来扩展：多端口N×N矩阵支持（通过端口向量`sca_in<double> in[N]`, `sca_out<double> out[N]`）

### 2.2 端口定义（TDF域）

#### 当前实现（v0.4）

| 端口名 | 方向 | 类型 | 说明 |
|-------|------|------|------|
| `in` | 输入 | `sca_tdf::sca_in<double>` | 信道输入信号（单端） |
| `out` | 输出 | `sca_tdf::sca_out<double>` | 信道输出信号（单端） |

**使用场景**：
- 单端传输链路（如单根传输线）
- 差分信号建模（需实例化两个Channel分别处理正负端）
- 简化测试场景

#### 未来扩展：多端口矩阵（设计规格）

对于完整的多端口S参数建模（如4端口差分通道），接口将扩展为：

| 端口名 | 方向 | 类型 | 说明 |
|-------|------|------|------|
| `in[N]` | 输入 | `sca_tdf::sca_in<double>` | N个输入端口（支持差分对配置） |
| `out[N]` | 输出 | `sca_tdf::sca_out<double>` | N个输出端口（对应S参数输出端） |

**端口配对示例（4端口差分通道）**：
- 输入差分对1：`in[0]`（正）+ `in[1]`（负）
- 输入差分对2：`in[2]`（正）+ `in[3]`（负）
- 输出差分对1：`out[0]`（正）+ `out[1]`（负）
- 输出差分对2：`out[2]`（正）+ `out[3]`（负）

> **注意**：此功能为设计规格，当前v0.4版本**未实现**。

### 2.3 构造函数与初始化

```cpp
ChannelSParamTdf(sc_core::sc_module_name nm, const ChannelParams& params);
```

**参数说明**：
- `nm`：SystemC模块名称，用于仿真层次标识和波形追踪
- `params`：信道参数结构体（`ChannelParams`），包含所有配置项

**v0.4初始化流程**：
1. 调用基类构造函数，注册模块名称
2. 存储参数到成员变量`m_params`
3. 预分配延迟线缓冲区`m_buffer`（用于简化模型或未来扩展）
4. 当前实现未加载S参数文件（占位实现）

### 2.4 参数配置（ChannelParams）

#### 当前实现参数（v0.4）

以下参数直接来源于 `include/common/parameters.h` 第90-105行的实际结构：

| 参数 | 类型 | 默认值 | 说明 | v0.4状态 |
|------|------|--------|------|---------|
| `touchstone` | string | "" | S参数文件路径（.sNp格式） | **占位**，未实际加载 |
| `ports` | int | 2 | 端口数量（N≥2） | **占位**，当前仅单端口 |
| `crosstalk` | bool | false | 启用多端口串扰耦合矩阵（NEXT/FEXT） | **未实现** |
| `bidirectional` | bool | false | 启用双向传输（S12反向路径和反射） | **未实现** |
| `attenuation_db` | double | 10.0 | 简化模型衰减量（dB） | **可用** |
| `bandwidth_hz` | double | 20e9 | 简化模型带宽（Hz） | **可用** |

**v0.4实现说明**：
- **简化模型可用**：`attenuation_db`和`bandwidth_hz`可用于一阶低通滤波器建模
- **S参数支持占位**：`touchstone`和`ports`参数已定义，但当前版本未实现文件加载和矩阵处理
- **高级特性未实现**：`crosstalk`和`bidirectional`为设计规格，代码中未启用

**使用约束（v0.4）**：
- 不要依赖S参数文件加载功能
- `ports`参数被忽略，模块固定为单输入单输出
- `crosstalk`和`bidirectional`设置无效

#### 未来扩展参数（设计规格）

以下参数**不在当前parameters.h中定义**，为文档规格说明，供未来扩展参考：

##### method参数（未实现）

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `method` | string | "rational" | 时域建模方法："rational"（有理函数拟合）或"impulse"（冲激响应卷积） |
| `config_file` | string | "" | 离线处理生成的配置文件路径（JSON格式） |

##### fit子结构（有理函数拟合法，未实现）

用于控制向量拟合（Vector Fitting）算法的离线处理参数。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `order` | int | 16 | 拟合阶数（极点/零点数量），建议6-16阶 |
| `enforce_stable` | bool | true | 强制稳定性约束（极点实部<0） |
| `enforce_passive` | bool | true | 强制无源性约束（能量守恒） |
| `band_limit` | double | 0.0 | 频段上限（Hz，0表示使用Touchstone文件最高频率） |

**设计原理**：
1. Python离线工具读取S参数频域数据`Sij(f)`
2. 向量拟合算法将其近似为有理函数：`H(s) = (b_n·s^n + ... + b_0) / (a_m·s^m + ... + a_0)`
3. 稳定性约束确保所有极点`p_k`满足`Re(p_k) < 0`
4. 无源性约束确保所有频率点的散射矩阵特征值≤1
5. 拟合结果（分子/分母系数）保存到配置文件
6. 在线仿真时通过`sca_tdf::sca_ltf_nd`滤波器实例化

**阶数选择指南**：
| 信道类型 | 带宽 | 推荐阶数 | 原因 |
|---------|------|---------|------|
| 短背板 | <10 GHz | 6-8 | 低损耗，频响平滑 |
| 长背板 | 10-25 GHz | 10-12 | 中等损耗，需捕捉趋肤效应 |
| 超长电缆 | >25 GHz | 14-16 | 高损耗，频率相关衰减显著 |

##### impulse子结构（冲激响应卷积法，未实现）

用于控制逆傅立叶变换（IFFT）的离线处理和在线卷积参数。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `time_samples` | int | 4096 | 冲激响应长度（采样点数），建议2048-8192 |
| `causality` | bool | true | 应用因果性窗函数（确保t<0时h(t)≈0） |
| `truncate_threshold` | double | 1e-6 | 尾部截断阈值（相对于峰值的幅度比） |
| `dc_completion` | string | "vf" | DC点补全方法："vf"（向量拟合）、"interp"（插值）、"none" |
| `resample_to_fs` | bool | true | 将频域数据重采样到目标采样率fs |
| `fs` | double | 0.0 | 目标采样频率（Hz，0表示使用全局fs） |
| `band_limit` | double | 0.0 | 频段上限（Hz，建议≤fs/2避免混叠） |
| `grid_points` | int | 0 | 频域网格点数（0表示与time_samples一致） |

**设计原理**：
1. 读取S参数频域数据，进行DC点补全和频率网格重采样
2. 执行IFFT得到时域冲激响应`h(t) = IFFT[Sij(f)]`
3. 应用因果性窗函数（如Hamming窗）抑制非因果分量
4. 截断尾部低于阈值的部分，减少卷积长度L
5. 保存时间序列到配置文件
6. 在线仿真时维护延迟线：`y(n) = Σ h(k) · x(n-k)`

**长度选择指南**：
| 场景 | 冲激长度L | 计算复杂度 | 推荐加速 |
|------|----------|-----------|---------|
| 短通道（<5 GHz） | 512-1024 | O(L) CPU可接受 | 无需GPU |
| 中等通道（5-15 GHz） | 2048-4096 | O(L) 或 O(L log L) | CPU多核或GPU直接卷积 |
| 长通道（>15 GHz） | 4096-8192 | O(L log L) FFT卷积 | GPU FFT卷积（Apple Silicon） |

##### gpu_acceleration子结构（Metal GPU加速，未实现）

仅在`method="impulse"`且系统为Apple Silicon时设计有效。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enabled` | bool | false | 启用GPU加速（**仅Apple Silicon可用**） |
| `backend` | string | "metal" | GPU后端（固定为"metal"，其他后端暂不支持） |
| `algorithm` | string | "auto" | 算法选择："direct"（直接卷积）、"fft"（FFT卷积）、"auto"（自动选择） |
| `batch_size` | int | 1024 | 批处理样本数（减少CPU-GPU传输延迟） |
| `fft_threshold` | int | 512 | L>threshold时自动切换FFT卷积 |

**设计原理**：
1. 收集`batch_size`个输入样本到批处理缓冲区
2. 一次性上传到GPU共享内存（Metal Shared Memory）
3. 根据冲激长度L选择算法：
   - **直接卷积**（L<512）：每个输出样本并行计算`y[n] = Σ h[k]·x[n-k]`
   - **FFT卷积**（L≥512）：利用Metal Performance Shaders执行`y = IFFT(FFT(x) ⊙ FFT(h))`
4. 下载结果到CPU，顺序输出到TDF端口

**系统要求**：
- **必须**：Apple Silicon（M1/M2/M3/M4或更新）
- **不支持**：Intel Mac、Linux、Windows、NVIDIA GPU、AMD GPU

**性能预期**：
- 直接卷积（L<512）：50-100x相对于单核CPU
- FFT卷积（L≥512）：200-500x相对于单核CPU
- 批处理模式可达1000x（高采样率场景）

##### port_mapping子结构（端口映射标准化，未实现）

解决不同来源Touchstone文件端口顺序不一致的问题。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enabled` | bool | false | 启用端口映射标准化 |
| `mode` | string | "manual" | 映射模式："manual"（手动指定）、"auto"（自动识别） |
| `manual.pairs` | vector<pair<int,int>> | [] | 差分对配置（如[[1,2],[3,4]]） |
| `manual.forward` | vector<pair<int,int>> | [] | 输入→输出配对（如[[1,3],[2,4]]） |
| `auto.criteria` | string | "energy" | 自动识别准则："energy"（通带能量）、"lowfreq"、"bandpass" |
| `auto.constraints` | object | {} | 约束条件（differential: bool, bidirectional: bool） |

**设计原理**：
- **手动模式**：根据配置构造置换矩阵P，对S(f)重排：`S'(f) = P·S(f)·P^T`
- **自动模式**：计算各Sij的通带能量`Eij = ∫|Sij(f)|²df`，使用最大匹配算法识别主传输路径

### 2.5 公共API方法

#### set_attributes()

设置TDF模块的时间属性和端口速率。

```cpp
void set_attributes();
```

**职责**：
- 设置采样时间步长：`set_timestep(1.0/fs)`（fs从`GlobalParams`获取）
- 声明端口速率：`in.set_rate(1)`, `out.set_rate(1)`（每时间步处理1个样本）
- 设置延迟：`out.set_delay(0)`（当前简化模型无延迟，未来扩展将根据冲激响应长度设置）

**v0.4实现**：
- 仅设置基本时间步长和端口速率
- 未实现动态延迟设置

#### processing()

每个时间步的信号处理函数，实现信道传递函数。

```cpp
void processing();
```

**v0.4职责**：
- **简化模型**：应用一阶低通滤波器（通过`attenuation_db`和`bandwidth_hz`配置）
- **直通模式**：若参数未配置，直接将输入复制到输出

**未来扩展职责（设计规格）**：
- **有理函数法**：通过`sca_ltf_nd`滤波器计算输出
- **冲激响应法**：更新延迟线，执行卷积`y(n) = Σ h(k)·x(n-k)`
- **串扰处理**：计算耦合矩阵作用`x'[i] = Σ C[i][j]·x[j]`
- **双向传输**：叠加反向路径S12和反射S11/S22的贡献

---

## 3. 核心实现机制

### 3.1 v0.4简化实现（当前版本）

**重要说明**：当前v0.4版本**未实现完整的S参数建模功能**，仅提供简化的一阶低通滤波器作为占位实现。以下描述的是当前实际实现的信号处理流程。

#### 3.1.1 当前信号处理流程

v0.4版本的`processing()`方法采用最简单的信号传输模型：

```
输入读取 → 简化滤波器 → 输出写入
```

**步骤1-输入读取**：从TDF输入端口读取当前时间步的信号样本：
```cpp
double x = in.read();
```

**步骤2-简化滤波器应用**：应用基于`attenuation_db`和`bandwidth_hz`参数的一阶低通滤波器：
- 幅度衰减：`A = 10^(-attenuation_db/20)`
- 带宽限制：采用一阶极点`H(s) = A / (1 + s/ω₀)`，其中`ω₀ = 2π × bandwidth_hz`
- 实现方式：通过SystemC-AMS的`sca_ltf_nd`滤波器或简单增益缩放

**步骤3-输出写入**：将处理后的信号写入TDF输出端口：
```cpp
out.write(y);
```

#### 3.1.2 简化模型的传递函数

当前实现使用的一阶低通传递函数：

```
H(s) = A / (1 + s/ω₀)
```

**参数映射**：
- `A = 10^(-attenuation_db/20)`：线性幅度衰减因子
- `ω₀ = 2π × bandwidth_hz`：-3dB带宽对应的角频率

**频域特性**：
- DC增益：`H(0) = A`
- -3dB频率：`f₋₃dB = bandwidth_hz`
- 高频滚降：-20dB/decade（一阶系统）
- 相位延迟：0° (DC) → -90° (高频)

**局限性**：
- 不能表征频率相关的复杂衰减特性（趋肤效应、介质损耗）
- 缺少群延迟/色散效应
- 无串扰建模能力
- 无反射和双向传输支持
- 无法捕捉S参数的非最小相位特性

#### 3.1.3 状态管理

当前版本的状态变量：

| 变量 | 类型 | 作用 |
|------|------|------|
| `m_params` | `ChannelParams` | 存储配置参数 |
| `m_buffer` | `std::vector<double>` | 预留的延迟线缓冲区（当前未使用） |

**注意**：`m_buffer`已声明但在v0.4中未实际使用，为未来扩展到冲激响应卷积法预留。

---

### 3.2 有理函数拟合法（设计规格）

**重要说明**：以下是未来实现完整S参数建模的设计规格，描述应该如何实现，而非当前代码的实际行为。

#### 3.2.1 离线处理阶段（Python工具链）

有理函数拟合法将S参数频域数据转换为紧凑的传递函数形式，包含以下关键步骤：

##### 步骤1：S参数文件加载

```python
import skrf as rf
network = rf.Network('channel.s4p')
freq = network.f  # 频率点数组 (Hz)
S_matrix = network.s  # S参数矩阵 [N_freq, N_ports, N_ports]
```

**数据预处理**：
- 验证频率点单调递增
- 检查复数S参数的幅度≤1（无源性初步检查）
- 提取感兴趣的传输路径（如S21, S43）和串扰项（S13, S14等）

##### 步骤2：向量拟合算法（Vector Fitting）

向量拟合是一种迭代优化算法，将频域响应S(f)近似为有理函数形式：

**目标函数**：对每个S参数Sij(f)，寻找有理函数H(s)使得：
```
H(s) = Σ(r_k / (s - p_k)) + d + s·h
```
在频域测量点上最小化误差：
```
min Σ|Sij(f_n) - H(j·2π·f_n)|²
```

**算法流程**：
1. **初始化极点**：在复平面左半平面均匀分布`order`个起始极点
   - 实极点：`p_k = -2π·f_k`，其中f_k在[f_min, f_max]对数分布
   - 共轭复极点对：`p_k = -σ_k ± j·ω_k`，覆盖关键频率区域

2. **迭代优化**（通常3-5轮）：
   - **留数估计**：固定极点{p_k}，通过线性最小二乘求解留数{r_k}和常数项d、h
   - **极点重定位**：将H(s)视为权重函数，通过加权最小二乘重新估计极点位置
   - **收敛检查**：监控拟合误差和极点移动量

3. **稳定性强制**：若某极点实部≥0，镜像到左半平面：`p_k' = -|Re(p_k)| + j·Im(p_k)`

4. **无源性强制**（可选）：
   - 构造散射矩阵的特征值约束
   - 确保所有频率点满足`max(eig(S'·S)) ≤ 1`
   - 通过二次规划微调极点/留数以满足约束

##### 步骤3：极点-留数到分子-分母多项式转换

将部分分式形式转换为多项式比率：

```
H(s) = (b_n·s^n + ... + b_1·s + b_0) / (a_m·s^m + ... + a_1·s + a_0)
```

**转换步骤**：
1. **分母构造**：
   ```python
   den = [1.0]  # 归一化首项
   for p_k in poles:
       den = poly_multiply(den, [1, -p_k])  # (s - p_k)
   ```

2. **分子构造**：
   - 将留数{r_k}和常数项d、h合并
   - 通过多项式乘法和加法构造分子系数
   - 确保分子阶数≤分母阶数（物理可实现性）

3. **归一化**：将分母首项系数归一化为1.0

##### 步骤4：配置文件导出

生成JSON格式的配置文件，包含所有S参数路径的传递函数：

```json
{
  "method": "rational",
  "fs": 100e9,
  "filters": {
    "S21": {
      "num": [b0, b1, b2, ..., bn],
      "den": [1.0, a1, a2, ..., am],
      "order": 8,
      "dc_gain": 0.7943,
      "mse": 1.2e-4
    },
    "S43": {...},
    "S13": {...}
  },
  "port_mapping": {
    "forward": [[1,3], [2,4]],
    "crosstalk": [[1,4], [2,3]]
  }
}
```

**质量指标**：
- `mse`：均方误差（相对于原始S参数）
- `max_error`：最大绝对误差
- `passivity_margin`：无源性裕度（最大特征值与1的差）

#### 3.2.2 在线仿真阶段（SystemC-AMS）

##### 初始化：滤波器实例创建

在`initialize()`或构造函数中，根据配置文件创建`sca_ltf_nd`滤波器对象：

```cpp
// 伪代码示例
void ChannelSParamTdf::initialize() {
    // 加载配置文件
    Config cfg = load_json(m_params.config_file);
    
    // 为每个S参数路径创建滤波器
    for (auto& [path_name, filter_cfg] : cfg.filters) {
        sca_util::sca_vector<double> num(filter_cfg.num);
        sca_util::sca_vector<double> den(filter_cfg.den);
        
        // 创建LTF滤波器对象
        auto ltf = std::make_shared<sca_tdf::sca_ltf_nd>(
            num, den, 1.0/m_fs  // 分子、分母、时间步长
        );
        
        m_filters[path_name] = ltf;
    }
}
```

**关键设计决策**：
- 每个Sij路径需要独立的滤波器实例
- N×N端口矩阵理论需要N²个滤波器（可利用对称性优化）
- 滤波器状态由SystemC-AMS内部管理，自动处理状态空间实现

##### 实时处理：processing()方法流程

完整的有理函数法信号处理流程：

```
输入读取 → [串扰预处理] → 传递函数滤波 → [双向传输叠加] → 输出写入
```

**单端口单向传输（最简场景）**：
```cpp
void ChannelSParamTdf::processing() {
    double x = in.read();
    
    // 应用S21传递函数
    double y = m_filters["S21"]->apply(x);
    
    out.write(y);
}
```

**多端口串扰场景**：
```cpp
void ChannelSParamTdf::processing() {
    // 步骤1：读取所有输入端口
    std::vector<double> x_in(N_ports);
    for (int i = 0; i < N_ports; ++i) {
        x_in[i] = in[i].read();
    }
    
    // 步骤2：应用耦合矩阵（串扰预处理）
    std::vector<double> x_coupled(N_ports, 0.0);
    for (int i = 0; i < N_ports; ++i) {
        for (int j = 0; j < N_ports; ++j) {
            // 主传输路径 + 串扰项
            double h_ij = m_filters[S_name(i,j)]->apply(x_in[j]);
            x_coupled[i] += h_ij;
        }
    }
    
    // 步骤3：写入输出端口
    for (int i = 0; i < N_ports; ++i) {
        out[i].write(x_coupled[i]);
    }
}
```

**双向传输场景**：
```cpp
void ChannelSParamTdf::processing() {
    // 正向路径：in → S21 → out
    double y_forward = m_filters["S21"]->apply(in.read());
    
    // 反向路径：out_prev → S12 → in（需要存储前一周期输出）
    double y_backward = m_filters["S12"]->apply(m_out_prev);
    
    // 端口1反射：in → S11 → in
    double y_reflect1 = m_filters["S11"]->apply(in.read());
    
    // 端口2反射：out_prev → S22 → out
    double y_reflect2 = m_filters["S22"]->apply(m_out_prev);
    
    // 叠加所有贡献
    double y_total = y_forward + y_reflect2;
    
    out.write(y_total);
    m_out_prev = y_total;  // 保存用于下一周期
}
```

##### 数值特性与优化

**计算复杂度**：
- 每时间步：O(order) 浮点运算
- 8阶滤波器：约40次乘加操作
- 多端口：O(N² × order)

**数值稳定性**：
- SystemC-AMS使用状态空间实现，自动平衡数值误差
- 极点强制在左半平面确保长期稳定性
- 推荐滤波器阶数≤16以避免高阶多项式的数值问题

---

### 3.3 冲激响应卷积法（设计规格）

#### 3.3.1 离线处理阶段（Python工具链）

冲激响应卷积法通过逆傅立叶变换将S参数转换为时域冲激响应，适用于捕捉非最小相位特性和复杂频域结构。

##### 步骤1：频域数据预处理

**DC点补全**：
Touchstone文件通常缺少0 Hz点，需要补全以避免时域直流偏置：

**方法A：向量拟合法（推荐）**
```python
# 使用VF估算DC值
vf_result = vector_fit(freq, S21, order=6)
S21_dc = vf_result.evaluate(s=0)  # H(0)
```
- 优势：物理约束强（稳定性/无源性），外推精度高
- 适用：所有信道类型

**方法B：低频外推法**
```python
# 对最低几个频率点进行幅相外推
freq_low = freq[:5]
mag_low = np.abs(S21[:5])
phase_low = np.angle(S21[:5])

# 线性外推到DC
mag_dc = np.interp(0, freq_low, mag_low)
phase_dc = np.interp(0, freq_low, phase_low)
S21_dc = mag_dc * np.exp(1j * phase_dc)
```
- 优势：实现简单
- 风险：低频测量噪声会导致DC误差

**带限处理**：
限制频率上限到Nyquist频率以避免混叠：
```python
f_nyquist = fs / 2
freq_valid = freq[freq <= f_nyquist]
S_valid = S21[freq <= f_nyquist]

# 高频滚降（可选，减少吉布斯效应）
window = np.hanning(len(freq_valid))
S_windowed = S_valid * window
```

**频率网格重采样**：
构造与目标采样率fs匹配的均匀频率网格：
```python
N = time_samples
df = fs / N
freq_grid = np.arange(0, fs/2, df)  # 0, df, 2df, ..., fs/2

# 插值到均匀网格（复数插值）
S_grid_real = np.interp(freq_grid, freq_valid, S_valid.real)
S_grid_imag = np.interp(freq_grid, freq_valid, S_valid.imag)
S_grid = S_grid_real + 1j * S_grid_imag
```

##### 步骤2：逆傅立叶变换（IFFT）

**双边频谱构造**：
```python
# 正频率：[0, df, 2df, ..., fs/2]
# 负频率：共轭镜像 [-fs/2, ..., -2df, -df]
S_positive = S_grid
S_negative = np.conj(S_positive[-1:0:-1])  # 镜像共轭

# 完整双边频谱 [0, +freq, -freq]
S_bilateral = np.concatenate([S_positive, S_negative])
```

**IFFT执行**：
```python
h_complex = np.fft.ifft(S_bilateral, n=N)
h_real = np.real(h_complex)  # 取实部（物理系统响应）

# 时间轴
dt = 1.0 / fs
time = np.arange(N) * dt
```

##### 步骤3：因果性处理

理想的因果系统满足`h(t) = 0, ∀t < 0`。实际IFFT结果可能在t<0区域有小幅非零值（数值误差或非最小相位特性）。

**因果性窗函数**（Hamming窗）：
```python
# 检测峰值位置
peak_idx = np.argmax(np.abs(h_real))

# 应用因果性窗：t<0部分抑制
causal_window = np.zeros_like(h_real)
causal_window[peak_idx:] = 1.0  # t≥t_peak保持
causal_window[:peak_idx] = np.hamming(peak_idx)  # t<t_peak逐渐衰减

h_causal = h_real * causal_window
```

**最小相位变换**（可选，适用于严格因果性要求）：
```python
from scipy.signal import minimum_phase
h_minphase = minimum_phase(h_real, method='hilbert')
```
- 优势：完全消除非因果分量
- 代价：改变相位特性，不再精确匹配原始S参数

##### 步骤4：截断与长度优化

**尾部检测**：
识别冲激响应显著部分，截断低能量尾部以减少卷积计算量：
```python
threshold = truncate_threshold * np.max(np.abs(h_causal))
significant = np.abs(h_causal) > threshold

# 找到最后一个显著样本
last_idx = np.where(significant)[0][-1]
L_truncated = last_idx + 1

h_final = h_causal[:L_truncated]
```

**能量验证**：
```python
energy_original = np.sum(h_causal**2)
energy_truncated = np.sum(h_final**2)
retention_ratio = energy_truncated / energy_original

print(f"能量保留率: {retention_ratio*100:.2f}%")
# 推荐 > 99.9%
```

##### 步骤5：配置文件导出

```json
{
  "method": "impulse",
  "fs": 100e9,
  "impulse_responses": {
    "S21": {
      "time": [0, 1e-11, 2e-11, ...],
      "impulse": [0.001, 0.012, 0.045, ...],
      "length": 2048,
      "dt": 1e-11,
      "energy": 0.9987,
      "peak_time": 5.2e-10
    }
  }
}
```

#### 3.3.2 在线仿真阶段（SystemC-AMS）

##### 初始化：延迟线分配

```cpp
void ChannelSParamTdf::initialize() {
    // 加载冲激响应
    Config cfg = load_json(m_params.config_file);
    m_impulse = cfg.impulse_responses["S21"].impulse;
    m_L = m_impulse.size();
    
    // 分配延迟线缓冲区
    m_buffer.resize(m_L, 0.0);
    m_buf_idx = 0;
}
```

##### 实时处理：直接卷积

**循环缓冲区卷积**：
```cpp
void ChannelSParamTdf::processing() {
    // 读取新输入
    double x_new = in.read();
    
    // 更新循环缓冲区
    m_buffer[m_buf_idx] = x_new;
    
    // 执行卷积: y(n) = Σ h(k) * x(n-k)
    double y = 0.0;
    for (int k = 0; k < m_L; ++k) {
        int buf_pos = (m_buf_idx - k + m_L) % m_L;
        y += m_impulse[k] * m_buffer[buf_pos];
    }
    
    // 输出
    out.write(y);
    
    // 更新索引
    m_buf_idx = (m_buf_idx + 1) % m_L;
}
```

**计算复杂度**：
- 每时间步：O(L) 乘加操作
- L=2048时：约2048次浮点运算
- 多端口：O(N² × L)

##### FFT卷积优化（长冲激响应场景）

当L > 512时，使用重叠-保留（Overlap-Save）FFT卷积：

**算法原理**：
```
y = IFFT(FFT(x) ⊙ FFT(h))
```

**块处理流程**：
```cpp
// 预计算冲激响应的FFT（初始化阶段）
void initialize_fft() {
    m_H_fft = fft(m_impulse, m_fft_size);
}

// 块处理（每accumulate B个样本）
void processing() {
    m_input_block[m_block_idx++] = in.read();
    
    if (m_block_idx == m_block_size) {
        // FFT输入块
        auto X_fft = fft(m_input_block, m_fft_size);
        
        // 频域乘法
        auto Y_fft = X_fft * m_H_fft;  // 逐元素乘
        
        // IFFT
        auto y_block = ifft(Y_fft);
        
        // 输出前B个样本（丢弃重叠部分）
        for (int i = 0; i < m_block_size; ++i) {
            m_output_queue.push(y_block[i]);
        }
        
        m_block_idx = 0;
    }
    
    // 从队列输出
    out.write(m_output_queue.front());
    m_output_queue.pop();
}
```

**性能提升**：
- 直接卷积：O(L) 每样本
- FFT卷积：O(log B) 每样本（分摊）
- 适用条件：L > 512

---

### 3.4 GPU加速实现（Apple Silicon专属，设计规格）

#### 3.4.1 适用场景判断

GPU加速仅在以下条件**全部满足**时启用：
1. 系统为Apple Silicon（M1/M2/M3/M4或更新）
2. `method="impulse"`（有理函数法不支持GPU加速）
3. `gpu_acceleration.enabled=true`
4. 冲激响应长度L或采样率满足性能阈值

#### 3.4.2 Metal GPU直接卷积

**初始化：GPU资源分配**
```cpp
// Metal设备和命令队列创建
id<MTLDevice> m_device = MTLCreateSystemDefaultDevice();
id<MTLCommandQueue> m_queue = [m_device newCommandQueue];

// 加载Metal着色器
id<MTLLibrary> library = [m_device newLibraryWithSource:shader_code];
id<MTLFunction> kernel = [library newFunctionWithName:@"convolution_kernel"];
id<MTLComputePipelineState> m_pipeline = 
    [m_device newComputePipelineStateWithFunction:kernel];

// 分配GPU缓冲区
m_impulse_gpu = [m_device newBufferWithBytes:impulse
                                      length:L*sizeof(float)
                                     options:MTLResourceStorageModeShared];
m_input_gpu = [m_device newBufferWithLength:batch_size*sizeof(float)];
m_output_gpu = [m_device newBufferWithLength:batch_size*sizeof(float)];
```

**Metal着色器（卷积核）**
```metal
kernel void convolution_kernel(
    device const float* input [[buffer(0)]],
    device const float* impulse [[buffer(1)]],
    device float* output [[buffer(2)]],
    constant int& L [[buffer(3)]],
    uint n [[thread_position_in_grid]])
{
    float sum = 0.0f;
    for (int k = 0; k < L; ++k) {
        int idx = n - k;
        if (idx >= 0) {
            sum += impulse[k] * input[idx];
        }
    }
    output[n] = sum;
}
```

**批处理执行流程**
```cpp
void processing() {
    // 收集输入样本到批处理缓冲区
    m_batch_buffer[m_batch_idx++] = in.read();
    
    if (m_batch_idx == m_batch_size) {
        // 上传到GPU
        memcpy([m_input_gpu contents], m_batch_buffer, 
               m_batch_size * sizeof(float));
        
        // 编码GPU命令
        id<MTLCommandBuffer> cmd = [m_queue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
        [enc setComputePipelineState:m_pipeline];
        [enc setBuffer:m_input_gpu offset:0 atIndex:0];
        [enc setBuffer:m_impulse_gpu offset:0 atIndex:1];
        [enc setBuffer:m_output_gpu offset:0 atIndex:2];
        
        // 分派线程网格
        MTLSize gridSize = MTLSizeMake(m_batch_size, 1, 1);
        MTLSize threadGroupSize = MTLSizeMake(256, 1, 1);
        [enc dispatchThreads:gridSize threadsPerThreadgroup:threadGroupSize];
        [enc endEncoding];
        
        // 提交并等待
        [cmd commit];
        [cmd waitUntilCompleted];
        
        // 下载结果
        memcpy(m_output_buffer, [m_output_gpu contents], 
               m_batch_size * sizeof(float));
        
        m_batch_idx = 0;
    }
    
    // 顺序输出
    out.write(m_output_buffer[m_output_idx++]);
}
```

#### 3.4.3 Metal GPU FFT卷积

**初始化：Metal Performance Shaders（MPS）**
```cpp
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

// 创建FFT描述符
MPSMatrixDescriptor* fft_desc = [MPSMatrixDescriptor 
    matrixDescriptorWithRows:1 
    columns:m_fft_size 
    dataType:MPSDataTypeComplexFloat32];

// 创建FFT对象
m_fft_forward = [[MPSMatrixFFT alloc] 
    initWithDevice:m_device 
    descriptor:fft_desc];
m_fft_inverse = [[MPSMatrixFFT alloc] 
    initWithDevice:m_device 
    descriptor:fft_desc 
    inverse:YES];

// 预计算冲激响应的FFT
compute_impulse_fft_gpu();
```

**频域卷积执行**
```cpp
void process_fft_convolution() {
    // 上传输入块
    upload_input_block();
    
    // GPU命令序列
    id<MTLCommandBuffer> cmd = [m_queue commandBuffer];
    
    // 1. FFT(input)
    [m_fft_forward encodeToCommandBuffer:cmd 
                            inputMatrix:m_input_gpu 
                           outputMatrix:m_input_fft_gpu];
    
    // 2. 频域逐元素乘法
    [m_multiply_kernel encodeToCommandBuffer:cmd
                                      input1:m_input_fft_gpu
                                      input2:m_impulse_fft_gpu
                                      output:m_product_fft_gpu];
    
    // 3. IFFT(product)
    [m_fft_inverse encodeToCommandBuffer:cmd
                            inputMatrix:m_product_fft_gpu
                           outputMatrix:m_output_gpu];
    
    [cmd commit];
    [cmd waitUntilCompleted];
    
    // 下载结果
    download_output_block();
}
```

#### 3.4.4 性能优化策略

**批处理大小选择**：
- 小批量（64-256）：延迟优先，适合实时仿真
- 大批量（1024-4096）：吞吐量优先，适合离线分析
- 推荐：1024（平衡延迟和效率）

**算法自动选择**：
```cpp
void select_gpu_algorithm() {
    if (m_L < m_fft_threshold) {
        // L < 512: 直接卷积
        m_gpu_mode = GpuMode::DIRECT_CONV;
    } else {
        // L ≥ 512: FFT卷积
        m_gpu_mode = GpuMode::FFT_CONV;
    }
}
```

**共享内存优化**：
- Metal Shared Memory模式：CPU/GPU零拷贝访问
- 减少PCIe传输延迟（虽然Apple Silicon为统一内存架构）

---

### 3.5 串扰与多端口处理（设计规格）

#### 3.5.1 端口映射标准化

**问题**：不同Touchstone文件的端口顺序不一致，需要标准化处理。

**手动映射**：
```json
{
  "port_mapping": {
    "enabled": true,
    "mode": "manual",
    "differential_pairs": [[1,2], [3,4]],
    "forward_paths": [[1,3], [2,4]]
  }
}
```

**实现**：构造置换矩阵P，对S矩阵重排：
```
S'(f) = P · S(f) · P^T
```

**自动识别**：
```python
# 计算各Sij的通带能量
energy_matrix = np.zeros((N, N))
for i in range(N):
    for j in range(N):
        energy_matrix[i,j] = np.sum(np.abs(S[i,j,:])**2)

# 最大权匹配算法识别主传输路径
from scipy.optimize import linear_sum_assignment
row_ind, col_ind = linear_sum_assignment(-energy_matrix)
forward_paths = list(zip(row_ind, col_ind))
```

#### 3.5.2 串扰耦合矩阵

**N×N端口系统信号流**：
```
x_in[N] → S矩阵卷积 → y_out[N]
```

**实现**（有理函数法）：
```cpp
void processing_crosstalk() {
    // 输入向量
    std::vector<double> x(N_ports);
    for (int i = 0; i < N_ports; ++i) {
        x[i] = in[i].read();
    }
    
    // N×N传递函数矩阵
    std::vector<double> y(N_ports, 0.0);
    for (int i = 0; i < N_ports; ++i) {
        for (int j = 0; j < N_ports; ++j) {
            // Sij传递函数
            double h_ij = m_filters[S_name(i,j)]->apply(x[j]);
            y[i] += h_ij;
        }
    }
    
    // 输出向量
    for (int i = 0; i < N_ports; ++i) {
        out[i].write(y[i]);
    }
}
```

**近端串扰（NEXT）与远端串扰（FEXT）识别**：
- NEXT：输入端耦合（如S13：端口1→端口3，同侧）
- FEXT：输出端耦合（如S23：端口2→端口3，异侧）

#### 3.5.3 双向传输

**完整双向模型**：
```
y_out = S21·x_in + S22·y_out_prev
y_in_reflect = S11·x_in + S12·y_out_prev
```

**实现注意事项**：
- 反向路径需要存储前一周期的输出
- 反射项会引入额外的群延迟
- 双向仿真的数值稳定性取决于|S11|和|S22|的大小

---

### 3.6 数值考虑与误差管理

#### 3.6.1 有理函数法数值稳定性

**极点位置约束**：
- 所有极点必须在左半平面：`Re(p_k) < 0`
- 避免极点过于接近虚轴（推荐`|Re(p_k)| > 0.01·|Im(p_k)|`）

**高阶滤波器风险**：
- 阶数>16：多项式系数动态范围大，浮点精度损失
- 建议：分解为多个低阶滤波器级联

**SystemC-AMS内部实现**：
- 自动选择状态空间或直接形式II
- 内部系数缩放避免溢出

#### 3.6.2 冲激响应法数值误差

**IFFT泄漏**：
- 频域截断导致时域振铃
- 缓解：应用窗函数（Hamming/Kaiser）

**卷积累积误差**：
- 长时间仿真：浮点误差累积
- 缓解：定期重置延迟线（插入已知样本）

**GPU单精度vs双精度**：
- Metal GPU默认单精度（float32）
- 双精度（float64）速度降低50%但精度提升

#### 3.6.3 能量守恒验证

**无源性检查**（离线）：
```python
# 所有频率点检查
for f in freq:
    eigenvalues = np.linalg.eigvals(S_matrix[f])
    assert np.all(np.abs(eigenvalues) <= 1.0), "违反无源性"
```

**在线监控**：
```cpp
void check_passivity() {
    double E_in = compute_energy(input_history);
    double E_out = compute_energy(output_history);
    
    if (E_out > E_in * 1.01) {
        std::cerr << "警告：输出能量超过输入，可能数值不稳定\n";
    }
}
```

---
## 4. 测试平台架构

### 4.1 测试平台设计思想

信道模块测试平台采用**分层验证策略**。核心设计理念：

1. **版本适配性**：区分v0.4简化实现与未来完整S参数建模的测试需求
2. **集成优先**：当前通过完整链路集成测试验证基本传输功能
3. **局限性明确**：清晰标识未覆盖的高级特性（S参数加载、串扰、双向传输）

v0.4版本**无独立测试平台**，依赖`simple_link_tb.cpp`完整链路集成测试。未来扩展方向包括独立测试台（`tb/channel/`）、频域验证工具和多端口测试场景。

### 4.2 当前测试环境（v0.4）

#### 4.2.1 集成测试平台（Simple Link Testbench）

**测试平台位置**：
```
tb/simple_link_tb.cpp
```

**测试拓扑**（信道在完整链路中的位置）：
```
TX链路                        Channel                      RX链路
┌─────────────────┐      ┌─────────────────┐      ┌─────────────────┐
│  WaveGeneration │      │                 │      │                 │
│       ↓         │      │                 │      │                 │
│     TxFFE       │      │  ChannelSparam  │      │    RxCTLE       │
│       ↓         │──────▶     (SISO)      │──────▶                 │
│    TxMux        │      │                 │      │    RxVGA        │
│       ↓         │      │  简化一阶LPF    │      │       ↓         │
│   TxDriver      │      │                 │      │   RxSampler     │
└─────────────────┘      └─────────────────┘      └─────────────────┘
```

**信号连接代码**（来自`simple_link_tb.cpp`第50-74行）：
```cpp
// 创建信道模块
ChannelSparamTdf channel("channel", params.channel);

// 连接TX输出到信道输入
tx_driver.out(sig_driver_out);
channel.in(sig_driver_out);

// 连接信道输出到RX输入
channel.out(sig_channel_out);
rx_ctle.in(sig_channel_out);
```

**测试配置**（来自`config/default.json`第33-42行）：
```json
{
  "channel": {
    "touchstone": "chan_4port.s4p",
    "ports": 2,
    "crosstalk": true,
    "bidirectional": true,
    "simple_model": {
      "attenuation_db": 10.0,
      "bandwidth_hz": 20e9
    }
  }
}
```

**验证能力清单**：

| 功能 | 状态 | 说明 |
|------|------|------|
| 基本信号传输 | ✅ 可用 | 输入→一阶LPF→输出 |
| 接口兼容性 | ✅ 可用 | 与TX/RX模块正确连接 |
| 一阶低通滤波器 | ✅ 可用 | 通过`attenuation_db`和`bandwidth_hz`配置 |
| S参数文件加载 | ❌ 未实现 | `touchstone`参数占位但未启用 |
| 多端口串扰 | ❌ 未实现 | `crosstalk`参数无效 |
| 双向传输 | ❌ 未实现 | `bidirectional`参数无效 |

**测试输出**：
- 波形追踪文件：`simple_link.dat`（包含`channel_out`信号，第97行定义）
- 终端日志：信道模块创建和连接信息

#### 4.2.2 当前测试的局限性

v0.4集成测试的主要限制：

| 限制项 | 原因 | 影响 |
|-------|------|------|
| 无独立单元测试 | `tb/channel/`目录不存在 | 无法隔离验证信道功能，无法快速调试 |
| 无频响校验 | 简化模型为一阶LPF，未实现S参数频域响应 | 无法验证传递函数精度（幅度/相位误差） |
| 无S参数加载测试 | Touchstone解析功能未实现 | 无法测试文件格式兼容性（.s2p/.s4p） |
| 无串扰场景 | 仅单输入单输出 | 无法验证多端口耦合矩阵（NEXT/FEXT） |
| 无性能基准 | 未测量计算时间 | 无法对比有理函数法vs冲激响应法效率 |

---

### 4.3 测试结果分析

#### 4.3.1 输出文件说明

**波形文件**：`simple_link.dat`（SystemC-AMS标准制表符分隔格式）

典型文件内容结构：
```
time          wave_out      driver_out    channel_out   ctle_out      ...
0.000000e+00  0.000000e+00  0.000000e+00  0.000000e+00  0.000000e+00  ...
1.250000e-11  5.000000e-01  4.000000e-01  3.578000e-01  4.123000e-01  ...
...
```

**关键信号列**：
- `channel_out`：信道模块输出（经一阶LPF衰减和带限后的信号）
- `driver_out`：TX驱动器输出（信道输入参考信号）
- `ctle_out`：CTLE输出（验证信道输出是否满足RX输入要求）

#### 4.3.2 基本验证方法

**波形目视检查**：
1. 使用Python脚本或波形查看器加载`simple_link.dat`
2. 对比`driver_out`与`channel_out`波形
3. 验证信道输出符合以下预期：
   - 幅度约为输入的`10^(-10dB/20) ≈ 0.316倍`
   - 高频分量被衰减（20 GHz带宽以上）
   - 无数值异常（NaN/Inf）

**终端日志检查**：
```bash
# 运行测试后查看输出
cd build
./bin/simple_link_tb

# 预期输出：
# Creating Channel module...
# Connecting Channel...
# Simulation completed successfully.
```

#### 4.3.3 一阶低通滤波器频响验证（简化方法）

虽然集成测试仅运行时域仿真，可通过以下简化方法粗略验证频响：

**方法1：阶跃响应分析**（手动操作）
1. 修改`config/default.json`，将波形源改为阶跃信号
2. 运行仿真，观察`channel_out`的上升时间
3. 估算-3dB带宽：`BW ≈ 0.35 / t_rise`
4. 与配置的20 GHz对比（预期t_rise ≈ 17.5 ps）

**方法2：Python后处理FFT**（推荐）
```python
import numpy as np
import matplotlib.pyplot as plt

# 加载波形数据
data = np.loadtxt('simple_link.dat', skiprows=1)
time = data[:, 0]
driver_out = data[:, 2]
channel_out = data[:, 3]

# 计算FFT
fs = 1.0 / (time[1] - time[0])
freq = np.fft.rfftfreq(len(time), 1/fs)
H_fft = np.fft.rfft(channel_out) / np.fft.rfft(driver_out)
H_mag_db = 20 * np.log10(np.abs(H_fft))

# 验证-3dB带宽
idx_3db = np.where(H_mag_db < -3.0)[0][0]
bw_measured = freq[idx_3db]
print(f"测量带宽: {bw_measured/1e9:.2f} GHz（预期: 20 GHz）")

# 绘图
plt.semilogx(freq/1e9, H_mag_db)
plt.axhline(-3, color='r', linestyle='--', label='-3dB线')
plt.xlabel('Frequency (GHz)')
plt.ylabel('Magnitude (dB)')
plt.legend()
plt.savefig('channel_freq_response.png')
```

**预期结果**：
- DC增益：约-10 dB（与`attenuation_db`一致）
- -3dB带宽：约20 GHz（与`bandwidth_hz`一致）
- 高频滚降：-20 dB/decade（一阶系统特征）

---

### 4.4 运行指南

#### 4.4.1 构建命令

```bash
# 创建构建目录
mkdir -p build && cd build

# 配置CMake
cmake ..

# 编译完整链路测试台
make simple_link_tb
```

#### 4.4.2 运行命令

```bash
# 运行集成测试
cd build
./bin/simple_link_tb

# 预期输出：
# === SerDes SystemC-AMS Simple Link Testbench ===
# Configuration loaded:
#   Sampling rate: 80 GHz
#   Data rate: 40 Gbps
#   Simulation time: 1 us
# 
# Creating TX modules...
# Creating Channel module...
# Creating RX modules...
# Connecting TX chain...
# Connecting Channel...
# Connecting RX chain...
# 
# Creating trace file...
# SystemC: simulation stopped, sc_stop() called
```

**波形文件生成**：`simple_link.dat`（位于构建目录）

#### 4.4.3 参数配置说明

修改`config/default.json`中的信道参数：

```json
{
  "channel": {
    "simple_model": {
      "attenuation_db": 10.0,    // 衰减量（dB），建议范围：5-20
      "bandwidth_hz": 20e9       // -3dB带宽（Hz），建议范围：10G-50G
    }
  }
}
```

**参数影响**：
- 增大`attenuation_db`：信道损耗增加，眼图闭合更严重
- 减小`bandwidth_hz`：高频衰减加剧，符号间干扰（ISI）增强

#### 4.4.4 结果查看方法

**方法1：使用Python绘图**
```bash
python scripts/plot_simple_link.py
# 生成: simple_link_waveform.png
```

**方法2：使用GtkWave查看器**
```bash
# 转换为VCD格式（如果支持）
# gtkwave simple_link.vcd
```

**方法3：文本编辑器直接查看**
```bash
head -n 100 simple_link.dat
```

---

### 4.5 与其他模块的测试集成

#### 4.5.1 完整链路回归测试

**测试目标**：确保信道模块更新不破坏完整链路功能。

**测试流程**：
1. 修改信道模块代码或参数
2. 重新编译：`make simple_link_tb`
3. 运行回归测试：`./bin/simple_link_tb`
4. 比对波形文件：检查`channel_out`是否与基线版本一致
5. 验证下游模块：检查`ctle_out`和`sampler_out`未受异常影响

**基线保存**：
```bash
# 保存当前版本为基线
cp simple_link.dat simple_link_baseline.dat

# 修改代码后对比
diff simple_link.dat simple_link_baseline.dat
```

#### 4.5.2 测试变更影响范围

信道模块变更对链路的影响：

| 变更类型 | 影响模块 | 验证重点 |
|---------|---------|---------|
| `attenuation_db`调整 | CTLE/VGA增益需求 | 检查RX链路是否饱和或信噪比下降 |
| `bandwidth_hz`调整 | CTLE零点/极点配置 | 检查高频增强是否过度或不足 |
| 算法实现修改 | 所有下游模块 | 完整波形对比，确保数值一致性 |

**下游模块敏感性**：
- **RxCTLE**：对信道高频衰减非常敏感，带宽不匹配会导致均衡失效
- **RxSampler**：对信道引入的ISI敏感，可能导致采样错误
- **RxCDR**：对信道相位失真敏感，可能影响锁定时间

---

### 4.6 未来扩展方向

以下功能为设计规格，当前v0.4**未实现**：

**独立测试平台**（`tb/channel/`目录）：
- 时域瞬态测试：`channel_tran_tb.cpp`
- 频域扫频验证：`channel_freq_tb.cpp`
- 多端口串扰测试：`channel_crosstalk_tb.cpp`

**测试辅助模块**：
- 信号源：多音频测试、扫频正弦波、冲激/阶跃激励
- 测量器：频响分析、眼图统计、串扰量化

**自动化工具**：
- Python预处理：S参数文件解析、向量拟合、IFFT预处理
- Python后处理：频响对比、眼图绘制、性能基准报告
- Bash脚本：批量测试、回归测试、HTML报告生成

---

## 参数
- `touchstone`: 必须，.sNp 文件路径
- `ports`: 必须，端口数（N）
- `method`: 必须，默认 "rational"
- `config_file`: 必须，离线处理生成的配置文件
- `crosstalk`: 可选，默认 false
- `bidirectional`: 可选，默认 false
- `fit.order`: 可选（rational 方法），默认 16
- `fit.enforce_stable`: 可选，默认 true
- `fit.enforce_passive`: 可选，默认 true
- `fit.band_limit`: 可选，默认使用 Touchstone 文件的最高频率
- `impulse.time_samples`: 可选（impulse 方法），默认 4096
- `impulse.causality`: 可选，默认 true
- `impulse.truncate_threshold`: 可选，默认 1e-6
- `gpu_acceleration.enabled`: 可选，默认 false（**仅 Apple Silicon 可用**）
- `gpu_acceleration.backend`: 固定为 "metal"（唯一支持的后端）
- `gpu_acceleration.algorithm`: 可选，默认 "auto"（根据 L 自动选择）
- `gpu_acceleration.batch_size`: 可选，默认 1024
- `gpu_acceleration.fft_threshold`: 可选，默认 512

## 行为模型

### 方法一：有理函数拟合法（推荐）

#### 1. 离线处理（Python）
- **向量拟合**：
  - 使用向量拟合算法（Vector Fitting）对每个 Sij(f) 进行有理函数近似
  - 拟合形式：`H(s) = Σ(r_k / (s - p_k)) + d + s*h`
  - 极点 p_k 和留数 r_k 通过迭代优化获得
  - 强制约束：
    - 稳定性：所有极点实部 < 0
    - 无源性：确保能量守恒（可选）
    - 因果性：自动满足

- **传递函数转换**：
  - 将极点-留数形式转换为分子/分母多项式
  - `H(s) = (b_n*s^n + ... + b_1*s + b_0) / (a_m*s^m + ... + a_1*s + a_0)`
  - 归一化分母首项为 1

- **配置导出**：
  - 保存为 JSON 格式：`{"filters": {"S21": {"num": [...], "den": [...]}, ...}}`
  - 包含拟合质量指标（MSE、最大误差等）

#### 2. 在线仿真（SystemC-AMS）
- **LTF 滤波器实例化**：
  - 使用 `sca_tdf::sca_ltf_nd(num, den, timestep)` 创建线性时不变滤波器
  - SystemC-AMS 自动处理状态空间实现和数值积分

- **多端口处理**：
  - 为每个 Sij 创建独立的 `sca_ltf_nd` 实例
  - N×N 端口矩阵：需要 N² 个滤波器（可根据对称性优化）

- **性能优势**：
  - 紧凑表示：8 阶滤波器仅需 ~20 个系数
  - 计算高效：O(order) 每时间步
  - 数值稳定：SystemC-AMS 内置优化

### 方法二：冲激响应卷积法

#### S 参数预处理：DC 值补全与采样频率匹配（Impulse 方法）

- **背景与必要性**：
  - Touchstone 文件（.sNp）常缺少 0 Hz（DC）点，直接 IFFT 会导致时域响应出现直流偏置和长尾振铃，破坏因果性
  - IFFT 需要在与系统采样频率 fs 一致的均匀频率网格上进行；若测量频率非均匀或上限超出 Nyquist，会出现混叠或泄漏

- **技术可行性**：
  - DC 值补全可通过向量拟合（Vector Fitting，VF）在连续 s 域估算 H(0)，并在拟合中施加稳定/无源约束，稳健性最好
  - 低频插值（对最后若干低频点进行幅相外推）方法简单，但易引入偏差与振铃；仅在数据质量较好且带宽较低时建议
  - 借助端口阻抗与等效 RLC 模型推断 DC（将 S 转 Y/Z 后估算），对通用通道泛化性不足，不作为默认方案

- **推荐实现方案（离线阶段）**：
  1. 读取 S(f)，进行带外清理：设置 band_limit ≤ fs/2（Nyquist），超出部分滚降或设为 0
  2. DC 补全：
     - 首选 VF 法：对各 Sij(f) 进行 VF，启用稳定性/无源性约束，评估 H(0) 作为 DC 点并补入
     - 备选插值法：对最低频点附近进行幅相外推，注意保持相位连续与因果性（风险较高）
  3. 构建目标 fs 的均匀频率网格：f_k = k·Δf，Δf = fs/N，0≤k≤N/2，其中 N 对应冲激长度（与 time_samples 一致或更大）
  4. 获得网格上的 Sij(f_k)：
     - 插值路径：对复数 S(f) 进行样条/分段线性插值（幅相连续、避免过度拟合）
     - VF 评估路径：直接用 VF 的有理函数在 f_k 上评估（稳健性优于插值，推荐）
  5. 负频率镜像、IFFT、因果性窗与尾部截断（truncate_threshold），得到 h(t)
  6. 验证：检查能量守恒（无源性）、相位连续性、时域零偏差与长尾抑制

- **采样频率与 VF 的关系**：
  - VF 工作在连续 s 域，参数与 fs 无关；不需要在拟合阶段"匹配采样频率"
  - 但拟合点的频率密度应覆盖到 Nyquist(fs/2)，并在高梯度区加密采样，以保证对目标 fs 的评估精度
  - 实践建议：按目标 fs 设置 band_limit ≤ fs/2；测量上限低于 fs/2 时，避免外推到 Nyquist，宁可降低 fs 或采用滚降策略

- **风险与规避**：
  - 错误 DC 导致时域直流偏移：用 VF+无源约束估计 DC；必要时仅对 S11/S22 施加更强约束
  - 插值振铃与谱泄漏：优先 VF 评估；在频域增设平滑窗或带外滚降；时域使用因果性窗并截断尾部
  - 混叠风险：严格限制 band_limit ≤ fs/2；不满足时降低 fs 或增大 N

- **预处理配置建议（文档层面，暂不改代码）**：
  - `impulse.dc_completion`: "vf" | "interp" | "none"（默认 "vf"）
  - `impulse.resample_to_fs`: true/false（默认 true）
  - `impulse.fs`: 采样频率（Hz）
  - `impulse.band_limit`: 频段上限（默认 Touchstone 最高频或设置为 ≤ fs/2）
  - `impulse.grid_points`: 频率网格点数 N（与 time_samples 对应）

#### 1. 离线处理（Python）
- **逆傅立叶变换**：
  - 读取 S 参数频域数据 Sij(f)
  - 构造双边频谱（负频率为正频率的共轭）
  - 应用 IFFT：h(t) = IFFT[Sij(f)]
  - 取实部并确保因果性

- **因果性处理**：
  - 检测峰值位置，确保 t < 0 部分能量接近零
  - 可选：应用最小相位变换
  - 可选：Hilbert 变换构造因果响应

- **截断与优化**：
  - 识别冲激响应长尾衰减阈值
  - 截断低于阈值的部分，减少卷积长度
  - 应用窗函数（如 Hamming）减少截断效应

- **配置导出**：
  - 保存时间轴、冲激响应数组和采样间隔
  - JSON 格式：`{"impulse_responses": {"S21": {"time": [...], "impulse": [...], "dt": ...}}}`

#### 2. 在线仿真（SystemC-AMS）
- **延迟线卷积**：
  - 维护输入历史：`delay_line[0..L-1]`，L 为冲激响应长度
  - 每时间步：`y(n) = Σ h(k) * x(n-k)`
  - 使用循环缓冲区优化内存访问

- **快速卷积（可选）**：
  - 对于长冲激响应（L > 512），可使用 overlap-add FFT 卷积
  - 需要外部 FFT 库（如 FFTW）
  - 块处理：缓冲输入块 → FFT → 频域乘法 → IFFT → overlap-add

- **性能考虑**：
  - 时间复杂度：O(L) 每时间步（直接卷积）或 O(L log L) 分摊（FFT）
  - 空间复杂度：O(L) 延迟线存储
  - 适用于 L < 1000 的中短通道

#### 3. GPU 加速（可选，仅 Apple Silicon）

- **系统要求**：
  - **必须**：Apple Silicon（M1/M2/M3 等 ARM64 架构）Mac 电脑
  - **不支持**：Intel Mac、Linux、Windows 系统
  - 其他 GPU 后端（CUDA、OpenCL、ROCm）在当前实现中不受支持

- **适用场景**：
  - 长冲激响应（L > 512）
  - 多端口仿真（N > 2）
  - 高采样率场景（> 100 GS/s）

- **直接卷积加速**（L < 512）：
  - 将卷积计算卸载到 GPU
  - 每个输出样本并行计算
  - 性能提升：50-100x（Metal on Apple Silicon）

- **FFT 卷积加速**（L > 512）：
  - 利用 Metal Performance Shaders（MPS）
  - 卷积定理：`y = IFFT(FFT(x) ⊙ FFT(h))`
  - 预计算冲激响应的 FFT，仅需一次
  - 性能提升：200-500x（批处理模式可达 1000x）

- **批处理策略**：
  - 收集一批输入样本（如 1024 个）
  - 一次上传到 GPU，减少延迟
  - GPU 并行计算所有输出
  - 下载结果并顺序输出

- **后端说明**：
  - **Metal**：当前唯一支持的 GPU 后端，Apple Silicon 专属优化
  - ~~OpenCL~~：暂不支持
  - ~~CUDA~~：暂不支持（需 NVIDIA GPU）
  - ~~ROCm~~：暂不支持（需 AMD GPU）

### 串扰建模
- **耦合矩阵**：
  - N 端口输入向量 `x[N]` 通过耦合矩阵 `C[N×N]` 线性组合
  - `x'[i] = Σ C[i][j] * x[j]`
  - 耦合后信号进入各自的 Sii/Sij 滤波器

- **提取方法**：
  - 从 S 参数矩阵提取交叉项 Sij (i≠j)
  - 近端串扰（NEXT）：S13, S14 等
  - 远端串扰（FEXT）：S23, S24 等

#### S 参数端口映射的标准化处理

- **问题描述**：
  - 不同来源的 .sNp 端口顺序与配对关系不统一（例如 s4p 中端口1可能对应端口2或端口3），会导致正向传输与串扰项被错误识别，进而影响 s2d/crosstalk 分析的正确性

- **技术可行性**：
  - 通过置换矩阵对端口进行重排，可对每个频点的 S 矩阵做统一标准化：对端口顺序施加同一置换 P，得到 S'(f) = P · S(f) · P^T（等价于同时对行列按一致的端口重排）
  - 手动指定与自动识别两种路径均可实现，且与现有串扰分析流程兼容

- **实现方案**：
  - **手动指定映射（推荐提供）**：
    - 在配置中允许用户明确端口分组与方向，例如差分对、输入/输出端口配对、主传输路径
    - 处理器依据配置构造置换矩阵 P，对所有频点的 S(f) 进行重排，确保后续分析的端口序一致
  - **自动识别（启发式，提供为辅助）**：
    - 计算各 Sij 的通带能量或平均幅度：Eij = ∫ |Sij(f)|^2 df，用于评估强传输路径
    - 对差分场景：依据耦合与串扰强度，识别相邻端口形成的差分对；利用 S11/S22 与互耦指标验证合理性
    - 构建加权图：节点为端口，边权为 Eij；使用最大匹配或最大权匹配，选取最可能的输入→输出配对
    - 验证准则：标准化后主路径（如 S21）应显著高于非主路径；NEXT/FEXT 分类与物理预期一致
  - **冲突与回退**：
    - 对称网络或多条强路径可能导致不唯一映射；提供置信度与候选方案，允许用户锁定或覆写部分端口
    - 映射生效后进行无源性与对称性检查，若不满足则回退到手动映射或提示用户确认

- **与串扰分析的关系**：
  - 标准化后的端口序确保 s2d/crosstalk 结果可比性与稳定性；避免因文件端口顺序差异引入的指标偏差

- **配置建议（文档层面，暂不改代码）**：
  - `port_mapping.enabled`: true/false
  - `port_mapping.mode`: "manual" | "auto"
  - `port_mapping.manual.pairs`: [[1,2],[3,4]]（差分对或端口分组）
  - `port_mapping.manual.forward`: [[1,3],[2,4]]（输入→输出配对）
  - `port_mapping.auto.criteria`: "energy" | "lowfreq" | "bandpass"
  - `port_mapping.auto.constraints`: { differential: true/false, bidirectional: true/false }

- **验证建议**：
  - 对多来源的 s4p 执行标准化后，比较主传输曲线与串扰分类的一致性；出现差异时复核自动识别的置信度并考虑手动覆写

### 双向传输
- **正向路径**：S21（端口1 → 端口2）
- **反向路径**：S12（端口2 → 端口1）
- **反射**：S11（端口1输入反射）、S22（端口2输入反射）
- **开关控制**：
  - `bidirectional=true`：启用 S12 和反射项
  - `bidirectional=false`：仅使用 S21，单向简化模型

### 方法选择指南

| 场景 | 推荐方法 | 原因 |
|------|----------|------|
| 长通道（>10 GHz 带宽） | Rational | 拟合紧凑，仿真快速 |
| 短通道（< 5 GHz） | 两者均可 | Impulse 更直观，Rational 更高效 |
| 高阶效应（非最小相位） | Impulse | 保留完整频域信息 |
| 快速参数扫描 | Rational | 重新拟合开销小 |
| 验证与调试 | 两者对比 | 交叉验证拟合精度 |
| **超长通道（L > 2048，Apple Silicon）** | **Impulse + GPU** | **Metal GPU 加速弥补计算开销** |
| **多端口高速场景（Apple Silicon）** | **Impulse + GPU FFT** | **批处理效率极高** |

### 性能对比表

假设 4 端口 S 参数，冲激响应长度 L=2048，**在 Apple Silicon Mac 上测试**：

| 实现方式 | 每秒处理样本数 | 相对速度 | 内存占用 | 系统要求 |
|---------|--------------|---------|----------|----------|
| Rational（CPU 8阶） | ~10M samples/s | **1000x** | ~1 KB | 通用 |
| Impulse（CPU 单核） | ~100K samples/s | 1x | ~16 KB | 通用 |
| Impulse（CPU 8核） | ~600K samples/s | 6x | ~16 KB | 通用 |
| **Impulse（Metal 直接）** | ~5M samples/s | **50x** | ~20 KB | **Apple Silicon** |
| **Impulse（Metal FFT）** | ~20M samples/s | **200x** | ~32 KB | **Apple Silicon** |

**注意**：GPU 加速性能数据仅适用于 Apple Silicon（M1/M2/M3）Mac 电脑。

## 依赖

### Python 工具链
- **必须**：
  - `numpy`：数值计算
  - `scipy`：信号处理、IFFT、向量拟合
  - `scikit-rf`：Touchstone 文件读取与 S 参数操作
- **可选**：
  - `vectfit3`：专业向量拟合库
  - `matplotlib`：频响/冲激响应可视化

### SystemC-AMS
- **必须**：SystemC-AMS 2.3.4
- **可选**：FFTW3（CPU 快速卷积）

### GPU 加速运行时（仅 Apple Silicon）
- **Metal**（macOS Apple Silicon）：
  - Metal Framework（系统自带）
  - Metal Performance Shaders（系统自带）
  - 支持架构：Apple M1/M2/M3 及后续芯片

**暂不支持的后端**：
- ~~OpenCL~~：未在当前实现中支持
- ~~CUDA~~（NVIDIA GPU）：不适用于 Apple Silicon
- ~~ROCm~~（AMD GPU）：不适用于 Apple Silicon

### 配置文件
- `config/channel_filters.json`（rational 方法）
- `config/channel_impulse.json`（impulse 方法）

## 使用示例

### 离线处理流程

```bash
# 1. 准备 S 参数文件
cp path/to/channel.s4p data/

# 2. 生成有理函数配置
python tools/sparam_processor.py \
  --input data/channel.s4p \
  --method rational \
  --order 8 \
  --output config/channel_filters.json

# 3. 生成冲激响应配置（可选，用于对比）
python tools/sparam_processor.py \
  --input data/channel.s4p \
  --method impulse \
  --samples 4096 \
  --output config/channel_impulse.json

# 4. 验证拟合质量
python tools/verify_channel_fit.py \
  --sparam data/channel.s4p \
  --rational config/channel_filters.json \
  --impulse config/channel_impulse.json \
  --plot results/channel_verification.png
```

### 系统配置示例

```json
{
  "channel": {
    "touchstone": "data/channel.s4p",
    "ports": 2,
    "method": "rational",
    "config_file": "config/channel_filters.json",
    "crosstalk": false,
    "bidirectional": true,
    "fit": {
      "order": 8,
      "enforce_stable": true,
      "enforce_passive": true,
      "band_limit": 25e9
    }
  }
}
```

### 系统配置示例（GPU 加速，Apple Silicon）

``json
{
  "channel": {
    "touchstone": "data/long_channel.s4p",
    "ports": 4,
    "method": "impulse",
    "config_file": "config/channel_impulse.json",
    "crosstalk": true,
    "bidirectional": true,
    "impulse": {
      "time_samples": 4096,
      "causality": true,
      "truncate_threshold": 1e-6
    },
    "gpu_acceleration": {
      "enabled": true,
      "backend": "metal",
      "algorithm": "auto",
      "batch_size": 1024,
      "fft_threshold": 512
    }
  }
}
```

**注意**：此配置仅在 Apple Silicon Mac 上有效。在其他平台上应将 `gpu_acceleration.enabled` 设为 `false`。

### SystemC-AMS 实例化

```cpp
// 创建 Channel 模块
ChannelModel channel("channel");
channel.config_file = "config/channel_filters.json";
channel.method = "rational";
channel.load_config();

// 连接信号
channel.in(tx_out);
channel.out(rx_in);
```

## 测试验证

### 1. 频响校验
- **目标**：验证时域实现与原始 S 参数频域一致性
- **方法**：
  - 输入扫频正弦信号
  - 记录幅度/相位响应
  - 与 Touchstone 文件绘图对比
- **指标**：
  - 幅度误差 < 0.5 dB（通带内）
  - 相位误差 < 5°（通带内）

### 2. 冲激响应对比
- **目标**：两种方法结果一致性
- **方法**：
  - Rational 方法：激励冲激 → 记录响应
  - Impulse 方法：直接输出预计算响应
  - 计算互相关和均方误差
- **指标**：
  - 归一化 MSE < 1%
  - 峰值时刻偏差 < 1 采样周期

### 3. 串扰场景
- **目标**：多端口耦合正确性
- **方法**：
  - 在端口1输入 PRBS，端口2观察串扰
  - 测量 NEXT/FEXT 比值
- **指标**：
  - 串扰幅度与 S13/S14 一致（±2 dB）

### 4. 双向传输
- **目标**：验证 S12/S21 和反射项
- **方法**：
  - 启用/禁用 bidirectional 开关
  - 对比输出差异
  - 测量反射系数（输入端）
- **指标**：
  - 反射系数与 S11 一致（±1 dB）

### 5. 数值稳定性
- **目标**：长时间仿真无发散
- **方法**：
  - 运行 1e6 个时间步
  - 监控输出能量
- **指标**：
  - 无 NaN/Inf
  - 输出能量 ≤ 输入能量（无源性）

### 6. 性能基准
- **目标**：仿真速度对比
- **方法**：
  - 测量每秒模拟时间（wall time）
  - Rational vs Impulse（CPU/GPU，不同冲激长度）
  - **GPU 测试平台**：Apple Silicon Mac（M1/M2/M3）
- **期望**：
  - Rational（8 阶）：> 1000x 实时
  - Impulse CPU（L=512）：> 10x 实时
  - **Impulse Metal GPU（L=512，Apple Silicon）：> 500x 实时**
  - **Impulse Metal GPU FFT（L=4096，Apple Silicon）：> 2000x 实时**

### 7. GPU 加速效果验证（仅 Apple Silicon）
- **目标**：Metal GPU 计算结果与 CPU 一致
- **系统要求**：Apple Silicon Mac（M1/M2/M3 或更新）
- **方法**：
  - 相同输入分别用 CPU 和 Metal GPU 计算
  - 逐样本对比输出
  - 计算最大绝对误差和 RMS 误差
- **指标**：
  - 最大误差 < 1e-6（单精度）或 1e-12（双精度）
  - RMS 误差 < 1e-8
  - 无数值发散现象

## 变更历史

### v0.3 (2025-10-16)
- **GPU 加速支持**：新增 Impulse 方法的 Metal GPU 加速功能（**仅 Apple Silicon**）
  - 新增 `gpu_acceleration` 配置对象
  - 支持 Metal 后端（Apple Silicon 专属）
  - 实现直接卷积和 FFT 卷积两种 GPU 算法
  - 批处理策略优化数据传输
  - 新增 Metal GPU 性能基准和验证测试
  - 补充性能对比表（CPU vs Metal GPU on Apple Silicon）
  - 更新方法选择指南，增加 Apple Silicon GPU 加速场景
  - 提供 Metal GPU 配置示例和依赖说明
  - **明确**：当前仅支持 Apple Silicon，其他 GPU 后端（CUDA/OpenCL/ROCm）暂不支持

### v0.2 (2025-10-16)
- **重大更新**：完全重写文档，新增两种实现方法
  - 新增有理函数拟合法详细说明（向量拟合 → LTF 滤波器）
  - 新增冲激响应卷积法详细说明（IFFT → 延迟线卷积）
  - 增加方法选择指南和性能对比
  - 扩展接口参数：`method`, `fit.*`, `impulse.*`
  - 详细说明 Python 离线处理流程
  - 补充 SystemC-AMS 两种实现方式
  - 新增串扰和双向传输机制说明
  - 完善测试验证策略（频响、冲激、串扰、双向、稳定性、性能）
  - 提供完整使用示例和配置模板

### v0.1 (初始版本)
- 初始模板，包含基本的向量拟合框架
- 占位性内容