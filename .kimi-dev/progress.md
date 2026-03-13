# Batch 1 进度: 基础架构 - 调制格式与 Scheme 重构

## Task 1.1: 创建调制格式抽象层 ✅ 已完成

**Task Goal:** 创建 `modulation.py` 实现调制格式抽象层，支持 PAM4 和 NRZ，预留 PAM3/PAM6/PAM8 扩展。

**完成状态:**
- ✅ 测试文件: `tests/unit/test_modulation.py` (6个测试)
- ✅ 实现文件: `eye_analyzer/modulation.py`
- ✅ 所有测试通过
- ✅ 已提交: `f7066df`

---

## Task 1.2: 重构 BaseScheme 支持 modulation 参数 ✅ 已完成

**Task Goal:** 修改 `schemes/base.py`，添加 `modulation` 参数支持，使 BaseScheme 能接受字符串或 ModulationFormat 对象。

**完成状态:**
- ✅ 测试文件: `tests/unit/test_schemes_base.py` (7个测试)
- ✅ 实现修改: `eye_analyzer/schemes/base.py`
- ✅ 所有新测试通过
- ✅ 已提交: `cdb06d3`

**关键修改:**
- `__init__(self, ui, modulation='nrz', ui_bins=128, amp_bins=256)`
- 支持 `modulation='pam4'` 字符串或 `modulation=PAM4()` 对象
- 默认 'nrz' 保持向后兼容

---

## Task 1.3: 重构 GoldenCdrScheme 支持 PAM4

**Task Goal:** 修改 `schemes/golden_cdr.py`，添加 PAM4 眼图分析支持。

**Project Conventions:**
- 继承 BaseScheme（已支持 modulation）
- 针对 PAM4 计算每个眼的指标
- 保持 NRZ 兼容性

**Steps:**

### Step 1: Write the failing test
- [ ] 创建 `tests/unit/test_golden_cdr_pam4.py`
- 生成 PAM4 测试波形
- 测试 PAM4 分析返回多眼指标
- 测试 NRZ 仍然工作

### Step 2: Run test to verify it fails
- [ ] 运行测试，确认缺失方法

### Step 3: Write implementation
- [ ] 修改 `schemes/golden_cdr.py`
- 添加 PAM4 专用分析方法
- 计算每个眼的眼高/眼宽

### Step 4: Run test to verify it passes
- [ ] 运行测试，全部通过

### Step 5: Commit
- [ ] git commit

**Acceptance Criteria:**
- [ ] PAM4 分析返回 3 个眼的指标
- [ ] NRZ 分析仍然工作
- [ ] 单元测试通过

**Constraints:**
- 使用 BaseScheme.modulation 获取格式信息
- 不要重复计算逻辑
