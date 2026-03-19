# Scheme C 验证报告：C++ PoleResidueFilter 与 scikit-rf VectorFitting

## 执行日期
2026-03-18

## 目标
验证 C++ PoleResidueFilter 实现是否满足频率响应相关性 > 0.95 的要求。

## 测试方法
1. **C++ 实现正确性验证**：对比 C++ 计算结果与 scikit-rf 理论计算
2. **与原始 S4P 对比**：验证拟合质量

## 结果摘要

### 1. C++ 实现正确性 ✓ PASS

| 指标 | 值 | 阈值 | 状态 |
|------|-----|------|------|
| 幅度相关性 (线性) | 1.000000 | > 0.95 | ✓ PASS |
| 幅度相关性 (dB) | 1.000000 | > 0.95 | ✓ PASS |
| 相位相关性 | 1.000000 | > 0.95 | ✓ PASS |
| RMSE (dB) | 0.0000 | < 3 | ✓ PASS |
| 最大误差 (dB) | 0.0000 | < 6 | ✓ PASS |

**结论**：C++ PoleResidueFilter 实现与理论计算完全一致，实现正确。

### 2. 与原始 S4P 对比 ✗ FAIL

| 指标 | 值 | 阈值 | 状态 |
|------|-----|------|------|
| 相关性 | 0.960921 | > 0.95 | ✓ PASS |
| RMSE (dB) | 28.0049 | < 3 | ✗ FAIL |
| 最大误差 (dB) | 45.6125 | < 6 | ✗ FAIL |

**根本原因分析**：

1. **DC 增益严重偏离**：
   - C++ / scikit-rf 输出：+25.90 dB
   - 原始 S4P：-0.58 dB
   - 偏差：26.48 dB

2. **复数 DC 增益问题**：
   ```
   DC gain = 0.622 - 19.697j
   ```
   物理系统应有实值 DC 增益，复数 DC 增益表明矢量拟合未正确收敛。

3. **极点和留数无共轭对称性**：
   - 32 个极点全部为复数
   - 无共轭对（应为 16 对共轭极点）
   - 留数也未形成共轭对

## 根本原因

**问题不在 C++ 实现，而在于 scikit-rf VectorFitting 导出的 pole-residue 配置。**

scikit-rf 的 `VectorFitting` 类：
- 拟合过程产生内部收敛警告（`max_iterations` 达到上限）
- 返回的极点和留数未强制共轭对称性
- 导致 DC 增益为复数且偏离正确值

## 对比其他方法

| 方法 | 相关性 | RMSE (dB) | DC 增益 (dB) | 状态 |
|------|--------|-----------|--------------|------|
| scikit-rf PR (32 极) | 0.96 | 28.0 | +25.9 | ✗ 失败 |
| Rational (8 阶) | 0.54 | 22.8 | -1.5 | ✗ 失败 |

## 结论

### C++ 实现评估：✓ 满足要求

C++ `PoleResidueFilter` 实现：
- 计算正确（与理论值相关性 1.000000）
- 数值稳定（级联二阶状态空间）
- 通过所有单元测试（阶跃响应、频率响应、复极点、能量守恒）

**C++ 实现本身满足需求，问题在于输入数据质量。**

### 建议

1. **修复矢量拟合配置**：
   - 使用专业工具（如 MATLAB `rationalfit` 或 BetterVectorFitting）
   - 或手动后处理强制共轭对称性
   - 增加迭代次数和收敛检查

2. **验证流程改进**：
   ```python
   # 在导出 pole-residue 前检查
   dc_gain_complex = sum(r / (-p) for r, p in zip(residues, poles)) + constant
   assert abs(dc_gain_complex.imag) < 1e-6, "DC gain must be real"
   ```

3. **替代方案**：
   - 使用其他矢量拟合库（如 `vectorfitting` Python 包）
   - 或直接使用有理函数形式（num/den）而非 pole-residue

## 下一步行动

选项 A：修复 scikit-rf 矢量拟合，重新导出配置
选项 B：接受 C++ 实现正确，但降级为仅支持测试/演示用途
选项 C：实现有理函数形式（直接 num/den 多项式）

---

**签名**：Kimi Code CLI  
**状态**：C++ 核心验证完成，等待 pole-residue 配置修复
