# Adaption 模块技术文档

**级别**：DE 顶层模块  
**类名**：`AdaptionDe`  
**当前版本**：v0.1 (2025-10-30)  
**状态**：开发中

---

## 1. 概述

Adaption 是 SerDes 链路的自适应控制中枢，运行于 SystemC DE（Discrete Event）域，承载链路运行时自适应算法库。该模块通过 DE‑TDF 桥接机制对 AMS 域模块（CTLE、VGA、Sampler、DFE Summer、CDR）的参数进行在线更新与控制，提升链路在不同通道特征、数据速率与噪声条件下的稳态性能（眼图开口、抖动抑制、误码率）与动态响应（锁定时间、收敛速度）。

### 1.1 设计原理

Adaption 模块的核心设计思想是建立多层次、多速率的自适应控制架构，通过实时反馈优化链路参数：

- **分层控制策略**：将自适应算法分为快路径（CDR PI、阈值自适应，高更新频率）与慢路径（AGC、DFE 抽头更新，低更新频率），符合实际硬件的分层控制架构，平衡性能与计算开销

- **反馈驱动优化**：依据采样误差、幅度统计、相位误差等实时指标，采用经典控制理论（PI 控制器）与自适应滤波理论（LMS/Sign-LMS）动态调整增益、抽头、阈值与相位命令

- **跨域协同机制**：通过 SystemC-AMS 的 DE‑TDF 桥接机制实现 DE 域控制逻辑与 TDF 域模拟前端之间的参数传递，确保时序对齐与参数原子更新

- **鲁棒性设计**：提供饱和钳位、速率限制、泄漏、冻结/回退等安全机制，防止算法发散或参数异常，确保在极端场景（信号丢失、噪声暴涨、配置错误）下的系统稳定性

**控制环路架构**：
```
┌─────────────────────────────────────────────────────────────┐
│                    Adaption DE 控制层                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │  快路径      │  │  慢路径      │  │  安全监管    │      │
│  │  CDR PI      │  │  AGC/DFE     │  │  冻结/回退   │      │
│  │  阈值自适应  │  │  抽头更新    │  │  快照保存    │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                 │                 │              │
└─────────┼─────────────────┼─────────────────┼──────────────┘
          │                 │                 │
          ▼                 ▼                 ▼
┌─────────────────────────────────────────────────────────────┐
│                   DE‑TDF 桥接层                             │
│  phase_cmd, vga_gain, dfe_taps, sampler_threshold          │
└─────────────────────────────────────────────────────────────┘
          │                 │                 │
          ▼                 ▼                 ▼
┌─────────────────────────────────────────────────────────────┐
│                   AMS 模拟前端层                            │
│  ┌──────┐  ┌──────┐  ┌────────┐  ┌────────┐  ┌────────┐  │
│  │ CDR  │  │ VGA  │  │Sampler │  │  DFE   │  │  CTLE  │  │
│  └──┬───┘  └──┬───┘  └───┬────┘  └───┬────┘  └───┬────┘  │
└─────┼────────┼──────────┼──────────┼───────────┼────────┘
      │        │          │          │           │
      └────────┴──────────┴──────────┴───────────┘
                      反馈信号
  phase_error, amplitude_rms, error_count, isi_metric
```

### 1.2 核心特性

- **四大自适应算法**：集成 AGC（自动增益控制）、DFE 抽头更新（LMS/Sign-LMS/NLMS）、CDR PI 控制器、阈值自适应算法，覆盖链路关键参数优化

- **多速率调度架构**：支持事件驱动、周期驱动、多速率三种调度模式，快路径（每 10‑100 UI）与慢路径（每 1000‑10000 UI）并行运行，优化计算效率

- **DE‑TDF 桥接机制**：通过 `sca_de::sca_in/out` 与 TDF 模块的 `sca_tdf::sca_de::sca_in/out` 端口连接，实现跨域参数传递，确保时序对齐与数据同步

- **安全与回退机制**：提供冻结策略（误码暴涨/幅度异常/相位失锁时暂停更新）、回退策略（恢复至上次稳定快照）、快照保存（周期性记录参数历史），提升系统鲁棒性

- **参数约束与钳位**：所有输出参数支持范围限制（gain_min/max、tap_min/max、phase_range）、速率限制（增益变化率限制）、抗积分饱和（CDR PI 控制器），防止参数发散

- **配置驱动设计**：通过 JSON/YAML 配置文件管理所有算法参数，支持运行时场景切换与参数重载，便于不同通道与速率场景的快速验证

- **Trace 与诊断**：输出关键信号时间序列（vga_gain、dfe_taps、sampler_threshold、phase_cmd、update_count、freeze_flag），支持后处理分析与回归验证

### 1.3 版本历史

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| v0.1 | 2025-10-30 | 初始版本，建立模块框架与四大算法架构（AGC、DFE、阈值、CDR PI），定义多速率调度与冻结/回退机制，提供 JSON 配置示例与使用说明，制定测试验证计划 |

## 2. 模块接口

### 2.1 端口定义（DE域）

Adaption 模块运行于 SystemC DE（Discrete Event）域，通过 `sca_de::sca_in/out` 端口与 TDF 域模块进行跨域通信。

#### 输入端口（来自 RX/CDR/SystemConfiguration）

| 端口名 | 方向 | 类型 | 说明 |
|-------|------|------|------|
| `phase_error` | 输入 | double | 相位误差（CDR 用，单位：秒或归一化 UI），来自 CDR 相位检测器输出 |
| `amplitude_rms` | 输入 | double | 幅度 RMS 或峰值（AGC 用），来自 RX 幅度统计模块 |
| `error_count` | 输入 | int | 误码计数或误差累积（阈值自适应/DFE 用），来自 Sampler 判决误差统计 |
| `isi_metric` | 输入 | double | ISI 指标（可选，用于 DFE 更新策略），反映码间干扰程度 |
| `mode` | 输入 | int | 运行模式（0=初始化，1=训练，2=数据，3=冻结），来自系统配置控制器 |
| `reset` | 输入 | bool | 全局复位或参数重置信号，高电平有效 |
| `scenario_switch` | 输入 | double | 场景切换事件（可选），用于多场景热切换触发 |

> **重要**：所有输入端口必须连接，即使对应算法未启用（SystemC-AMS 要求所有端口均需连接）。

#### 输出端口（到 RX/CDR）

| 端口名 | 方向 | 类型 | 说明 |
|-------|------|------|------|
| `vga_gain` | 输出 | double | VGA 增益设定（线性倍数），通过 DE‑TDF 桥接写入 RX VGA 模块 |
| `ctle_zero` | 输出 | double | CTLE 零点频率（Hz，可选），支持在线调整 CTLE 频响特性 |
| `ctle_pole` | 输出 | double | CTLE 极点频率（Hz，可选），在线调整 CTLE 带宽 |
| `ctle_dc_gain` | 输出 | double | CTLE 直流增益（线性倍数，可选），在线调整 CTLE 低频增益 |
| `dfe_taps` | 输出 | vector&lt;double&gt; | DFE 抽头系数数组 [tap1, tap2, ..., tapN]，写入 DFE Summer 模块 |
| `sampler_threshold` | 输出 | double | 采样阈值（V），写入 Sampler 模块判决比较器 |
| `sampler_hysteresis` | 输出 | double | 迟滞窗口（V），写入 Sampler 模块抗噪迟滞 |
| `phase_cmd` | 输出 | double | 相位插值器命令（秒或归一化步长），写入 CDR PI 控制器 |
| `update_count` | 输出 | int | 更新次数计数器，用于诊断和性能分析 |
| `freeze_flag` | 输出 | bool | 冻结/回退状态标志，高电平表示参数更新已暂停 |

#### 桥接机制说明

| 机制 | 说明 |
|------|------|
| **DE‑TDF 桥接** | 使用 `sca_de::sca_in/out` 与 TDF 模块的 `sca_tdf::sca_de::sca_in/out` 端口连接 |
| **时序对齐** | DE 事件驱动或周期驱动更新，参数在下一 TDF 采样周期生效；避免读写竞争与跨域延迟不确定性 |
| **数据同步** | 通过缓冲机制或时间戳标记，确保参数原子更新（多参数同时切换时） |
| **延迟处理** | DE→TDF 桥接可能有 1 个 TDF 周期延迟，算法设计需考虑此延迟对稳定性影响 |

### 2.2 参数配置（AdaptionParams）

#### 基本参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `Fs` | double | 80e9 | 系统采样率（Hz），影响更新周期与时序对齐 |
| `UI` | double | 2.5e-11 | 单位间隔（秒），用于归一化相位误差与抖动指标 |
| `seed` | int | 12345 | 随机种子（用于仿真可重复性，与算法随机化扰动相关） |
| `update_mode` | string | "multi-rate" | 调度模式："event"（事件驱动）\|"periodic"（周期驱动）\|"multi-rate"（多速率） |
| `fast_update_period` | double | 2.5e-10 | 快路径更新周期（秒，用于 CDR/阈值，约 10 UI） |
| `slow_update_period` | double | 2.5e-7 | 慢路径更新周期（秒，用于 AGC/DFE，约 10000 UI） |

#### AGC 子结构

自动增益控制参数，通过 PI 控制器动态调整 VGA 增益以维持目标输出幅度。

| 参数 | 说明 |
|------|------|
| `enabled` | 启用 AGC 算法 |
| `target_amplitude` | 目标幅度（V 或归一化），期望的输出信号幅度 |
| `kp` | PI 控制器比例系数，控制响应速度 |
| `ki` | PI 控制器积分系数，控制稳态误差 |
| `gain_min` | 最小增益（线性倍数），饱和下限 |
| `gain_max` | 最大增益（线性倍数），饱和上限 |
| `rate_limit` | 增益变化速率限制（linear/s），防止过快变化导致不稳定 |
| `initial_gain` | 初始增益（线性倍数），系统启动时的默认值 |

**工作原理**：
1. 从 `amplitude_rms` 端口读取当前输出幅度
2. 计算幅度误差：`amp_error = target_amplitude - current_amplitude`
3. PI 控制器更新：`gain = P + I`，其中 `P = kp * amp_error`，`I += ki * amp_error * dt`
4. 增益饱和钳位：`gain = clamp(gain, gain_min, gain_max)`
5. 速率限制：防止单次更新增益变化过大
6. 输出到 `vga_gain` 端口，下一 TDF 周期生效

#### DFE 子结构

DFE 抽头更新参数，使用自适应滤波算法在线优化 DFE 抽头系数以抑制码间干扰（ISI）。

| 参数 | 说明 |
|------|------|
| `enabled` | 启用 DFE 在线更新 |
| `num_taps` | 抽头数量（通常 3‑8），决定 DFE 可抑制的 ISI 深度 |
| `algorithm` | 更新算法："lms" \| "sign-lms" \| "nlms"，选择不同的自适应策略 |
| `mu` | 步长系数（LMS/Sign‑LMS），控制收敛速度与稳定性权衡 |
| `leakage` | 泄漏系数（0‑1），防止噪声累积导致抽头发散 |
| `initial_taps` | 初始抽头系数数组 [tap1, tap2, ..., tapN]，系统启动时的默认值 |
| `tap_min` | 单个抽头最小值（饱和约束），防止抽头系数过小 |
| `tap_max` | 单个抽头最大值（饱和约束），防止抽头系数过大 |
| `freeze_threshold` | 误差超过此阈值时冻结更新，避免异常噪声干扰 |

**工作原理**（以 Sign-LMS 为例）：
1. 从 `error_count` 端口或专用误差端口读取当前判决误差 `e(n)`
2. 对每个抽头 `i`：
   - 获取延迟 `i` 个符号的判决值 `x[n-i]`
   - Sign-LMS 更新：`tap[i] = tap[i] + mu * sign(e(n)) * sign(x[n-i])`
   - 泄露处理：`tap[i] = (1 - leakage) * tap[i]`
   - 饱和钳位：`tap[i] = clamp(tap[i], tap_min, tap_max)`
3. 冻结条件：若 `|e(n)| > freeze_threshold`，暂停所有抽头更新
4. 输出到 `dfe_taps` 数组端口，DFE Summer 在下一周期使用新系数

#### 阈值自适应子结构

采样阈值自适应参数，通过动态调整判决阈值和迟滞窗口优化误码率性能。

| 参数 | 说明 |
|------|------|
| `enabled` | 启用阈值自适应 |
| `initial` | 初始阈值（V），系统启动时的默认判决电平 |
| `hysteresis` | 迟滞窗口（V），抗噪迟滞宽度 |
| `adapt_step` | 调整步长（V/更新），每次更新的阈值变化量 |
| `target_ber` | 目标 BER（用于阈值优化目标，可选），自适应算法的优化目标 |
| `drift_threshold` | 电平漂移阈值（V），超过时触发阈值调整 |

**工作原理**：
1. 从采样信号统计高/低电平分布（均值、方差）
2. 或使用误码趋势（`error_count` 变化率）作为反馈
3. 梯度下降或二分查找策略调整阈值，向误码减小方向移动
4. 根据噪声强度动态调整迟滞窗口，平衡抗噪与灵敏度
5. 输出到 `sampler_threshold` 和 `sampler_hysteresis` 端口

#### CDR PI 子结构

CDR PI 控制器参数，通过相位插值器命令调整采样相位，实现时钟数据恢复。

| 参数 | 说明 |
|------|------|
| `enabled` | 启用 PI 控制 |
| `kp` | 比例系数，控制环路响应速度 |
| `ki` | 积分系数，控制稳态相位误差 |
| `phase_resolution` | 相位命令分辨率（秒），量化步长 |
| `phase_range` | 相位命令范围（±秒），最大相位调整范围 |
| `anti_windup` | 启用抗积分饱和，防止积分器溢出 |
| `initial_phase` | 初始相位命令（秒），系统启动时的默认相位 |

**工作原理**：
1. 从 `phase_error` 端口读取相位误差（早/晚采样差值）
2. PI 控制器更新：`phase_cmd = P + I`，其中 `P = kp * phase_error`，`I += ki * phase_error * dt`
3. 抗饱和处理：若 `phase_cmd` 超出 `±phase_range`，钳位并停止积分累积
4. 量化处理：按 `phase_resolution` 量化命令：`phase_cmd_q = round(phase_cmd / phase_resolution) * phase_resolution`
5. 输出到 `phase_cmd` 端口，相位插值器根据命令调整采样时刻

#### 安全与回退子结构

安全监管机制参数，提供冻结、回退、快照保存等鲁棒性保障。

| 参数 | 说明 |
|------|------|
| `freeze_on_error` | 误差超限时是否冻结所有更新，防止参数发散 |
| `rollback_enable` | 是否支持参数回滚至上次稳定快照，故障恢复机制 |
| `snapshot_interval` | 稳定快照保存间隔（秒），周期性记录参数历史 |
| `error_burst_threshold` | 误码暴涨阈值（触发冻结/回退），异常检测门限 |

**工作原理**：
1. **冻结策略**：检测误码暴涨（`error_count > error_burst_threshold`）、幅度异常、相位失锁等条件，暂停所有参数更新
2. **快照保存**：每隔 `snapshot_interval` 保存一次当前参数（增益、抽头、阈值、相位）到历史缓冲
3. **回退策略**：冻结持续时间超过阈值或关键指标持续劣化时，恢复至上次稳定快照参数，重新启动训练
4. **历史记录**：维护最近 N 次更新的参数与指标历史，输出到 trace（`update_count`、`freeze_flag`）用于诊断

## 4. 测试平台架构

### 4.1 测试平台设计思想

Adaption 测试平台（`AdaptionTestbench`）采用多场景驱动的分层测试架构，支持四大自适应算法（AGC、DFE、阈值自适应、CDR PI）的独立测试与联合验证。核心设计理念：

1. **分层测试策略**：将测试分为单元级（单算法）、集成级（多算法联合）、系统级（完整链路）三个层次，逐步验证算法正确性与系统鲁棒性

2. **多速率仿真支持**：通过 DE 域事件触发器模拟快路径（每 10-100 UI）与慢路径（每 1000-10000 UI）的并行调度，验证多速率架构的正确性

3. **可重构测试环境**：支持通过配置文件快速切换测试场景（短通道/长通道/串扰/抖动），无需重新编译

4. **自动化验证框架**：集成收敛性检测、稳定性监控、回归指标计算，自动判断测试通过/失败

5. **故障注入机制**：支持误码暴涨、幅度异常、相位失锁等故障注入，验证冻结/回退机制的鲁棒性

### 4.2 测试场景定义

测试平台支持八种核心测试场景：

| 场景 | 命令行参数 | 测试目标 | 输出文件 |
|------|----------|---------|----------|
| BASIC_FUNCTION | `basic` / `0` | 基本功能测试（所有算法联合） | adaption_basic.csv |
| AGC_TEST | `agc` / `1` | AGC 自动增益控制 | adaption_agc.csv |
| DFE_TEST | `dfe` / `2` | DFE 抽头更新（LMS/Sign-LMS） | adaption_dfe.csv |
| THRESHOLD_TEST | `threshold` / `3` | 阈值自适应算法 | adaption_threshold.csv |
| CDR_PI_TEST | `cdr` / `4` | CDR PI 控制器 | adaption_cdr.csv |
| FREEZE_ROLLBACK | `freeze` / `5` | 冻结与回退机制 | adaption_freeze.csv |
| MULTI_RATE | `multirate` / `6` | 多速率调度架构 | adaption_multirate.csv |
| SCENARIO_SWITCH | `switch` / `7` | 多场景热切换 | adaption_switch.csv |

### 4.3 场景配置详解

#### BASIC_FUNCTION - 基本功能测试

验证 Adaption 模块在标准链路场景下所有算法的联合工作能力。

- **信号源**：PRBS-31 伪随机序列
- **符号率**：40 Gbps（UI = 25ps）
- **通道**：标准长通道（S21 插损 15dB @ 20GHz）
- **AGC 配置**：目标幅度 0.4V，增益范围 [0.5, 8.0]
- **DFE 配置**：5 个抽头，Sign-LMS 算法，步长 1e-4
- **阈值配置**：初始阈值 0.0V，迟滞 0.02V
- **CDR 配置**：Kp=0.01，Ki=1e-4，相位范围 ±0.5 UI
- **仿真时长**：10 μs（400,000 UI）
- **验证点**：
  - AGC 增益收敛至稳定值（变化 < 1%）
  - DFE 抽头收敛至稳定值（变化 < 0.001）
  - 相位误差 RMS < 0.01 UI
  - 误码率 < 1e-9

#### AGC_TEST - AGC 自动增益控制测试

验证 AGC PI 控制器在不同幅度输入下的增益调整能力。

- **信号源**：幅度阶跃信号（0.2V → 0.6V → 0.3V）
- **阶跃时刻**：2 μs、5 μs
- **AGC 配置**：目标幅度 0.4V，Kp=0.1，Ki=100.0
- **验证点**：
  - 阶跃响应无超调（增益变化率受速率限制）
  - 稳态误差 < 5%
  - 收敛时间 < 5000 UI
  - 增益范围 [gain_min, gain_max] 钳位生效

#### DFE_TEST - DFE 抽头更新测试

验证 DFE 抽头更新算法在 ISI 场景下的收敛性与稳定性。

- **信号源**：PRBS-31
- **通道**：强 ISI 通道（S21 插损 25dB @ 20GHz）
- **DFE 配置**：
  - 抽头数量：8 个
  - 算法：Sign-LMS（可切换 LMS/NLMS）
  - 步长：1e-4
  - 泄漏系数：1e-6
- **验证点**：
  - 抽头在 10000 UI 内收敛至稳定值
  - 收敛后抽头变化 < 0.001
  - 误码率改善 > 10x（vs 无 DFE）
  - 长时间运行（1e6 UI）无发散

#### THRESHOLD_TEST - 阈值自适应测试

验证阈值自适应算法在电平漂移和噪声变化下的跟踪能力。

- **信号源**：PRBS-31 + 直流偏移漂移（±50mV）
- **偏移频率**：1 MHz
- **噪声注入**：RJ sigma=5ps
- **阈值配置**：自适应步长 0.001V，漂移阈值 0.05V
- **验证点**：
  - 阈值自动跟踪电平漂移（误差 < 10mV）
  - 迟滞窗口根据噪声强度调整
  - 误码率最小化
  - 异常噪声暴涨时不误触发极端阈值

#### CDR_PI_TEST - CDR PI 控制器测试

验证 CDR PI 控制器在不同相位误差下的锁定能力与抖动抑制性能。

- **信号源**：PRBS-31 + 相位噪声
- **相位噪声**：SJ 5MHz, 2ps + RJ 1ps
- **初始相位误差**：±0.5 UI
- **CDR 配置**：Kp=0.01，Ki=1e-4，相位范围 ±0.5 UI
- **验证点**：
  - 锁定时间 < 1000 UI
  - 稳态相位误差 RMS < 0.01 UI
  - 大相位扰动下积分器不溢出
  - 相位噪声抑制符合预期

#### FREEZE_ROLLBACK - 冻结与回退机制测试

验证冻结与回退机制在异常场景下的鲁棒性。

- **信号源**：PRBS-31
- **故障注入**：
  - 3 μs：误码暴涨（error_count > 100）
  - 6 μs：幅度异常（amplitude_rms 超出范围）
  - 9 μs：相位失锁（|phase_error| > 0.5 UI）
- **安全配置**：
  - 误码暴涨阈值：100
  - 快照保存间隔：1 μs
  - 回退启用：true
- **验证点**：
  - 故障触发时冻结标志置位
  - 参数更新暂停
  - 回退至上次稳定快照
  - 恢复时间 < 2000 UI

#### MULTI_RATE - 多速率调度架构测试

验证快路径与慢路径的并行调度与优先级处理。

- **信号源**：PRBS-31
- **更新周期**：
  - 快路径：25ps（1 UI）
  - 慢路径：2.5ns（100 UI）
- **验证点**：
  - 快路径更新次数 ≈ 400,000
  - 慢路径更新次数 ≈ 4,000
  - 无竞态条件
  - 参数更新时间戳正确

#### SCENARIO_SWITCH - 多场景热切换测试

验证场景切换时的参数原子性与防抖策略。

- **场景序列**：
  - 0-3 μs：短通道（S21 插损 5dB）
  - 3-6 μs：长通道（S21 插损 15dB）
  - 6-9 μs：串扰场景
- **切换时刻**：3 μs、6 μs
- **验证点**：
  - 参数原子切换（所有参数同时更新）
  - 切换后进入训练期（误码统计冻结）
  - 无参数不一致导致的瞬态误码
  - 切换时间 < 100 UI

### 4.4 信号连接拓扑

测试平台的模块连接关系如下：

```
┌─────────────────────────────────────────────────────────────────┐
│                    AdaptionTestbench (DE 域)                     │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  信号源      │  │  通道模型    │  │  RX 链路     │          │
│  │  (WaveGen)   │  │  (Channel)   │  │  (CTLE/VGA/ │          │
│  │              │  │              │  │   Sampler)   │          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
│         │                 │                 │                   │
│         └─────────────────┴─────────────────┘                   │
│                           │                                     │
│                           ▼                                     │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                    AdaptionDe (DE 域)                     │  │
│  │                                                           │  │
│  │  输入端口:                                                 │  │
│  │    phase_error ←──────────────────────────────────────┐  │  │
│  │    amplitude_rms ←─────────────────────────────────┐  │  │
│  │    error_count ←────────────────────────────────┐  │  │  │
│  │    isi_metric ←─────────────────────────────┐  │  │  │  │
│  │    mode ←──────────────────────────────┐  │  │  │  │  │
│  │    reset ←─────────────────────────┐  │  │  │  │  │  │
│  │    scenario_switch ←──────────┐  │  │  │  │  │  │  │
│  │                               │  │  │  │  │  │  │  │
│  │  输出端口:                     │  │  │  │  │  │  │  │
│  │    vga_gain ──────────────────┼──┼──┼──┼──┼──┼──┼──┼──▶  │
│  │    dfe_taps ──────────────────┼──┼──┼──┼──┼──┼──┼──┼──▶  │
│  │    sampler_threshold ─────────┼──┼──┼──┼──┼──┼──┼──┼──▶  │
│  │    sampler_hysteresis ────────┼──┼──┼──┼──┼──┼──┼──┼──▶  │
│  │    phase_cmd ─────────────────┼──┼──┼──┼──┼──┼──┼──┼──▶  │
│  │    update_count ──────────────┼──┼──┼──┼──┼──┼──┼──┼──▶  │
│  │    freeze_flag ───────────────┼──┼──┼──┼──┼──┼──┼──┼──▶  │
│  └───────────────────────────────┼──┼──┼──┼──┼──┼──┼──┼──┘  │
│                                  │  │  │  │  │  │  │  │       │
│  ┌───────────────────────────────┼──┼──┼──┼──┼──┼──┼──┼──┐  │
│  │  监控器 (TraceMonitor)        │  │  │  │  │  │  │  │  │  │
│  │  - 记录参数时间序列           │  │  │  │  │  │  │  │  │  │
│  │  - 计算收敛指标               │  │  │  │  │  │  │  │  │  │
│  │  - 输出 CSV 文件              │  │  │  │  │  │  │  │  │  │
│  └───────────────────────────────┼──┼──┼──┼──┼──┼──┼──┼──┘  │
│                                  │  │  │  │  │  │  │  │       │
└──────────────────────────────────┼──┼──┼──┼──┼──┼──┼──┼───────┘
                                   │  │  │  │  │  │  │  │
                                   ▼  ▼  ▼  ▼  ▼  ▼  ▼  ▼
┌─────────────────────────────────────────────────────────────────┐
│                    TDF 域模块（RX/CDR）                          │
│                                                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │   VGA    │  │  DFE     │  │ Sampler  │  │   CDR    │        │
│  │          │  │  Summer   │  │          │  │          │        │
│  │ vga_gain │  │ dfe_taps │  │threshold │  │phase_cmd │        │
│  │    ◄─────┼──┼─────◄────┼──┼─────◄────┼──┼─────◄────┼        │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘        │
│                                                                  │
│  反馈信号:                                                       │
│    phase_error ──────────────────────────────────────────────┐   │
│    amplitude_rms ────────────────────────────────────────┐   │   │
│    error_count ──────────────────────────────────────┐   │   │   │
│    isi_metric ─────────────────────────────────┐   │   │   │   │
└─────────────────────────────────────────────────┼───┼───┼───┼───┘
                                                  │   │   │   │
                                                  ▼   ▼   ▼   ▼
                                            ┌─────────────────┐
                                            │  误码计数器     │
                                            │  幅度统计模块   │
                                            │  相位检测器     │
                                            └─────────────────┘
```

### 4.5 辅助模块说明

#### WaveGen - 波形生成器

支持多种波形类型，用于生成测试信号。

- **波形类型**：PRBS7/9/15/23/31、正弦波、方波、DC
- **抖动注入**：RJ（随机抖动）、SJ（周期性抖动）、DJ（确定性抖动）
- **调制**：AM（幅度调制）、PM（相位调制）
- **可配置参数**：幅度、频率、共模电压、抖动参数

#### Channel - 通道模型

基于 S 参数的信道模型，支持不同通道场景。

- **S 参数文件**：Touchstone 格式（.s2p, .s4p）
- **简单模型**：衰减、带宽限制
- **串扰建模**：NEXT（近端串扰）、FEXT（远端串扰）
- **双向传输**：支持反射与回波

#### RX 链路 - 接收端链路

包含 CTLE、VGA、Sampler、DFE Summer 等模块。

- **CTLE**：连续时间线性均衡器，补偿高频衰减
- **VGA**：可变增益放大器，支持 AGC 控制
- **Sampler**：采样器，支持阈值与迟滞调整
- **DFE Summer**：判决反馈均衡器，支持在线抽头更新

#### CDR - 时钟数据恢复

提供相位误差反馈，支持相位插值器控制。

- **相位检测器**：Bang-Bang PD 或线性 PD
- **PI 控制器**：接收相位命令，调整采样相位
- **相位插值器**：根据命令调整采样时刻

#### TraceMonitor - 追踪监控器

记录关键信号时间序列，用于后处理分析。

- **记录信号**：vga_gain、dfe_taps、sampler_threshold、phase_cmd、update_count、freeze_flag
- **统计计算**：均值、RMS、收敛时间、变化率
- **输出格式**：CSV 文件，包含时间戳和信号值
- **收敛检测**：自动判断参数是否收敛（变化 < 阈值持续 N 次更新）

#### FaultInjector - 故障注入器

模拟异常场景，验证冻结/回退机制。

- **误码暴涨**：注入大量判决误差
- **幅度异常**：修改幅度统计值超出范围
- **相位失锁**：注入大相位误差
- **信号丢失**：设置输入为零或噪声

#### ScenarioManager - 场景管理器

管理多场景切换与配置加载。

- **配置加载**：从 JSON/YAML 文件加载场景参数
- **场景切换**：触发场景切换事件，更新所有参数
- **防抖策略**：切换后进入训练期，冻结误码统计
- **历史记录**：记录场景切换时间与参数快照

---

## 3. 核心实现机制

### 3.1 信号处理流程

Adaption 模块运行于 SystemC DE（Discrete Event）域，采用分层多速率处理架构，通过 `processing()` 方法实现完整的自适应控制流程。模块内部维护四个核心处理线程：快路径线程（CDR PI、阈值自适应）、慢路径线程（AGC、DFE 抽头更新）、安全监管线程（冻结/回退检测）、参数输出线程（DE-TDF 桥接）。

**初始化阶段**（构造函数）：
1. **配置加载**：从 JSON/YAML 配置文件读取所有算法参数，初始化 `AdaptionParams` 结构体
2. **参数初值设置**：将 `vga_gain`、`dfe_taps`、`sampler_threshold`、`phase_cmd` 设置为配置的初值（`initial_gain`、`initial_taps`、`initial`、`initial_phase`）
3. **内部状态初始化**：
   - AGC 积分器 `I_agc = 0`，前一增益 `gain_prev = initial_gain`
   - DFE 抽头缓冲区 `taps_buffer = initial_taps`，误差累积 `error_sum = 0`
   - CDR PI 积分器 `I_cdr = 0`，前一相位命令 `phase_prev = initial_phase`
   - 安全快照缓冲区清空，冻结标志 `freeze_flag = false`
4. **调度器配置**：根据 `update_mode` 创建定时事件（周期驱动）或设置事件触发器（事件驱动）
5. **Trace 注册**：注册关键信号（`vga_gain`、`dfe_taps`、`sampler_threshold`、`phase_cmd`、`update_count`、`freeze_flag`）到波形追踪系统

**快路径处理流程**（高频更新，每 10-100 UI 触发一次）：
```
输入读取 → CDR PI 处理 → 阈值自适应处理 → 参数暂存 → 更新计数器
```

**步骤1-输入读取**：
- 从 `phase_error` 端口读取相位误差（CDR 相位检测器输出）
- 从 `error_count` 端口读取误码累积或当前判决误差
- 从 `mode` 和 `reset` 端口读取系统状态（若 `reset=true`，跳转至初始化）

**步骤2-CDR PI 控制器处理**：
- 若 `cdr_pi.enabled = false`，跳过此步骤
- 计算比例项：`P = kp_cdr * phase_error`
- 更新积分项：`I_cdr += ki_cdr * phase_error * dt`（`dt` 为快路径更新周期）
- 计算相位命令：`phase_cmd = P + I_cdr`
- 抗饱和处理：若 `phase_cmd > phase_range`，钳位至 `phase_range` 并停止 `I_cdr` 累积；若 `phase_cmd < -phase_range`，钳位至 `-phase_range` 并停止 `I_cdr` 累积
- 量化处理：`phase_cmd_q = round(phase_cmd / phase_resolution) * phase_resolution`
- 暂存至输出缓冲区：`phase_cmd_buffer = phase_cmd_q`

**步骤3-阈值自适应处理**：
- 若 `threshold.enabled = false`，跳过此步骤
- 从采样信号统计或误码趋势估算梯度（向误码减小方向）
- 阈值调整：`threshold_new = threshold_prev + adapt_step * sign(gradient)`
- 钳位限制：`threshold_new = clamp(threshold_new, -vcm_out, vcm_out)`
- 迟滞更新：根据噪声强度调整 `hysteresis`（噪声大时增大迟滞，噪声小时减小迟滞）
- 暂存至输出缓冲区：`sampler_threshold_buffer = threshold_new`，`sampler_hysteresis_buffer = hysteresis_new`

**步骤4-更新计数器**：
- 快路径计数器：`update_count_fast++`
- 总更新计数器：`update_count = update_count_slow + update_count_fast`

**慢路径处理流程**（低频更新，每 1000-10000 UI 触发一次）：
```
输入读取 → AGC 处理 → DFE 抽头更新 → 冻结检测 → 快照保存 → 更新计数器
```

**步骤1-输入读取**：
- 从 `amplitude_rms` 端口读取当前输出幅度（RMS 或峰值）
- 从 `isi_metric` 端口读取码间干扰指标（可选，用于 DFE 更新策略）
- 从 `error_count` 端口读取误码累积（DFE 用）

**步骤2-AGC PI 控制器处理**：
- 若 `agc.enabled = false`，跳过此步骤
- 计算幅度误差：`amp_error = target_amplitude - amplitude_rms`
- 计算比例项：`P = kp_agc * amp_error`
- 更新积分项：`I_agc += ki_agc * amp_error * dt_slow`（`dt_slow` 为慢路径更新周期）
- 计算增益：`gain = P + I_agc`
- 增益饱和钳位：`gain = clamp(gain, gain_min, gain_max)`
- 速率限制：`delta_gain = clamp(gain - gain_prev, -rate_limit * dt_slow, rate_limit * dt_slow)`
- 更新增益：`gain_out = gain_prev + delta_gain`，`gain_prev = gain_out`
- 暂存至输出缓冲区：`vga_gain_buffer = gain_out`

**步骤3-DFE 抽头更新**：
- 若 `dfe.enabled = false`，跳过此步骤
- 从误差端口读取当前判决误差 `e(n)`（或从 `error_count` 估算）
- 对每个抽头 `i`（`i = 1` 到 `num_taps`）：
  - 获取延迟 `i` 个符号的判决值 `x[n-i]`（需从 RX 模块获取，通过专用端口或历史缓冲）
  - 根据 `algorithm` 选择更新策略：
    - **LMS**：`tap[i] = tap[i] + mu * e(n) * x[n-i]`
    - **Sign-LMS**：`tap[i] = tap[i] + mu * sign(e(n)) * sign(x[n-i])`
    - **NLMS**：`tap[i] = tap[i] + mu * e(n) * x[n-i] / (signal_power + epsilon)`
  - 泄漏处理：`tap[i] = (1 - leakage) * tap[i]`
  - 饱和钳位：`tap[i] = clamp(tap[i], tap_min, tap_max)`
- 冻结检测：若 `|e(n)| > freeze_threshold`，设置 `dfe_frozen = true`，暂停所有抽头更新
- 暂存至输出缓冲区：`dfe_taps_buffer = taps`

**步骤4-冻结检测**：
- 若 `safety.freeze_on_error = false`，跳过此步骤
- 检测误码暴涨：若 `error_count > error_burst_threshold`，设置 `freeze_flag = true`
- 检测幅度异常：若 `amplitude_rms` 超出 `[target_amplitude * 0.5, target_amplitude * 2.0]`，设置 `freeze_flag = true`
- 检测相位失锁：若 `|phase_error| > 0.5 * UI` 持续超过 1000 UI，设置 `freeze_flag = true`

**步骤5-快照保存**：
- 若 `safety.rollback_enable = false`，跳过此步骤
- 若当前时间距离上次快照时间 ≥ `snapshot_interval`：
  - 保存当前参数到快照缓冲区：`snapshot = {vga_gain, dfe_taps, sampler_threshold, phase_cmd, timestamp}`
  - 更新上次快照时间：`last_snapshot_time = current_time`

**步骤6-更新计数器**：
- 慢路径计数器：`update_count_slow++`
- 总更新计数器：`update_count = update_count_slow + update_count_fast`

**安全监管处理流程**（每次快/慢路径更新后执行）：
```
冻结状态检查 → 回退触发 → 参数输出 → DE-TDF 桥接
```

**步骤1-冻结状态检查**：
- 若 `freeze_flag = true` 且 `mode = 3`（冻结模式），跳过所有参数更新，维持当前值
- 若 `freeze_flag = true` 且 `mode != 3`（非冻结模式），检查冻结条件是否解除：
  - 若误码恢复正常（`error_count < error_burst_threshold * 0.8`）且幅度正常且相位锁定，清除 `freeze_flag = false`

**步骤2-回退触发**：
- 若 `safety.rollback_enable = false`，跳过此步骤
- 若冻结持续时间超过阈值（`freeze_duration > 2 * snapshot_interval`）或关键指标持续劣化：
  - 从快照缓冲区恢复参数：`{vga_gain, dfe_taps, sampler_threshold, phase_cmd} = snapshot`
  - 重置积分器：`I_agc = 0`，`I_cdr = 0`
  - 重置计数器：`update_count = 0`
  - 清除冻结标志：`freeze_flag = false`
  - 进入训练模式（`mode = 1`）

**步骤3-参数输出**：
- 将缓冲区参数写入输出端口：
  - `vga_gain.write(vga_gain_buffer)`
  - `dfe_taps.write(dfe_taps_buffer)`
  - `sampler_threshold.write(sampler_threshold_buffer)`
  - `sampler_hysteresis.write(sampler_hysteresis_buffer)`
  - `phase_cmd.write(phase_cmd_buffer)`
- 写入状态信号：
  - `update_count.write(update_count)`
  - `freeze_flag.write(freeze_flag)`

**步骤4-DE-TDF 桥接**：
- 输出端口通过 `sca_de::sca_out` 连接到 TDF 模块的 `sca_tdf::sca_de::sca_in` 端口
- 参数在当前 DE 事件完成后，下一 TDF 采样周期生效
- SystemC-AMS 桥接机制保证原子性：多参数同时更新时，TDF 模块在同一采样周期读取所有新参数

**复位处理流程**（`reset = true` 时触发）：
1. 清除所有积分器：`I_agc = 0`，`I_cdr = 0`
2. 恢复参数初值：`vga_gain = initial_gain`，`dfe_taps = initial_taps`，`sampler_threshold = initial`，`phase_cmd = initial_phase`
3. 清空快照缓冲区
4. 清除冻结标志：`freeze_flag = false`
5. 重置计数器：`update_count = 0`，`update_count_fast = 0`，`update_count_slow = 0`

### 3.2 关键算法实现机制

#### 3.2.1 PI 控制器实现（AGC 与 CDR）

PI（比例-积分）控制器是 AGC 和 CDR 的核心算法，通过比例项快速响应误差，通过积分项消除稳态误差。

**数学模型**：
```
u(t) = Kp * e(t) + Ki * ∫e(τ)dτ
```
其中 `e(t)` 为误差信号，`u(t)` 为控制输出，`Kp` 为比例系数，`Ki` 为积分系数。

**离散化实现**（前向欧拉法）：
```
P[n] = Kp * e[n]
I[n] = I[n-1] + Ki * e[n] * T
u[n] = P[n] + I[n]
```
其中 `T` 为采样周期（快路径或慢路径更新周期）。

**抗积分饱和（Anti-Windup）策略**：
1. **条件积分法**：仅当输出未饱和时更新积分项
   ```cpp
   if (u[n-1] < u_max && u[n-1] > u_min) {
       I[n] = I[n-1] + Ki * e[n] * T;
   }
   ```
2. **反向积分法**：输出饱和时，积分项向反方向累积
   ```cpp
   if (u[n] > u_max) {
       u[n] = u_max;
       I[n] -= K_windup * (u[n] - u_max);  // 反向积分
   }
   ```
3. **钳位法**：输出饱和时，钳位积分项
   ```cpp
   if (u[n] > u_max) {
       u[n] = u_max;
       I[n] = I[n];  // 停止累积
   }
   ```

**AGC PI 控制器设计要点**：
- **目标**：维持输出幅度为 `target_amplitude`
- **误差定义**：`amp_error = target_amplitude - amplitude_rms`
- **增益范围**：`[gain_min, gain_max]`，防止过小增益导致信号丢失或过大增益导致饱和
- **速率限制**：`rate_limit`，防止增益突变导致系统不稳定
- **典型参数**：`Kp = 0.01-0.1`，`Ki = 10-1000`（根据环路带宽设计）

**CDR PI 控制器设计要点**：
- **目标**：最小化相位误差，实现时钟数据恢复
- **误差定义**：`phase_error` 来自相位检测器（早/晚采样差值）
- **相位范围**：`±phase_range`，通常为 ±0.5 UI（单位间隔）
- **分辨率**：`phase_resolution`，相位插值器的量化步长
- **典型参数**：`Kp = 0.001-0.1`，`Ki = 1e-5 - 1e-3`（根据锁定时间和抖动容忍度设计）

#### 3.2.2 LMS/Sign-LMS 自适应滤波算法（DFE）

DFE 抽头更新采用自适应滤波算法，通过最小化判决误差优化抽头系数，抑制码间干扰（ISI）。

**LMS（最小均方）算法数学模型**：
```
目标：最小化 E[e²(n)]，其中 e(n) = d(n) - y(n)
抽头更新：w(n+1) = w(n) + μ * e(n) * x(n)
```
其中 `w(n)` 为抽头系数向量，`x(n)` 为输入向量（延迟的判决值），`μ` 为步长系数，`e(n)` 为判决误差。

**Sign-LMS 算法简化**：
```
抽头更新：w(n+1) = w(n) + μ * sign(e(n)) * sign(x(n))
```
优点：仅需加法运算，适合硬件实现；缺点：收敛速度较慢，稳态误差较大。

**NLMS（归一化 LMS）算法**：
```
抽头更新：w(n+1) = w(n) + μ * e(n) * x(n) / (||x(n)||² + ε)
```
优点：步长自适应，收敛速度快且稳定；缺点：需要除法运算，计算复杂度高。

**泄漏 LMS（Leaky LMS）**：
```
抽头更新：w(n+1) = (1 - λ) * w(n) + μ * e(n) * x(n)
```
其中 `λ` 为泄漏系数（`1e-6 - 1e-4`），防止噪声累积导致抽头发散。

**DFE 抽头更新实现流程**：
1. **误差获取**：从 `error_count` 端口或专用误差端口读取 `e(n)`
2. **输入向量构建**：从 RX 模块获取延迟的判决值 `x[n-1], x[n-2], ..., x[n-N]`（N 为抽头数量）
3. **抽头更新**：对每个抽头 `i`，根据算法选择更新策略
4. **泄漏处理**：`tap[i] *= (1 - leakage)`
5. **饱和钳位**：`tap[i] = clamp(tap[i], tap_min, tap_max)`
6. **冻结检测**：若 `|e(n)| > freeze_threshold`，暂停更新

**步长选择原则**：
- **收敛速度**：步长越大，收敛越快，但稳态误差越大
- **稳定性**：步长需满足 `0 < μ < 2 / (N * P_x)`，其中 `P_x` 为输入信号功率
- **典型值**：`μ = 1e-5 - 1e-3`，根据信号功率和噪声水平调整

#### 3.2.3 阈值自适应算法

阈值自适应算法通过动态调整判决阈值和迟滞窗口，优化误码率性能，适应信号电平漂移和噪声变化。

**梯度下降法**：
```
目标：最小化误码率 BER(threshold)
梯度估计：∇BER ≈ (BER(threshold + Δ) - BER(threshold - Δ)) / (2Δ)
阈值更新：threshold_new = threshold_old - α * ∇BER
```
其中 `α` 为学习率（`adapt_step`），`Δ` 为扰动步长。

**电平统计法**：
1. 统计高电平均值 `μ_H` 和低电平均值 `μ_L`
2. 计算最佳阈值：`threshold_opt = (μ_H + μ_L) / 2`
3. 阈值更新：`threshold_new = threshold_old * (1 - β) + threshold_opt * β`（指数平滑，β 为平滑系数）

**迟滞窗口自适应**：
- **噪声估计**：计算信号 RMS 或标准差 `σ_noise`
- **迟滞计算**：`hysteresis = k * σ_noise`，其中 `k` 为系数（通常 2-3）
- **限制**：`hysteresis = clamp(hysteresis, 0.01, 0.1)`（防止过大或过小）

**实现流程**：
1. 从采样信号统计高/低电平分布（均值、方差）
2. 或使用误码趋势（`error_count` 变化率）作为反馈
3. 梯度下降或电平统计法调整阈值
4. 根据噪声强度动态调整迟滞窗口
5. 输出到 `sampler_threshold` 和 `sampler_hysteresis` 端口

#### 3.2.4 DE-TDF 桥接机制

DE-TDF 桥接是 Adaption 模块与 AMS 域模块通信的关键机制，通过 SystemC-AMS 提供的桥接端口实现跨域参数传递。

**桥接端口类型**：
- **DE 域**：`sca_de::sca_in<T>`（输入），`sca_de::sca_out<T>`（输出）
- **TDF 域**：`sca_tdf::sca_de::sca_in<T>`（输入），`sca_tdf::sca_de::sca_out<T>`（输出）

**连接方式**：
```cpp
// Adaption 模块（DE 域）
sca_de::sca_out<double> vga_gain;

// RX VGA 模块（TDF 域）
sca_tdf::sca_de::sca_in<double> vga_gain_in;

// 连接
adaption.vga_gain(rx.vga_gain_in);
```

**时序对齐机制**：
1. **DE 事件触发**：Adaption 模块在 DE 域按事件或周期触发更新
2. **参数写入**：DE 输出端口写入参数到桥接缓冲区
3. **TDF 采样**：TDF 模块在下一采样周期从桥接缓冲区读取参数
4. **延迟处理**：DE→TDF 桥接有 1 个 TDF 周期延迟，算法设计需考虑此延迟

**原子性保证**：
- 多参数同时更新时，SystemC-AMS 桥接机制保证原子性
- TDF 模块在同一采样周期读取所有新参数，避免参数不一致

**数据同步策略**：
- **缓冲机制**：DE 输出通过缓冲区传递，TDF 模块读取最新值
- **时间戳标记**：对于时间敏感参数，可添加时间戳标记
- **防抖策略**：切换场景后进入短暂训练期，冻结误码统计

### 3.3 设计决策说明

#### 3.3.1 多速率调度架构的选择

**决策**：采用多速率调度架构，将算法分为快路径（CDR PI、阈值自适应，每 10-100 UI 更新一次）与慢路径（AGC、DFE 抽头，每 1000-10000 UI 更新一次）。

**原因**：
1. **符合实际硬件**：实际 SerDes 芯片采用分层控制架构，CDR 和阈值需要快速响应相位和电平变化，而 AGC 和 DFE 抽头变化较慢
2. **优化计算开销**：快路径更新频率高但计算量小（PI 控制、阈值调整），慢路径更新频率低但计算量大（DFE 抽头更新 O(N)），平衡性能与开销
3. **稳定性考虑**：DFE 抽头更新过快可能导致抖动和发散，慢速更新提供更好的稳定性
4. **灵活性**：可根据场景动态调整更新频率，如训练阶段使用快更新，数据阶段使用慢更新

**替代方案**：
- **单速率周期驱动**：所有算法按同一周期更新，缺点是浪费计算资源（AGC/DFE 不需要高频更新）
- **纯事件驱动**：仅在指标超限时触发更新，缺点是响应不可预测，可能导致参数发散

#### 3.3.2 Sign-LMS 算法的选择

**决策**：DFE 抽头更新默认采用 Sign-LMS 算法，可选 LMS 和 NLMS。

**原因**：
1. **硬件友好**：Sign-LMS 仅需加法运算，无需乘法器，适合硬件实现
2. **收敛稳定性**：Sign-LMS 对噪声和异常误差不敏感，鲁棒性强
3. **计算效率**：SystemC 仿真中，Sign-LMS 计算开销低，不影响仿真性能
4. **工程实践**：工业界广泛采用 Sign-LMS（如 PCIe、Ethernet SerDes）

**权衡**：
- **收敛速度**：LMS > Sign-LMS，但 Sign-LMS 在实际场景中收敛速度足够（1000-10000 UI）
- **稳态误差**：LMS < Sign-LMS，但可通过泄漏和冻结机制补偿
- **计算复杂度**：Sign-LMS << LMS << NLMS

#### 3.3.3 软饱和与钳位策略

**决策**：所有输出参数采用软饱和（钳位）策略，而非硬限幅。

**原因**：
1. **避免突变**：硬限幅会导致参数突变，引起系统瞬态响应
2. **平滑过渡**：钳位策略保证参数在合理范围内，平滑过渡
3. **稳定性**：软饱和减少参数振荡，提高系统稳定性
4. **工程实践**：实际硬件采用饱和放大器，行为级模型应反映这一特性

**实现方式**：
```cpp
// 增益钳位
gain = clamp(gain, gain_min, gain_max);

// 相位命令钳位
phase_cmd = clamp(phase_cmd, -phase_range, phase_range);

// 抽头钳位
tap[i] = clamp(tap[i], tap_min, tap_max);
```

#### 3.3.4 冻结与回退机制的设计

**决策**：提供冻结策略（误码暴涨时暂停更新）和回退策略（恢复至上次稳定快照）。

**原因**：
1. **鲁棒性**：防止算法在异常场景（信号丢失、噪声暴涨、配置错误）下发散
2. **故障恢复**：回退至上次稳定快照，快速恢复系统正常工作
3. **诊断支持**：冻结标志和快照历史用于故障诊断和调试
4. **工程实践**：实际 SerDes 芯片采用类似机制（如 PCIe 的 Link Training）

**实现要点**：
- **冻结条件**：误码暴涨、幅度异常、相位失锁
- **快照保存**：周期性保存参数历史（`snapshot_interval`）
- **回退触发**：冻结持续时间超过阈值或关键指标持续劣化
- **历史记录**：维护最近 N 次更新的参数与指标历史

#### 3.3.5 DE-TDF 桥接延迟的处理

**决策**：接受 DE→TDF 桥接的 1 个 TDF 周期延迟，算法设计考虑此延迟对稳定性的影响。

**原因**：
1. **SystemC-AMS 限制**：DE→TDF 桥接有固有延迟，无法避免
2. **稳定性设计**：通过降低更新频率和增加阻尼系数，补偿延迟影响
3. **工程实践**：实际硬件中控制环路也有延迟（ADC 转换、数字滤波器）

**补偿策略**：
- **降低更新频率**：快路径每 10-100 UI 更新一次，慢路径每 1000-10000 UI 更新一次
- **增加阻尼系数**：PI 控制器的积分系数 `Ki` 适当减小，提高稳定性
- **抗饱和策略**：防止积分器因延迟累积导致溢出

#### 3.3.6 配置驱动设计

**决策**：通过 JSON/YAML 配置文件管理所有算法参数，支持运行时场景切换与参数重载。

**原因**：
1. **灵活性**：不同通道和速率场景需要不同参数配置
2. **可维护性**：配置文件集中管理，便于修改和版本控制
3. **可扩展性**：新增参数时无需修改代码，仅需更新配置文件
4. **工程实践**：工业界广泛采用配置驱动设计（如 PCIe、Ethernet）

**实现方式**：
```cpp
// 加载配置
nlohmann::json config;
std::ifstream("config/default.json") >> config;
adaption.load_config(config);

// 场景切换
adaption.load_config("config/scene_long_channel.json");
```

---

## 行为模型

### 调度模式

#### 事件驱动（Event-Driven）
- 当输入指标（phase_error、amplitude_rms、error_count）超过门限或变化率达到阈值时，触发对应算法更新
- 优点：响应快速、计算开销低（仅在必要时更新）
- 缺点：需要精细设计门限，避免过度触发或漏触发

#### 周期驱动（Periodic）
- 按固定时间间隔（fast_update_period/slow_update_period）周期性执行估计与更新
- 优点：实现简单、时序可预测
- 缺点：可能在稳态浪费计算资源

#### 多速率（Multi-Rate）
- 快路径（高频更新）：CDR PI、阈值自适应（如每 10‑100 UI 更新一次）
- 慢路径（低频更新）：AGC、DFE 抽头（如每 1000‑10000 UI 更新一次）
- 优点：平衡性能与开销，符合实际硬件分层控制策略

### 算法要点

#### AGC（自动增益控制）
1. **幅度估计**：
   - 从 amplitude_rms 读取当前幅度（RMS/峰值/滑窗平均）
   - 计算误差：`amp_error = target_amplitude - current_amplitude`

2. **PI 控制器更新**：
   - 比例项：`P = kp * amp_error`
   - 积分项：`I += ki * amp_error * dt`
   - 输出增益：`gain = P + I`

3. **约束与钳位**：
   - 增益范围：`gain = clamp(gain, gain_min, gain_max)`
   - 速率限制：`delta_gain = clamp(gain - gain_prev, -rate_limit*dt, rate_limit*dt)`
   - 更新：`gain_out = gain_prev + delta_gain`

4. **输出**：写入 `vga_gain` 端口，下一 TDF 周期生效

#### DFE 抽头更新（LMS/Sign-LMS）
1. **误差获取**：从 error_count 或专用误差端口读取当前误差 `e(n)`

2. **LMS 更新**：
   - 对每个抽头 `i`：`tap[i] = tap[i] + mu * e(n) * x[n-i]`
   - 其中 `x[n-i]` 为延迟 `i` 的符号判决值（需从 RX 获取）

3. **Sign-LMS 简化**：
   - `tap[i] = tap[i] + mu * sign(e(n)) * sign(x[n-i])`
   - 降低乘法复杂度，适合硬件实现

4. **泄漏与约束**：
   - 泄漏：`tap[i] = (1 - leakage) * tap[i]`（防止发散）
   - 饱和：`tap[i] = clamp(tap[i], tap_min, tap_max)`

5. **冻结条件**：若 `|e(n)| > freeze_threshold`，暂停更新（避免异常噪声干扰）

6. **输出**：写入 `dfe_taps` 数组或单独端口，DFE Summer 在下一周期使用新系数

#### 阈值自适应
1. **电平分布估计**：
   - 从采样信号统计高/低电平均值与方差
   - 或使用误码趋势（error_count 变化率）

2. **阈值调整**：
   - 目标：最小化误码或最大化眼图开口
   - 策略：梯度下降或二分查找，向误差减小方向调整
   - `threshold += adapt_step * sign(gradient)`

3. **迟滞更新**：
   - 根据噪声强度动态调整迟滞窗口，平衡抗噪与灵敏度

4. **输出**：写入 `sampler_threshold` 与 `sampler_hysteresis`

#### CDR PI 控制器
1. **相位误差获取**：从 `phase_error` 端口读取（早/晚采样差值或相位检测器输出）

2. **PI 更新**：
   - 比例项：`P = kp * phase_error`
   - 积分项：`I += ki * phase_error * dt`
   - 相位命令：`phase_cmd = P + I`

3. **抗饱和（Anti-Windup）**：
   - 若 `phase_cmd` 超出 `±phase_range`，钳位并停止积分累积
   - `phase_cmd = clamp(phase_cmd, -phase_range, phase_range)`

4. **量化与分辨率**：
   - 按 `phase_resolution` 量化命令：`phase_cmd_q = round(phase_cmd / phase_resolution) * phase_resolution`

5. **输出**：写入 `phase_cmd`，相位插值器根据命令调整采样时刻

### 稳定性与回退机制

#### 冻结策略
- 检测条件：
  - 误码暴涨：`error_count > error_burst_threshold`
  - 幅度异常：`amplitude_rms` 超出预期范围
  - 相位失锁：`|phase_error|` 持续超限
- 动作：暂停所有参数更新，维持当前值，等待条件恢复

#### 回退策略
- 快照保存：每隔 `snapshot_interval` 保存一次当前参数（增益、抽头、阈值、相位）
- 触发条件：冻结持续时间超过阈值或关键指标持续劣化
- 动作：恢复至上次稳定快照参数，重新启动训练

#### 历史记录
- 维护最近 N 次更新的参数与指标历史（用于诊断与回归分析）
- 输出到 trace（update_count、freeze_flag、参数快照）

## 依赖

### SystemC 库
- **必须**：SystemC 2.3.4（DE 域基础）
- **必须**：SystemC‑AMS 2.3.4（DE‑TDF 桥接机制，`sca_de::sca_in/out`）

### 外部模块
- **RX 模块**：提供采样误差、幅度统计、判决结果
- **CDR 模块**：提供相位误差、接收相位命令
- **SystemConfiguration**：提供场景参数与初值

### 算法库（可选）
- 标准 C++ 库：`<vector>`, `<cmath>`, `<algorithm>`（用于数组、饱和、统计）
- 无需外部数学库（基础 LMS/PI 可用标准库实现）

## 时序与跨域桥接

### DE 更新周期与 TDF 对齐
- **DE 域时钟**：Adaption 模块在 DE 域运行，事件驱动或按固定周期（如每 N 个 TDF 周期触发一次）
- **TDF 采样率**：AMS 模块（CTLE/VGA/Sampler/DFE）以系统 Fs 运行，参数在每个 TDF 时间步读取
- **对齐原则**：DE 输出参数在当前事件完成后，下一 TDF 采样周期生效；避免同一 TDF 步内读写竞争

### 跨域数据同步
- **缓冲机制**：DE 输出通过 `sca_de::sca_out` 写入缓冲，TDF 模块在采样时刻读取最新值
- **时间戳标记**：对于多参数同时更新（如 CTLE 零/极/增益），确保原子性（同一时刻生效）
- **延迟处理**：DE→TDF 桥接可能有 1 个 TDF 周期延迟，算法设计需考虑此延迟对稳定性影响

### 多场景/热切换
- **场景切换事件**：通过 `scenario_switch` 或 `mode` 信号触发
- **参数原子切换**：同时更新所有相关参数，避免过渡期参数不一致
- **防抖策略**：切换后进入短暂训练期，冻结误码统计，避免瞬态误触发冻结/回退

## 数据流与追踪

### 输入数据流（RX/Channel → Adaption）
- 采样误差/相位误差：实时或周期性报告
- 幅度统计：RMS/峰值，可由 RX 内部统计模块提供或 Adaption 从原始采样计算
- 误码计数：Sampler 判决结果与理想比特序列比较（需 PRBS 同步）

### 输出参数流（Adaption → RX/CDR）
- 增益/抽头/阈值/相位命令：通过 DE‑TDF 桥接写入对应模块
- 参数更新频率：快路径（CDR/阈值）每 10‑100 UI，慢路径（AGC/DFE）每 1000‑10000 UI

### Trace 输出（用于后处理/回归）
- 记录关键信号：
  - `vga_gain(t)`、`dfe_taps(t)`、`sampler_threshold(t)`、`phase_cmd(t)`
  - `update_count(t)`、`freeze_flag(t)`、`error_count(t)`
- 使用 `sc_trace()` 或 `sca_trace()`（根据信号类型）写入 `.dat` 或 VCD
- 后处理：Python 读取 trace，绘制参数收敛曲线、更新频率分布、冻结事件时间线

### 与 Python EyeAnalyzer 协同
- Adaption 以在线控制为主，不直接调用 Python
- Python 后处理用于离线验证 Adaption 效果：
  - 对比 Adaption 开启/关闭的眼图开口、TJ、BER
  - 分析参数收敛速度与稳定性
  - 生成回归报告（眼高/眼宽改善百分比、锁定时间等）

## 使用示例

### 配置示例（JSON 片段）
```json
{
  "adaption": {
    "update_mode": "multi-rate",
    "fast_update_period": 2.5e-10,
    "slow_update_period": 2.5e-7,
    "agc": {
      "enabled": true,
      "target_amplitude": 0.4,
      "kp": 0.1,
      "ki": 100.0,
      "gain_min": 0.5,
      "gain_max": 8.0,
      "rate_limit": 10.0,
      "initial_gain": 2.0
    },
    "dfe": {
      "enabled": true,
      "num_taps": 5,
      "algorithm": "sign-lms",
      "mu": 1e-4,
      "leakage": 1e-6,
      "initial_taps": [-0.05, -0.02, 0.01, 0.005, 0.002],
      "tap_min": -0.5,
      "tap_max": 0.5,
      "freeze_threshold": 0.3
    },
    "threshold": {
      "enabled": true,
      "initial": 0.0,
      "hysteresis": 0.02,
      "adapt_step": 0.001,
      "drift_threshold": 0.05
    },
    "cdr_pi": {
      "enabled": true,
      "kp": 0.01,
      "ki": 1e-4,
      "phase_resolution": 1e-12,
      "phase_range": 5e-11,
      "anti_windup": true,
      "initial_phase": 0.0
    },
    "safety": {
      "freeze_on_error": true,
      "rollback_enable": true,
      "snapshot_interval": 1e-5,
      "error_burst_threshold": 100
    }
  }
}
```

### SystemC 实例化示例
```cpp
// 创建 Adaption 模块
AdaptionDe adaption("adaption");
adaption.load_config("config/scene_base.json");

// 连接输入（来自 RX/CDR）
adaption.phase_error(cdr.phase_error_out);
adaption.amplitude_rms(rx.amplitude_stat);
adaption.error_count(rx.error_counter);
adaption.mode(sys_config.mode);
adaption.reset(sys_config.reset);

// 连接输出（到 RX/CDR）
adaption.vga_gain(rx.vga_gain_in);
adaption.dfe_taps(rx.dfe_taps_in);
adaption.sampler_threshold(rx.sampler_threshold_in);
adaption.sampler_hysteresis(rx.sampler_hysteresis_in);
adaption.phase_cmd(cdr.phase_cmd_in);

// Trace 输出
sc_trace(tf, adaption.vga_gain, "vga_gain");
sc_trace(tf, adaption.phase_cmd, "phase_cmd");
sc_trace(tf, adaption.update_count, "update_count");
sc_trace(tf, adaption.freeze_flag, "freeze_flag");
```

### 运行流程
1. **初始化**（mode=0）：
   - 加载配置参数，初始化增益/抽头/阈值/相位命令为初值
   - 复位积分器与历史缓冲

2. **训练阶段**（mode=1）：
   - 启用所有自适应算法（AGC/DFE/阈值/CDR PI）
   - 高频更新（快路径）与低频更新（慢路径）并行运行
   - 监控冻结条件，必要时暂停更新

3. **数据阶段**（mode=2）：
   - 维持训练后的参数，或继续低频微调（可选）
   - 统计误码率与眼图指标

4. **冻结阶段**（mode=3）：
   - 停止所有参数更新，维持当前值
   - 用于诊断或切换场景前的稳定期

5. **Trace 输出**：
   - 仿真结束后生成 `results.dat`，包含参数时间序列
   - Python 后处理分析收敛曲线与 Adaption 效果

## 测试验证

### 单元测试

#### AGC 测试
- **阶跃响应**：输入幅度从 0.2V 阶跃至 0.6V，验证增益收敛至目标、无超调
- **稳态误差**：恒定输入下，增益稳定后幅度误差 < 5%
- **速率限制**：快速幅度变化时，增益变化率不超过 `rate_limit`

#### DFE 更新测试
- **ISI 场景**：典型长通道（S 参数定义），注入 PRBS31
- **收敛性**：抽头在 1000‑10000 UI 内收敛至稳定值
- **误码改善**：DFE 开启后误码率下降 > 10x
- **泄漏稳定性**：长时间运行（1e6 UI）抽头不发散

#### 阈值自适应测试
- **电平偏移**：输入信号直流偏移 ±50mV，阈值自动跟踪
- **噪声注入**：RJ sigma=5ps，阈值与迟滞自动调整，误码率最小化
- **鲁棒性**：异常噪声暴涨时不误触发极端阈值

#### CDR PI 测试
- **锁定时间**：初始相位误差 ±0.5 UI，锁定时间 < 1000 UI
- **稳态抖动**：锁定后相位误差 RMS < 0.01 UI
- **抗饱和**：大相位扰动下积分器不溢出，恢复后正常工作
- **噪声容忍度**：相位噪声注入（SJ 5MHz, 2ps），环路稳定

### 集成仿真

#### 标准链路场景
- **短通道**（S21 插损 < 5dB）：
  - AGC/DFE/阈值/CDR 联合工作
  - 眼图开口 > 80% UI，TJ@1e-12 < 0.3 UI
- **长通道**（S21 插损 > 15dB）：
  - DFE 抽头数增加至 8，收敛时间 < 10000 UI
  - 眼图开口 > 50% UI，误码率 < 1e-9

#### 串扰场景
- **强串扰**（NEXT > -30dB）：
  - 阈值自适应补偿串扰引起的电平偏移
  - DFE 抑制 FEXT 导致的 ISI
  - 眼图开口下降 < 20%（vs 无串扰）

#### 双向传输场景
- 启用 Channel 双向开关（S12/S11/S22）
- AGC 与 DFE 应对反射与回波
- 验证参数不发散、误码率在可接受范围

#### 抖动与调制组合
- PRBS31 + RJ(5ps) + SJ(5MHz, 2ps) + DJ
- CDR PI 滤除 SJ，RJ 通过阈值与 DFE 部分抑制
- TJ@1e-12 分解：RJ/DJ 比例符合预期

### 回归指标

- **眼图改善**：
  - Adaption 开启 vs 关闭（固定参数）
  - 眼高改善 > 20%，眼宽改善 > 15%
  - 开口面积改善 > 30%

- **抖动抑制**：
  - TJ@1e-12 降低 > 10%
  - RJ sigma 降低 > 5%（通过 DFE 与阈值优化）

- **锁定时间**：
  - CDR 锁定时间 < 1000 UI（95% 场景）
  - AGC 收敛时间 < 5000 UI
  - DFE 收敛时间 < 10000 UI

- **误码率**：
  - 目标 BER < 1e-12 场景下，实际 BER < 1e-11（含安全裕量）
  - 误码暴涨时冻结/回退机制正常触发，恢复时间 < 2000 UI

- **稳定性**：
  - 1e6 UI 长时间仿真，参数无发散
  - 冻结/回退事件 < 5 次（正常场景）
  - 参数更新次数符合预期（快路径 > 1000 次，慢路径 > 10 次）

## 性能与数值稳定性

### 复杂度评估
- **AGC**：O(1) 每次更新（PI 计算 + 钳位）
- **DFE**：O(num_taps) 每次更新（LMS/Sign-LMS，num_taps 通常 3‑8）
- **阈值自适应**：O(1) 每次更新（梯度估计 + 调整）
- **CDR PI**：O(1) 每次更新（PI 计算 + 量化）
- **总开销**：快路径每 10‑100 UI 更新一次，慢路径每 1000‑10000 UI 更新一次；对仿真性能影响 < 5%

### 数值稳定性
- **步长选择**：
  - AGC：kp/ki 根据环路带宽与阻尼系数设计（典型 kp=0.01‑0.1，ki=10‑1000）
  - DFE：mu 根据信号功率与噪声调整（典型 1e-5 ‑ 1e-3）
  - CDR PI：kp/ki 根据锁定时间与稳定性权衡（典型 kp=0.001‑0.1，ki=1e-5 ‑ 1e-3）

- **定点/浮点**：
  - SystemC 仿真使用 double（浮点），无溢出风险
  - 硬件实现需考虑定点量化与饱和处理（本文档仅涵盖行为模型）

- **饱和与钳位**：
  - 所有输出参数必须钳位至合理范围（gain_min/max、tap_min/max、phase_range）
  - 积分器使用抗饱和策略，防止长时间累积导致溢出

- **泄漏**：
  - DFE 抽头泄漏（1e-6 ‑ 1e-4）防止噪声累积导致发散
  - AGC/CDR 积分器可选泄漏（通常不需要，依赖饱和约束）

### 并发性与竞态
- **多输入指标合并**：
  - 若多个输入信号同时触发更新（phase_error + amplitude_rms），需定义优先级或合并策略
  - 建议：快路径（CDR/阈值）优先，慢路径（AGC/DFE）延后

- **跨域竞态**：
  - DE 写 + TDF 读：通过 SystemC‑AMS 桥接机制保证原子性，无需额外锁
  - 多参数同时更新：确保在同一 DE 事件内完成，TDF 在下一周期统一读取

## 变更历史

### v0.1（初稿，2025-10-30）
- 建立模块框架：概述、接口、参数、行为模型、依赖、使用示例、测试验证
- 定义四大算法：AGC、DFE 抽头更新、阈值自适应、CDR PI 控制器
- 提出多速率调度与冻结/回退机制
- 明确 DE‑TDF 桥接时序与跨域同步策略
- 提供 JSON 配置示例与 SystemC 实例化代码
- 定义单元测试、集成仿真与回归指标

### 后续计划
- v0.2：补充 CTLE 参数在线调整接口与策略（零/极/增益自适应）
- v0.3：增加多场景热切换详细流程与防抖策略
- v0.4：与实际 RX/CDR 模块接口联调，更新端口定义与信号名称
- v0.5：完善回归测试套件，补充边界条件与异常场景（如输入信号丢失、配置缺失）
