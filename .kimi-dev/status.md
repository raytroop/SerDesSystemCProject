# Eye Analyzer PAM4 + BER + JTol 增强

## Goal
将 pystateye 的 PAM4 分析能力、统计眼图能力整合到 eye_analyzer，并新增完整的 BER 分析和 Jitter Tolerance 测试功能，打造支持从预仿真到后仿真全流程的 SerDes 信号完整性分析平台。

## Context
- **开发周期**: 4 周
- **技术方案**: 模块化重构 + 扩展架构（方案 A+）
- **向后兼容**: 不考虑

## Tech Stack
- Python 3.8+
- NumPy/SciPy
- Matplotlib/Seaborn
- PyYAML

## Requirements

### R1: 可扩展调制格式架构
- PAM4 标准差分电平（-3, -1, +1, +3）
- 架构预留 PAM3/PAM6/PAM8 扩展
- Strategy Pattern + Registry 模式

### R2: 双模式分析
- **统计眼图（前仿真）**: 脉冲响应 → ISI PDF → 噪声/抖动注入 → BER
- **经验眼图（后仿真）**: 时域波形分析

### R3: 完整 Jitter 分析
- 时域：PAM4 三眼分别提取
- PDF：Dual-Dirac 建模
- 预算：RJ/DJ/TJ 分解

### R4: 完整 BER 分析（6大功能）
1. BER Contour 等高线
2. Target BER 眼图指标
3. Bathtub Curve（时间+电压方向）
4. 每眼独立 BER
5. 理论 vs 实际 BER 对比
6. Q 因子与 BER 转换

### R5: Jitter Tolerance 测试
- 标准模板：IEEE 802.3ck, OIF-CEI, JEDEC, PCIe
- 自定义扫描：频率/幅度/精度可调
- 对比图 + Pass/Fail 判定

### R6: 统一接口
```python
analyzer = EyeAnalyzer(
    ui=2.5e-11,
    modulation='pam4',      # 'nrz' | 'pam4' | ...
    mode='statistical'       # 'statistical' | 'empirical'
)
```

## Scenarios

### Scenario 1: PAM4 统计眼图分析
- **Given**: 脉冲响应数据 + 噪声/抖动参数
- **When**: 调用 `analyzer.analyze(pulse_response=..., noise_sigma=..., jitter_dj=...)`
- **Then**: 返回眼图矩阵 + BER 等高线 + COM + 每眼指标

### Scenario 2: PAM4 经验眼图分析
- **Given**: 时域波形数据
- **When**: 调用 `analyzer.analyze(waveform=...)`
- **Then**: 返回眼图指标 + Jitter 分解

### Scenario 3: Jitter Tolerance 测试
- **Given**: 多组 SJ 条件下的波形数据
- **When**: 调用 `analyzer.jtol_analyze(waveforms=..., template='ieee_802_3ck')`
- **Then**: 返回 JTol 曲线 + Pass/Fail 判定 + 对比图

### Scenario 4: 标准模板对比
- **Given**: 实测 JTol 曲线 + 标准模板名称
- **When**: 生成对比报告
- **Then**: 显示实测 vs 模板曲线，标注裕量

## Subtasks（批次划分）

### Batch 1: 基础架构（Week 1）
- [ ] 创建 `modulation.py` - 调制格式抽象层
- [ ] 重构 `schemes/base.py` - 支持 modulation 参数
- [ ] 重构 `schemes/golden_cdr.py` - 支持 PAM4
- [ ] 重构 `schemes/sampler_centric.py` - 支持 PAM4

### Batch 2: 统计眼图核心（Week 1-2）
- [ ] 创建 `statistical/` 子包
- [ ] 实现 `pulse_response.py` - 脉冲响应预处理
- [ ] 实现 `isi_calculator.py` - ISI PDF 计算
- [ ] 实现 `noise_injector.py` - 噪声注入
- [ ] 实现 `jitter_injector.py` - 抖动注入
- [ ] 实现 `ber_calculator.py` - BER 等高线
- [ ] 创建 `schemes/statistical.py` - 统计眼图方案

### Batch 3: BER 分析模块（Week 2-3）
- [ ] 创建 `ber/` 子包
- [ ] 实现 `contour.py` - BER Contour
- [ ] 实现 `bathtub.py` - Bathtub Curve
- [ ] 实现 `qfactor.py` - Q 因子转换
- [ ] 实现 `template.py` - 标准模板定义
- [ ] 实现 `jtol.py` - Jitter Tolerance
- [ ] 实现 `__init__.py` - BERAnalyzer 统一入口

### Batch 4: Jitter 重构与可视化（Week 3）
- [ ] 重构 `jitter.py` - 支持多眼 Jitter 提取
- [ ] 重构 `visualization.py` - 支持 PAM4 叠加显示
- [ ] 重构 `analyzer.py` - 新版统一入口

### Batch 5: 集成与测试（Week 4）
- [ ] 更新 `__init__.py` - 导出新接口
- [ ] 编写单元测试
- [ ] 与 pystateye 结果对比验证
- [ ] 文档更新

## Progress Tracking

| Batch | Status | Start | End | Notes |
|-------|--------|-------|-----|-------|
| 1 | Not Started | - | - | - |
| 2 | Not Started | - | - | - |
| 3 | Not Started | - | - | - |
| 4 | Not Started | - | - | - |
| 5 | Not Started | - | - | - |

## References
- Design Doc: `docs/plans/2025-03-12-eye-analyzer-pam4-ber-jtol-design.md`
- Tech Spec: `/home/yzliu/.kimi/plans/jade-black-panther-kamala-khan.md`
- pystateye: `/mnt/d/systemCProjects/pystateye/statistical_eye.py`
