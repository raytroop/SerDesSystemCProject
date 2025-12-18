# DFE Summer 模块小文档

级别：AMS 子模块（RX）

## 概述
DFE Summer（判决反馈均衡求和器）位于 RX 接收链的 CTLE/VGA 之后、Sampler 之前，作用是将主路径的差分信号与“基于历史判决比特生成的反馈信号”进行求和（通常为相减），从而抵消后游符号间干扰（post‑cursor ISI），增大眼图开度并降低误码率。

核心原则：
- 反馈至少延迟 1 UI（使用 b[n−1], b[n−2], …），避免零延迟代数环。
- tap_coeffs（tap 的系数）为各后游系数；init_bits 用于启动阶段预填充历史比特，保证反馈寄存器有定义的初值。
- 当 tap_coeffs 全为 0 时，DFE summer 等效为直通（v_fb=0），不会引入环路或改变主路径输出。

应用场景：高速串行链路（SERDES）RX 端的后游 ISI 取消，与 Sampler/CDR/Adaption 联合工作。
## 接口
- 端口（TDF）：
  - 输入：`sca_tdf::sca_in<double> in_p`, `sca_tdf::sca_in<double> in_n`（主路径差分）
  - 输入：`sca_tdf::sca_in<std::vector<int>> data_in`（历史判决数据数组）
    - 作用：保存采样时刻之前的历史判决比特序列，用于计算 DFE 反馈补偿。
    - 数组长度：由 `tap_coeffs` 的长度（即配置的 tap 数量 N）决定。
    - 数组内容：`data_in[0]` 为最近一次判决 b[n−1]，`data_in[1]` 为 b[n−2]，...，`data_in[N−1]` 为 b[n−N]。
    - 数据来源：通常由 Sampler 或 RX 顶层模块维护并更新，每个 UI 周期移位插入新的判决结果。
    - 类型说明：若 Sampler 在 DE 域输出，可通过 DE→TDF 桥接转换；或直接使用 TDF 域的判决结果（0/1 整数）。
  - 输出：`sca_tdf::sca_out<double> out_p`, `sca_tdf::sca_out<double> out_n`（均衡后的差分输出）
  
- 参数更新端口（DE→TDF 桥接）：
  - 输入：`sca_tdf::sca_de::sca_in<std::vector<double>> tap_coeffs_de`
    - 作用：接收来自 Adaption 模块的 DFE 抽头系数数组（对应 `adaption.md` 中的 `dfe_taps`）。
    - 生效时序：DE 域写入 `dfe_taps` 后，在下一 TDF 采样周期由本模块读取并更新内部 `tap_coeffs`。
    - 长度约束：与 `tap_coeffs` 初始配置长度一致，若长度不匹配应报错或截断/填充处理。
  - （可选）若采用标量端口形式：
    - 输入：`sca_tdf::sca_de::sca_in<double> tap1_de / tap2_de / ...`
    - 作用：与 `dfe_tap1/tap2/...` 一一对应，由上层 SystemC 连接进行组装。
- 配置键：
  - `tap_coeffs`（[double]）：后游 tap 的初始系数列表，按 k=1…N 顺序。
    - 作用：定义 DFE 的抽头数量（N = tap_coeffs.size()）和初始系数值。
    - 长度约束：`tap_coeffs` 的长度 N 决定了 `data_in` 数组的必需长度（也为 N）。
    - 动态更新：
      - 当 Adaption 未启用或未连接 `dfe_taps` 时，本模块始终使用此静态系数。
      - 当 Adaption 启用且通过 DE→TDF 端口提供 `dfe_taps` 时，运行时系数由 Adaption 输出接管，`tap_coeffs` 仅作为初始值，但 **tap 数量 N 保持不变**。
  - `tap_count`（int，只读/派生）：抽头数量，等于 `tap_coeffs.size()`。
    - 说明：该参数由 `tap_coeffs` 自动派生，用于明确指示所需的历史判决数据数量。
    - 典型值：3-9（根据信道 ISI 特性和复杂度权衡）。
  - `ui`（double，秒）：单位间隔，用于 TDF 步长与反馈更新节拍
  - `vcm_out`（double，V）：差分输出共模电压
  - `vtap`（double）：比特映射后的反馈电压缩放（单位与 CTLE/VGA 输出匹配）
  - `map_mode`（string）：比特映射模式，`"pm1"`（0→−1，1→+1）或 `"01"`（0→0，1→1）
  - `sat_min`/`sat_max`（double，V，可选）：输出限幅范围（软/硬限制）
  - `init_bits`（[double]，可选）：历史比特初始化值（长度**必须**与 `tap_coeffs.size()` 一致）
  - `enable`（bool）：模块使能，默认 true（关闭时直通）
## 参数
- `tap_coeffs`：默认 `[]` 或 `[0,...,0]`（空数组表示无 DFE，0 数组表示直通）。
  - 当所有系数为 0（且未连接 Adaption）时，DFE summer 等效直通。
  - 启用 Adaption 且连接 `dfe_taps` 时，直通/均衡状态由 Adaption 输出的系数决定（例如 Adaption 输出全 0 抽头）。
  - 长度范围：典型 3-9，由系统配置根据信道特性决定。空数组时 `data_in` 不使用（或长度为 0）。
- `tap_count`（派生）：等于 `tap_coeffs.size()`，决定 `data_in` 数组的长度需求。
- `ui`：默认 `2.5e-11`（秒），与系统 UI 保持一致
- `vcm_out`：默认 `0.0`（V），与前级输出共模一致更为稳定
- `vtap`：默认 `1.0`（线性倍数），用于把比特映射量级匹配到主路径幅度
- `map_mode`：默认 `"pm1"`（推荐，抗直流偏置更稳健）
- `sat_min/sat_max`：默认不限制；如需，建议设置为物理输出范围以抑制过补偿与噪声放大
- `init_bits`：默认按 `tap_coeffs.size()` 填充为 0（在 `pm1` 映射下表示"无反馈"）；也可用训练序列对应的 ±1 预填充。
  - **长度约束**：必须等于 `tap_coeffs.size()`，否则模块初始化时应报错或自动截断/填充。
- `enable`：默认 true；当 false 时，输出为主路径直通

单位与映射约定：
- `map_mode="pm1"` 时，0/1 → −1/+1；`vtap` 把 ±1 转换到伏特等效幅度
- `map_mode="01"` 时，0/1 → 0/1；`vtap` 把 0/1 转换到目标幅度
## 行为模型
1. 差分输入：`v_main = in_p - in_n`

2. 历史判决数据获取与反馈计算：
   - **输入验证**：
     - 读取 `data_in` 数组，长度必须等于 N（`tap_coeffs.size()`）。
     - 若长度不匹配，应报错或自动截断/零填充（具体策略由实现决定）。
   - **反馈电压计算**：
     - 对于每个 UI，使用 `data_in` 中的历史判决数据计算反馈电压：
       ```
       v_fb = Σ_{k=1..N} tap_coeffs[k-1] * map(data_in[k-1]) * vtap
       ```
       其中：
       - `data_in[0]` = b[n−1]（最近一次判决）
       - `data_in[1]` = b[n−2]
       - ...
       - `data_in[N−1]` = b[n−N]
   - **比特映射**：
     - `map_mode="pm1"` 时：0 → −1，1 → +1
     - `map_mode="01"` 时：0 → 0，1 → 1
   - **初始化阶段**（可选）：
     - 若提供了 `init_bits`，DFE Summer 可以在启动时将其作为 `data_in` 的初始值（由外部模块负责初始化 `data_in`）。
     - 若未提供，`data_in` 应由外部模块初始化为全 0 或其他默认值。

3. 求和与输出：
   - 差分均衡：`v_eq = v_main - v_fb`
   - 可选限幅：软限制 `v_eq_sat = tanh(v_eq / Vsat) * Vsat`（`Vsat = 0.5*(sat_max - sat_min)`），或硬裁剪到 `[sat_min, sat_max]`
   - 差分/共模合成：`out_p = vcm_out + 0.5*v_eq`，`out_n = vcm_out - 0.5*v_eq`

4. 历史判决数据的维护（外部职责）：
   - **重要**：DFE Summer **不负责**维护 `data_in` 数组的更新，该数组由 **RX 顶层模块或 Sampler** 负责管理。
   - **更新机制**（由外部实现）：
     - 每个 UI 周期，Sampler 产生新的判决比特 `b[n]`。
     - RX 顶层模块将 `data_in` 数组左移（或等价操作）：
       ```
       data_in[N-1] = data_in[N-2]
       data_in[N-2] = data_in[N-3]
       ...
       data_in[1] = data_in[0]
       data_in[0] = b[n]  // 新判决插入到队首
       ```
     - 在下一个 UI 周期，DFE Summer 读取更新后的 `data_in`，此时 `data_in[0]` 已经是 b[n]，但 DFE 使用它来计算 b[n+1] 的反馈，保证至少 1 UI 延迟。
   - **因果性保障**：
     - 由于 `data_in[0]` 对应 b[n−1]（上一 UI 的判决），使用它计算当前 UI n 的反馈不会形成代数环。
     - 严格禁止在当前 UI 使用当前判决 b[n] 作为反馈（零延迟环路）。

5. 零延迟环路说明：
   - 若把当前比特 `b[n]` 直接用于当前输出的反馈，会形成代数环（当前输出依赖当前比特，当前比特又依赖当前输出）
   - 后果：数值不稳定、步长缩小、仿真停滞，且物理上出现"瞬时完美抵消"的非真实行为
   - 规避：严格使用 `b[n−k] (k≥1)`，通过 `data_in` 数组机制天然保证（`data_in[0]` 最早也是 b[n−1]）

6. tap_coeffs=0 的特例：
   - 当 `tap_coeffs` 全为 0 时，`v_fb=0`，模块等效直通；`data_in` 的值不影响输出，但仍应保持有效长度以兼容后续自适应启用

7. 抽头更新路径（与 Adaption 的交互）：
   - 初始状态：
     - DFE Summer 按配置中的 `tap_coeffs` 初始化内部系数，长度为 N。
   - 在线更新（启用 Adaption 时）：
     - Adaption 模块在 DE 域根据误差 `e[n]` 与 **从 `data_in` 获取的历史判决值** 执行 LMS/Sign‑LMS 等算法，得到新的抽头系数数组（详见 `adaption.md` 中的 DFE 抽头更新部分）。
     - 新抽头通过 `sca_de::sca_out<std::vector<double>> dfe_taps` 输出，经 DE→TDF 桥接连接到 DFE Summer 的 `tap_coeffs_de` 输入端口。
     - DFE Summer 在每个 TDF 周期读取最新的 `dfe_taps`，并更新内部 `tap_coeffs`，通常在下一 UI 开始生效。
     - **长度一致性**：`dfe_taps` 的长度必须与初始 `tap_coeffs` 长度 N 一致，否则应报错或截断/填充。
   - 未启用 Adaption 或未连接 `dfe_taps` 时：
     - 内部 `tap_coeffs` 保持为配置初值或最近一次静态设置，不发生在线更新。
## 依赖
- 必须：SystemC‑AMS 2.3.x（TDF 域实现）
- 时间步：`set_timestep(ui)`，与 CDR/Sampler 的 UI 对齐
- 数值稳定性：避免代数环；`fs ≫ 链路最高特征频率`，经验 ≥ 20–50×
- 互联建议：前级 CTLE/VGA 推荐使用 `sca_tdf::sca_ltf_nd` 等线性滤波器
- 互联建议（`data_in` 数组的管理）：
  - `data_in` 数组应由 RX 顶层模块或专用的"历史判决队列管理器"维护。
  - 推荐实现方式：
    - 在 RX 顶层模块中维护一个长度为 N 的循环队列或移位寄存器。
    - 每个 UI 周期，Sampler 输出新判决 `b[n]`，队列左移并将 `b[n]` 插入队首。
    - DFE Summer 从队列读取 `data_in`（可能需要复制或引用传递，视 SystemC‑AMS 实现而定）。
  - 若 Sampler 在 DE 域输出比特，可通过 DE→TDF 桥接转换后再写入队列。
  - 初始化：系统启动时，队列应预填充 `init_bits`（若提供）或全 0。
- 互联建议（与 Adaption 的 tap 更新）：
  - Adaption 模块通过 `sca_de::sca_out<std::vector<double>> dfe_taps` 输出 DFE 抽头系数。
  - DFE Summer 通过 `sca_tdf::sca_de::sca_in<std::vector<double>> tap_coeffs_de` 读取 `dfe_taps`，利用 SystemC‑AMS 提供的 DE‑TDF 桥接机制自动完成跨域同步。
  - 按照 `adaption.md` 的约定，DE 域更新在当前事件完成后，抽头在下一 TDF 采样周期生效，避免同一时间步内的读写竞争。
- 规范：参数开关（如 `enable`）需明确控制；限幅与映射需与系统单位一致
## 使用示例

1. 基本直通（DFE 关闭或 `tap_coeffs=0`）：
   - 配置：`enable=true`，`tap_coeffs=[0,0,0]`（3 个 tap 全 0），`ui=2.5e-11`，`vcm_out` 与前级一致
   - 连接：
     - CTLE/VGA → `in_p/in_n`
     - RX 顶层维护长度为 3 的 `data_in` 数组（初始化为 `[0,0,0]` 或 `init_bits`）
     - DFE Summer 读取 `data_in`，但因 `tap_coeffs` 全 0，`v_fb=0`，输出等效直通
     - 输出 → Sampler 前级
   - 若未连接 Adaption 的 `dfe_taps` 端口，可仅依赖 `tap_coeffs` 配置实现静态直通。

2. 开启 3‑tap DFE：
   - 配置：`tap_coeffs=[0.05, 0.03, 0.02]`，`map_mode="pm1"`，`vtap=1.0`，`init_bits=[0,0,0]`
   - 连接：
     - CTLE/VGA → `in_p/in_n`
     - RX 顶层维护长度为 3 的 `data_in` 数组，初始化为 `[0,0,0]`（对应 `init_bits`）
     - 每个 UI 周期：
       1. Sampler 产生新判决 `b[n]`（0 或 1）
       2. RX 顶层更新 `data_in`：`data_in = [b[n], data_in[0], data_in[1]]`（丢弃 `data_in[2]`）
       3. DFE Summer 读取 `data_in`，计算 `v_fb = 0.05*map(data_in[0]) + 0.03*map(data_in[1]) + 0.02*map(data_in[2])`
     - 输出 → Sampler 前级
   - 如需限幅：设置 `sat_min/sat_max` 到物理范围（例如 `0.0/1.2`）
   - 若使用 Adaption 在线更新，可将上述初始 `tap_coeffs` 作为 `initial_taps`，后续抽头由 Adaption 通过 `dfe_taps` 动态调整（但 tap 数量保持为 3）。

3. 与自适应联动（推荐）：
   - Adaption 模块在 DE 域执行以下流程：
     1. 从误差端口读取误差 `e[n]`
     2. 从 `data_in` 数组读取历史判决值 `b[n−1], b[n−2], ..., b[n−N]`（或通过桥接从 DFE Summer 获取）
     3. 执行 DFE 抽头更新算法（例如 Sign‑LMS：`c_k ← c_k + μ·sign(e[n])·sign(b[n−k])`）
     4. 将更新后的抽头数组写入 `dfe_taps` 端口（参见 `adaption.md` 中 "DFE 抽头系数（到 DFE Summer）"）
   - DFE Summer 通过 `tap_coeffs_de`（或等价的 DE→TDF 端口）接收 `dfe_taps`，在下一 UI 周期将 `tap_coeffs` 更新为新值。
   - DFE Summer 本身不内置自适应算法，职责仅是：
     - 读取 `data_in` 数组和当前 `tap_coeffs`
     - 计算反馈电压 `v_fb = Σ tap_coeffs[k-1] * map(data_in[k-1]) * vtap`
     - 将 `v_fb` 与主路径差分 `v_main` 求和并输出均衡结果
   - **数据流向总结**：
     ```
     Sampler → 更新 data_in 数组 → DFE Summer 读取 data_in + tap_coeffs 计算 v_fb
                                    ↑
                                    |
                          Adaption 读取 data_in + 误差 e[n] → 更新 tap_coeffs → tap_coeffs_de
     ```

4. 9‑tap DFE 大规模配置示例：
   - 配置：`tap_coeffs=[0.10, 0.08, 0.06, 0.04, 0.03, 0.02, 0.015, 0.01, 0.005]`（9 个 tap）
   - `data_in` 数组长度为 9，由 RX 顶层维护
   - 反馈计算：`v_fb = Σ_{k=1..9} tap_coeffs[k-1] * map(data_in[k-1]) * vtap`
   - 适用场景：长信道、严重 ISI，需要更多 tap 进行补偿
## 测试验证
- 直通一致性：`tap_coeffs=0` 或 `enable=false` 时，`out_p/out_n` 与主路径差分一致（考虑共模合成）
- 因果性验证：检查反馈是否至少延迟 1 UI（波形对齐或在代码中查看更新顺序）
- 眼图开度：对比 `taps=0` 与 `taps>0` 情况下的眼图，验证后游 ISI 取消效果
- 限幅行为：当设置 `sat_min/sat_max`，输出应在范围内软/硬限制，避免过补偿
- init_bits 影响：在不同初始填充下启动瞒态的差异（0 填充稳码；训练序列填充更快收敛）
- 抗零延迟环路：故意将当前比特用于反馈（仅测试环境），应观察到数值问题或不合理行为；恢复因果更新后应正常
- BER 评估：在 PRBS 输入下，统计误码率变化；验证 DFE 后的 BER 下降
- `data_in` 数组验证：
  - 长度一致性：确保 `data_in.size()` == `tap_coeffs.size()`，否则应报错或自动处理
  - 因果性验证：检查 `data_in[0]` 确实对应 b[n−1]（不是 b[n]），可通过波形对齐或日志确认
  - 队列更新正确性：在连续 UI 周期中，验证 `data_in` 数组正确移位（旧值向右移，新值插入左侧）
  - 边界条件：测试 tap 数量为 1、3、9 等不同配置下 `data_in` 的行为
## 变更历史
- v0.4（2025-12-18）：重新引入并改进 `data_in` 接口，实现配置驱动的历史判决数据数组机制：
  - 将 `data_in` 从单比特改为 `std::vector<int>` 数组，长度由 `tap_coeffs.size()` 决定。
  - 明确 `data_in` 数组的索引顺序（`data_in[0]` = b[n−1]）、长度约束和数据来源。
  - 强调 DFE Summer 不负责维护 `data_in`，由 RX 顶层模块或 Sampler 负责管理队列更新。
  - 补充详细的行为模型描述，包括数组验证、反馈计算公式和外部更新机制。
  - 更新使用示例，展示不同 tap 数量下的 `data_in` 管理方式和与 Adaption 的协同流程。
  - 新增 `data_in` 相关的测试验证要点。
  - 新增 `tap_count` 派生参数说明，明确 tap 数量与 `data_in` 长度的关系。
- v0.3（2025-12-18）：对 DFE Summer 与 Adaption 之间的数据交互进行补充和收敛：
  - 移除显式判决比特输入端口 `data_in`，历史符号由 RX 内部提供；
  - 新增 DE→TDF 抽头更新端口（接收 Adaption 的 `dfe_taps` 输出），并在行为模型中描述抽头在线更新路径；
  - 明确 `tap_coeffs` 在配置中作为初始抽头，运行时可被 Adaption 输出覆盖；
  - 更新使用示例和互联建议，使其与 `adaption.md` 一致。
- v0.2（2025-10-22）：将配置键 `taps` 重命名为 `tap_coeffs`，并明确"DFE 求和依据 tap_coeffs（tap 的系数）进行"，同步更新行为模型、示例与测试描述。
- v0.1（2025-10-22）：首次完整文档；明确零延迟环路的风险与规避、`init_bits` 的作用与设置、`taps=0` 的直通特性；补充接口/参数/行为模型/测试方案与依赖说明。
