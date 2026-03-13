# Batch 1 进度: 基础架构 - 调制格式与 Scheme 重构

## Task 1.1: 创建调制格式抽象层

**Task Goal:** 创建 `modulation.py` 实现调制格式抽象层，支持 PAM4 和 NRZ，预留 PAM3/PAM6/PAM8 扩展。

**Project Conventions:**
- Python 3.8+ 类型注解
- NumPy 数组操作
- 抽象基类模式

**Steps:**

### Step 1: Write the failing test
- [x] 创建 `tests/unit/test_modulation.py`
- 测试 PAM4 基本属性
- 测试 NRZ 基本属性  
- 测试工厂函数

### Step 2: Run test to verify it fails
- [ ] 运行测试，确认 ModuleNotFoundError

### Step 3: Write minimal implementation
- [ ] 创建 `eye_analyzer/modulation.py`
- 实现 ModulationFormat ABC
- 实现 PAM4 类
- 实现 NRZ 类
- 实现工厂函数

### Step 4: Run test to verify it passes
- [ ] 运行测试，全部通过

### Step 5: Commit
- [ ] git commit

**Acceptance Criteria:**
- [ ] PAM4 和 NRZ 类通过所有单元测试
- [ ] 工厂函数 `create_modulation()` 工作正常
- [ ] 代码符合项目风格

**Constraints:**
- 使用 Python 类型注解
- 遵循 NumPy 数组操作规范
- 保持与现有代码风格一致
