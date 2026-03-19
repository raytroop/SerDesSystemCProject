# Vector Fitting 替代方案调研报告

## 问题总结

scikit-rf 的 VectorFitting 存在以下问题：
1. 迭代不收敛（达到 max_iterations 上限）
2. 返回的极点/留数不保证共轭对称
3. DC 增益为复数（物理上无效）
4. 拟合质量差（相关性 ~0.96 但 RMSE 28dB）

---

## 替代方案对比

### 方案 1: 修复 scikit-rf 输出（推荐短期方案）

**思路**: 对 scikit-rf 的输出进行后处理，强制共轭对称。

```python
def enforce_conjugate_symmetry(poles, residues, constant):
    """
    强制 pole-residue 表示具有共轭对称性。
    对于物理系统，H(s) 必须满足 H(s*) = H*(s)。
    """
    # 只保留上半平面极点
    unique_poles = []
    unique_residues = []
    
    for p, r in zip(poles, residues):
        if p.imag >= 0:  # 只保留正频率极点
            unique_poles.append(p)
            # 调整留数以获得实值 DC 增益
            unique_residues.append(r)
    
    # 重新计算常数项以获得正确的 DC 增益
    # ... 优化代码 ...
    
    return unique_poles, unique_residues, constant
```

**优点**:
- 无需引入新依赖
- 实施快速

**缺点**:
- 治标不治本
- 需要额外的优化步骤

---

### 方案 2: 使用 SciPy 有理函数拟合

**思路**: 使用 `scipy.interpolate` 或 `scipy.optimize` 进行有理函数拟合。

```python
from scipy.optimize import least_squares
import numpy as np

def rational_fit(freq, H, n_poles):
    """
    直接优化 pole-residue 参数。
    """
    def error_function(params):
        poles = params[:n_poles] + 1j * params[n_poles:2*n_poles]
        residues = params[2*n_poles:3*n_poles] + 1j * params[3*n_poles:4*n_poles]
        constant = params[-2]
        proportional = params[-1]
        
        s = 1j * 2 * np.pi * freq
        H_fit = np.zeros_like(s, dtype=complex)
        for p, r in zip(poles, residues):
            H_fit += r / (s - p)
        H_fit += constant + proportional * s
        
        return np.concatenate([np.real(H - H_fit), np.imag(H - H_fit)])
    
    # 初始猜测（来自 scikit-rf 或均匀分布）
    x0 = initialize_poles(freq, H, n_poles)
    
    result = least_squares(error_function, x0, method='lm')
    return result.x
```

**优点**:
- 依赖少（只需 SciPy）
- 可以添加约束（如共轭对称）

**缺点**:
- 需要良好的初始猜测
- 可能陷入局部最优

---

### 方案 3: 使用 VFdriver（原始 Fortran 代码）

**来源**: https://www.sintef.no/projectweb/vectorfitting/

**思路**: Gustavsen 的原始 VectorFitting 实现（Fortran 90）。

**使用方法**:
1. 下载 Fortran 源码
2. 编译为共享库或使用 f2py 包装
3. 在 Python 中调用

```bash
# 下载 VFdriver
wget https://www.sintef.no/contentassets/1e9f55a5a5f74c8d9d5c8b8a6f8b6d7c/vfdriver.zip
unzip vfdriver.zip
cd VFdriver
make
```

```python
# Python 调用（需要 f2py 包装）
import subprocess
import numpy as np

def vector_fit_fortran(freq, H, n_poles):
    # 写入输入文件
    np.savetxt('input.txt', np.column_stack([freq, H.real, H.imag]))
    
    # 调用 Fortran 程序
    subprocess.run(['./VFdriver', str(n_poles)])
    
    # 读取输出文件
    poles = np.loadtxt('poles.txt')
    residues = np.loadtxt('residues.txt')
    return poles, residues
```

**优点**:
- 最权威的实现
- 经过广泛验证
- 支持 Relaxed VF 和 Fast VF

**缺点**:
- 需要 Fortran 编译器
- 调用复杂

---

### 方案 4: Rational Function Approximation（基于 SciPy）

**思路**: 使用 SciPy 的 `signal.residue` 相关功能。

```python
from scipy import signal
import numpy as np

def rational_approximation_scipy(freq, H, order):
    """
    使用 SciPy 进行有理函数近似。
    注意：这是频率域拟合，不是 VectorFitting。
    """
    s = 1j * 2 * np.pi * freq
    
    # 使用多项式拟合作为起点
    # H(s) ≈ (b_n*s^n + ... + b_0) / (a_m*s^m + ... + a_0)
    
    # 转换为 state-space 然后降阶
    # 或使用 Pade 近似
    
    return num, den  # 分子和分母多项式系数
```

**优点**:
- 纯 Python/SciPy
- 易于集成

**缺点**:
- 不是标准的 VectorFitting
- 可能不适用于散射参数

---

### 方案 5: Loewner 框架（推荐长期方案）

**思路**: 基于数据驱动的 Loewner 矩阵方法。

```python
def loewner_framework(freq, H, order):
    """
    Loewner 框架有理插值。
    参考文献: Antoulas, "Approximation of Large-Scale Dynamical Systems"
    """
    # 分割数据为左右子集
    n = len(freq)
    left_idx = slice(0, n, 2)
    right_idx = slice(1, n, 2)
    
    s_left = 1j * 2 * np.pi * freq[left_idx]
    s_right = 1j * 2 * np.pi * freq[right_idx]
    H_left = H[left_idx]
    H_right = H[right_idx]
    
    # 构建 Loewner 和 shifted Loewner 矩阵
    L = np.zeros((len(s_left), len(s_right)), dtype=complex)
    Ls = np.zeros_like(L)
    
    for i, (si, Hi) in enumerate(zip(s_left, H_left)):
        for j, (sj, Hj) in enumerate(zip(s_right, H_right)):
            L[i, j] = (Hi - Hj) / (si - sj)
            Ls[i, j] = (si * Hi - sj * Hj) / (si - sj)
    
    # SVD 降阶
    U, S, Vh = np.linalg.svd(L)
    
    # 提取降阶后的系统矩阵
    # ... 实现细节 ...
    
    return poles, residues
```

**优点**:
- 无需迭代，直接求解
- 数学上优雅
- 适合大规模数据

**缺点**:
- 实现复杂
- 对噪声敏感
- 需要完整的代码实现

---

## 推荐方案

### 短期（1-2 天）

**使用方案 1 + 手动调整**: 修复现有的 scikit-rf 输出

```python
# 关键修复步骤
1. 检查 DC 增益是否为实数
2. 强制极点/留数共轭对称
3. 重新计算 constant 以匹配 DC 点
4. 验证拟合质量
```

### 中期（1 周）

**使用方案 3**: 集成 VFdriver (Fortran)

- 下载并编译 Gustavsen 的原始代码
- 创建 Python 包装器
- 替代 scikit-rf

### 长期（2-4 周）

**使用方案 5**: 实现 Loewner 框架

- 更稳定的数据驱动方法
- 适合自动化流程
- 无迭代收敛问题

---

## 立即尝试的代码

以下是修复 scikit-rf 输出的代码框架：

```python
import numpy as np
import skrf

def fix_scikit_rf_vf(input_s4p, output_json, n_poles=16):
    """
    使用 scikit-rf 进行 VectorFitting，然后修复输出的共轭对称性。
    """
    nw = skrf.Network(input_s4p)
    
    # 执行拟合
    vf = skrf.VectorFitting(nw)
    vf.max_iterations = 200
    vf.vector_fit(n_poles_real=0, n_poles_cmplx=n_poles,
                  fit_constant=True, fit_proportional=False)
    
    # 获取 S21 的结果
    idx_s21 = 1 * nw.nports + 0
    poles = vf.poles
    residues = vf.residues[idx_s21]
    constant = vf.constant_coeff[idx_s21]
    
    # 修复：强制共轭对称
    # 步骤 1：识别共轭对
    poles_fixed = []
    residues_fixed = []
    
    used = set()
    for i, p in enumerate(poles):
        if i in used:
            continue
        # 寻找最接近的共轭对
        conj_idx = None
        min_diff = float('inf')
        for j, q in enumerate(poles):
            if j in used or i == j:
                continue
            diff = abs(p - np.conj(q))
            if diff < min_diff:
                min_diff = diff
                conj_idx = j
        
        if conj_idx is not None and min_diff < 1e6:  # 1 Mrad/s 容差
            # 强制共轭对称
            p_avg = (poles[i] + np.conj(poles[conj_idx])) / 2
            r_avg = (residues[i] + np.conj(residues[conj_idx])) / 2
            
            poles_fixed.append(p_avg)
            residues_fixed.append(r_avg)
            used.add(i)
            used.add(conj_idx)
        else:
            # 实数极点
            poles_fixed.append(poles[i].real)
            residues_fixed.append(residues[i].real)
            used.add(i)
    
    # 步骤 2：调整 constant 以匹配 DC 增益
    s21_orig = nw.s[:, 1, 0]
    dc_target = s21_orig[0]  # 第一个频率点的 S21
    
    s = 1j * 2 * np.pi * nw.f[0]
    dc_contrib = sum(r / (s - p) for p, r in zip(poles_fixed, residues_fixed))
    constant_fixed = dc_target - dc_contrib
    
    # 保存修复后的结果
    import json
    config = {
        'version': '2.1-scikit-rf-fixed',
        'method': 'POLE_RESIDUE',
        'pole_residue': {
            'poles_real': [float(p.real) for p in poles_fixed],
            'poles_imag': [float(p.imag) for p in poles_fixed],
            'residues_real': [float(r.real) for r in residues_fixed],
            'residues_imag': [float(r.imag) for r in residues_fixed],
            'constant': float(constant_fixed.real),
            'proportional': 0.0,
            'order': len(poles_fixed)
        },
        'fs': 100e9
    }
    
    with open(output_json, 'w') as f:
        json.dump(config, f, indent=2)
    
    return config

# 运行修复
fix_scikit_rf_vf('peters_01_0605_B12_thru.s4p', 'channel_config_fixed.json')
```

---

## 结论

1. **scikit-rf 不是最佳选择**，但可以通过后处理修复
2. **VFdriver (Fortran)** 是最可靠的替代方案
3. **Loewner 框架** 是最有前景的长期方案

建议立即实施方案 1 的修复代码，同时考虑集成 VFdriver 作为长期解决方案。
