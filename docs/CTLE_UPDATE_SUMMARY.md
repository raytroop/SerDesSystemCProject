# CTLE 模块更新总结

## 更新日期
2025-11-23

## 修改文件
1. `/include/common/parameters.h` - RxCtleParams 结构体
2. `/include/ams/rx_ctle.h` - RxCtleTdf 类声明
3. `/src/ams/rx_ctle.cpp` - RxCtleTdf 类实现

## 主要变更

### 1. 参数结构体扩展 (parameters.h)

#### RxCtleParams 新增字段：
- **差分输出共模电压**：
  - `vcm_out` (double): 差分输出共模电压，默认 0.6V

- **输入偏移**：
  - `vos` (double): 输入偏移电压，默认 0.0V

- **噪声参数**：
  - `vnoise_sigma` (double): 噪声标准差，默认 0.0V

- **饱和限制**：
  - `sat_min` (double): 输出最小电压，默认 -0.5V
  - `sat_max` (double): 输出最大电压，默认 0.5V

- **PSRR (电源抑制比) 子结构**：
  ```cpp
  struct PsrrParams {
      bool enable;                  // 启用开关，默认 false
      double gain;                  // PSRR 路径增益，默认 0.0
      std::vector<double> zeros;    // 零点频率 (Hz)
      std::vector<double> poles;    // 极点频率 (Hz)
      double vdd_nom;               // 名义电源电压，默认 1.0V
  }
  ```

- **CMFB (共模反馈) 子结构**：
  ```cpp
  struct CmfbParams {
      bool enable;                  // 启用开关，默认 false
      double bandwidth;             // 环路带宽，默认 1e6 Hz
      double loop_gain;             // 环路增益，默认 1.0
  }
  ```

- **CMRR (共模抑制比) 子结构**：
  ```cpp
  struct CmrrParams {
      bool enable;                  // 启用开关，默认 false
      double gain;                  // CM->DIFF 泄漏增益，默认 0.0
      std::vector<double> zeros;    // 零点频率 (Hz)
      std::vector<double> poles;    // 极点频率 (Hz)
  }
  ```

### 2. 头文件更新 (rx_ctle.h)

#### 接口变更：
- **原设计**：单端输入/输出 (`in`, `out`)
- **新设计**：差分输入/输出
  - 输入：`in_p`, `in_n` (差分信号)
  - 输出：`out_p`, `out_n` (差分信号)
  - 可选输入：`vdd` (电源电压，用于 PSRR)

#### 新增成员变量：
- 传递函数滤波器指针：
  - `m_H_ctle`: 主 CTLE 滤波器
  - `m_H_psrr`: PSRR 路径滤波器
  - `m_H_cmrr`: CMRR 路径滤波器
  - `m_H_cmfb`: CMFB 环路滤波器

- 内部状态：
  - `m_vcm_prev`: 前一周期共模输出
  - `m_out_p_prev`, `m_out_n_prev`: 前一周期输出(用于 CMFB 测量)

- 随机数生成器：
  - `m_rng`: Mersenne Twister 随机数生成器
  - `m_noise_dist`: 高斯噪声分布

#### 新增方法：
- `initialize()`: 初始化滤波器和内部状态
- `build_transfer_function()`: 从零极点构建传递函数
- `apply_saturation()`: 应用软饱和限制 (tanh)

### 3. 实现文件更新 (rx_ctle.cpp)

#### 核心处理流程 (processing())：

1. **读取差分和共模输入**：
   - `vin_diff = in_p - in_n`
   - `vin_cm = 0.5 * (in_p + in_n)`

2. **添加偏移** (可选)：
   - 如果 `offset_enable=true`，添加 `vos` 到差分信号

3. **注入噪声** (可选)：
   - 如果 `noise_enable=true`，添加高斯噪声

4. **主 CTLE 滤波**：
   - 使用 `m_H_ctle` (ltf_nd) 滤波差分信号
   - 如果无滤波器，则应用简单的 DC 增益

5. **软饱和限制**：
   - 使用 `tanh(x/Vsat) * Vsat` 实现软限制
   - `Vsat = 0.5 * (sat_max - sat_min)`

6. **PSRR 路径** (可选)：
   - 计算电源纹波：`vdd_ripple = vdd - vdd_nom`
   - 通过 `m_H_psrr` 滤波
   - 添加到差分输出

7. **CMRR 路径** (可选)：
   - 输入共模信号通过 `m_H_cmrr` 滤波
   - 添加到差分输出（模拟共模泄漏）

8. **差分输出合成**：
   - `vout_total_diff = vout_diff_sat + vout_psrr_diff + vout_cmrr_diff`

9. **共模与 CMFB** (可选)：
   - 无 CMFB：`vcm_eff = vcm_out`
   - 有 CMFB：
     - 测量前一周期共模：`vcm_meas = 0.5 * (out_p_prev + out_n_prev)`
     - 计算误差：`e_cm = vcm_out - vcm_meas`
     - 通过环路滤波器：`delta_vcm = m_H_cmfb->estimate_nd(t, e_cm)`
     - 有效共模：`vcm_eff = vcm_out + delta_vcm`

10. **生成差分输出**：
    - `out_p = vcm_eff + 0.5 * vout_total_diff`
    - `out_n = vcm_eff - 0.5 * vout_total_diff`

11. **更新内部状态**：
    - 保存当前输出用于下一周期的 CMFB

#### 传递函数构建 (build_transfer_function())：
- 从零点和极点频率列表构建 SystemC-AMS `ltf_nd` 对象
- 形式：`H(s) = dc_gain * ∏(s/ωz + 1) / ∏(s/ωp + 1)`
- 使用卷积方法逐步构建分子和分母多项式

#### 软饱和函数 (apply_saturation())：
- 实现：`y = tanh(x/Vsat) * Vsat`
- 当 `Vsat <= 0` 时不进行饱和处理

## 与设计文档的对应关系

| 文档章节 | 实现位置 |
|---------|---------|
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
| 行为模型 - 8个步骤 | `processing()`: Steps 1-11 |

## 技术要点

### 1. 避免代数环
- CMFB 使用前一周期的输出进行测量 (`m_out_p_prev`, `m_out_n_prev`)
- 这避免了当前周期输出依赖于当前周期输入的代数环

### 2. 多零点/多极点传递函数
- 使用动态构建的 `sca_ltf_nd` 对象
- 支持任意数量的零点和极点
- 自动处理多项式卷积

### 3. 软饱和
- 使用 `tanh` 函数实现平滑的饱和特性
- 避免硬限幅导致的谐波失真

### 4. 可选功能
- PSRR、CMFB、CMRR 均可独立启用/禁用
- 通过参数的 `enable` 标志控制
- 未启用时不创建对应的滤波器对象

## 注意事项

1. **时间步设置**：
   - `set_attributes()` 中设置了默认时间步 `1.0 / 100e9` (10ps)
   - 对于高速 SerDes (>10GHz)，需根据最高极点频率调整

2. **滤波器稳定性**：
   - 零极点配置需保证滤波器稳定性
   - 采样率应远高于最高极点频率 (建议 ≥20-50×)

3. **VDD 端口**：
   - 如果不使用 PSRR 功能，`vdd` 端口仍需连接（可连接到常数源）
   - SystemC-AMS 要求所有端口都必须连接

4. **内存管理**：
   - `build_transfer_function()` 返回的指针使用 `new` 分配
   - 需要在析构函数中释放（当前未实现，应添加）

## 建议的后续工作

1. **添加析构函数**：
   ```cpp
   ~RxCtleTdf() {
       if (m_H_ctle) delete m_H_ctle;
       if (m_H_psrr) delete m_H_psrr;
       if (m_H_cmrr) delete m_H_cmrr;
       if (m_H_cmfb) delete m_H_cmfb;
   }
   ```

2. **参数验证**：
   - 在构造函数中验证参数有效性
   - 检查 `sat_min < sat_max`
   - 检查增益和带宽的合理范围

3. **测试用例**：
   - 频响测试（Bode 图）
   - PSRR 测试（电源纹波抑制）
   - CMFB 测试（共模阶跃响应）
   - CMRR 测试（共模抑制）
   - 饱和测试（大信号输入）

4. **配置文件示例**：
   - 更新 `config/default.json` 和 `default.yaml`
   - 包含完整的 CTLE 参数示例

## 编译注意事项

- 需要 SystemC-2.3.4 和 SystemC-AMS-2.3.4
- C++14 标准
- 使用 `<random>` 头文件（C++11特性）
- 使用 `<cmath>` 中的 `tanh` 函数

## 兼容性

- **向后兼容性**：破坏性更改
  - 原有的单端接口 (`in`, `out`) 已移除
  - 需要修改使用 CTLE 的测试平台代码
  
- **配置兼容性**：部分兼容
  - 原有参数 (`zeros`, `poles`, `dc_gain`) 保持不变
  - 新增参数有合理的默认值

## 参考文档
- `docs/modules/ctle.md` - CTLE 模块完整设计规范
