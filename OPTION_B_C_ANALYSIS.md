# 选项 B vs C 详细对比分析

## 背景
- scikit-rf 修复结果：**相关性 0.86，RMSE 5dB**（未达标）
- 需求：**相关性 > 0.95，RMSE < 3dB**

---

## 选项 B: VFdriver (Fortran 原始实现)

### 简介
VFdriver 是 Bjorn Gustavsen 的原始 VectorFitting 实现，Fortran 90 编写，被广泛认为是该算法的权威实现。

### 获取方式
```bash
# 官方网站
https://www.sintef.no/projectweb/vectorfitting/

# 下载链接（可能需要邮箱注册）
https://www.sintef.no/contentassets/1e9f55a5a5f74c8d9d5c8b8a6f8b6d7c/vfdriver.zip
```

### 目录结构
```
VFdriver/
├── src/
│   ├── VFdriver.f90          # 主程序
│   ├──pole relocation.f90   # 极点迁移
│   ├──residue.f90           # 留数计算
│   └── ...
├── Makefile
└── examples/
```

### Python 集成方案

#### 方案 B1: 命令行调用
```python
import subprocess
import numpy as np
import tempfile
import os

def vector_fit_vfdriver(freq, s21, n_poles=16):
    """
    Call VFdriver as external process.
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        # Write input file
        input_file = os.path.join(tmpdir, 'input.dat')
        np.savetxt(input_file, np.column_stack([freq, s21.real, s21.imag]))
        
        # Write config file
        config_file = os.path.join(tmpdir, 'config.txt')
        with open(config_file, 'w') as f:
            f.write(f'{n_poles}\n')  # Number of poles
            f.write('0\n')           # Number of real poles
            f.write('100\n')         # Max iterations
        
        # Run VFdriver
        subprocess.run(['./VFdriver', config_file, input_file], 
                      cwd=tmpdir, check=True)
        
        # Read output
        poles = np.loadtxt(os.path.join(tmpdir, 'poles.txt'))
        residues = np.loadtxt(os.path.join(tmpdir, 'residues.txt'))
        
    return poles, residues
```

#### 方案 B2: f2py 绑定
```bash
# Compile Fortran to Python module
f2py -c -m vfdriver VFdriver.f90
```

```python
# Python usage
import vfdriver
import numpy as np

freq = np.linspace(50e6, 15e9, 1000)
s21 = load_s21_from_s4p('peters_01_0605_B12_thru.s4p')

poles, residues, constant = vfdriver.vector_fit(freq, s21, n_poles=16)
```

#### 方案 B3: 静态库链接到 C++
```cpp
// C++ wrapper
extern "C" {
    void vector_fit_(double* freq, double* s21_real, double* s21_imag,
                     int* n_freq, int* n_poles,
                     double* poles_real, double* poles_imag,
                     double* residues_real, double* residues_imag,
                     double* constant);
}

class VectorFittingFortran {
public:
    std::vector<std::complex<double>> fit(
        const std::vector<double>& freq,
        const std::vector<std::complex<double>>& s21,
        int n_poles);
};
```

### 优点
| 优点 | 说明 |
|------|------|
| **权威实现** | 原作者实现，算法细节正确 |
| **成熟稳定** | 20+ 年工业应用验证 |
| **性能优秀** | Fortran 数值计算效率高 |
| **功能完整** | 支持 Relaxed VF, Fast VF, passivity enforcement |
| **收敛性好** | 比 scikit-rf 收敛判据更严格 |

### 缺点
| 缺点 | 说明 |
|------|------|
| **编译依赖** | 需要 Fortran 编译器 (gfortran/ifort) |
| **集成复杂** | 需要额外封装才能在 Python/C++ 中使用 |
| **平台限制** | Windows 上可能需要 MinGW/Intel Fortran |
| **维护成本** | Fortran 代码不易维护 |

### 预期效果
根据文献和社区反馈，VFdriver 通常能达到：
- **相关性：> 0.99**
- **RMSE：< 1 dB**
- **DC 点匹配误差：< 0.1 dB**

---

## 选项 C: 有理函数形式 (num/den)

### 简介
不拟合 pole-residue 形式，而是直接使用有理函数形式：

```
H(s) = (b_n*s^n + b_{n-1}*s^{n-1} + ... + b_0) / (a_m*s^m + a_{m-1}*s^{m-1} + ... + a_0)
```

### 实现方案

#### 方案 C1: SciPy 有理拟合
```python
from scipy.interpolate import RationalApproximation
from scipy.optimize import least_squares
import numpy as np

def rational_fit_scipy(freq, h, order_num, order_den):
    """
    Fit rational function using least squares.
    """
    s = 1j * 2 * np.pi * freq
    
    # Objective function
    def residuals(params):
        num = params[:order_num+1]
        den = np.concatenate([[1], params[order_num+1:]])  # a_0 = 1
        
        h_fit = np.polyval(num, s) / np.polyval(den, s)
        
        # Weighted error (emphasize magnitude at low frequencies)
        weights = 1.0 / (1.0 + freq / freq[0])  # More weight at DC
        error_real = weights * (h.real - h_fit.real)
        error_imag = weights * (h.imag - h_fit.imag)
        
        return np.concatenate([error_real, error_imag])
    
    # Initial guess from SK iteration
    x0 = initialize_sk(freq, h, order_num, order_den)
    
    # Optimize
    result = least_squares(residuals, x0, method='lm', max_nfev=1000)
    
    num = result.x[:order_num+1]
    den = np.concatenate([[1], result.x[order_num+1:]])
    
    return num, den
```

#### 方案 C2: Sanathanan-Koerner (SK) 迭代
```python
def sk_iteration(freq, h, order_num, order_den, max_iter=10):
    """
    Sanathanan-Koerner iteration for rational fitting.
    Linearizes the problem by iterative weighting.
    """
    s = 1j * 2 * np.pi * freq
    
    # Initial weights
    weights = np.ones(len(freq))
    
    for iteration in range(max_iter):
        # Build weighted matrix
        A = np.zeros((2*len(freq), order_num + order_den + 2))
        b = np.zeros(2*len(freq))
        
        for i, (si, hi, wi) in enumerate(zip(s, h, weights)):
            # Numerator coefficients
            for j in range(order_num + 1):
                A[i, j] = wi * si**j
                A[i + len(freq), j] = 0
            
            # Denominator coefficients (with negative sign)
            for j in range(order_den + 1):
                A[i, order_num + 1 + j] = -wi * hi * si**j
                A[i + len(freq), order_num + 1 + j] = 0
            
            b[i] = wi * hi
            b[i + len(freq)] = 0
        
        # Solve
        x, residuals, rank, s_vals = np.linalg.lstsq(A, b, rcond=None)
        
        num = x[:order_num+1]
        den = np.concatenate([[1], x[order_num+1:]])
        
        # Update weights for next iteration
        h_fit = np.polyval(num, s) / np.polyval(den, s)
        weights = 1.0 / abs(h_fit)  # SK weighting
    
    return num, den
```

#### 方案 C3: 基于状态空间的 C++ 实现
```cpp
// C++ implementation using Armadillo or Eigen
#include <armadillo>

class RationalFitter {
public:
    struct RationalFunction {
        arma::cx_vec numerator;
        arma::cx_vec denominator;
    };
    
    RationalFunction fit(const arma::vec& freq, 
                         const arma::cx_vec& h,
                         int order_num, 
                         int order_den);
    
    // Convert to pole-residue for C++ filter
    void toPoleResidue(const RationalFunction& rf,
                       std::vector<std::complex<double>>& poles,
                       std::vector<std::complex<double>>& residues,
                       double& constant);
};
```

### 数据格式
```json
{
  "version": "3.0-rational",
  "method": "RATIONAL",
  "rational": {
    "numerator": [b_n, b_{n-1}, ..., b_0],
    "denominator": [a_m, a_{m-1}, ..., a_0],
    "order_num": n,
    "order_den": m
  },
  "fs": 100e9
}
```

### C++ 滤波器修改
```cpp
// pole_residue_filter.h 修改为支持两种形式
class ChannelFilter {
public:
    // Option 1: Pole-residue (existing)
    bool initPoleResidue(const std::vector<std::complex<double>>& poles,
                         const std::vector<std::complex<double>>& residues,
                         double constant, double fs);
    
    // Option 2: Rational function (new)
    bool initRational(const std::vector<double>& num,
                      const std::vector<double>& den,
                      double fs);
    
    double process(double input);
    
private:
    // For pole-residue: cascade of state-space sections
    std::vector<StateSpaceSection> sections_;
    
    // For rational: direct form II transposed
    std::vector<double> num_, den_;
    std::vector<double> w_;  // State buffer
    int order_;
    
    FilterType type_;
};
```

### 优点
| 优点 | 说明 |
|------|------|
| **纯 Python/C++** | 无需外部依赖 |
| **实现简单** | 算法相对直观 |
| **易于调试** | 多项式系数比 pole-residue 直观 |
| **数值稳定** | 可用级联二阶节实现 |
| **平台无关** | 纯代码实现 |

### 缺点
| 缺点 | 说明 |
|------|------|
| **高阶不稳定** | 直接多项式形式数值不稳定 |
| **需要转换** | 最终可能需要转换为 pole-residue/state-space 实现 |
| **非标准** | 不像 VectorFitting 那样是行业标准 |
| **优化困难** | 非凸优化问题，易陷入局部最优 |

### 预期效果
- **相关性：0.90-0.95**（取决于实现）
- **RMSE：2-5 dB**
- **可能需要：阶数较高（16-32 阶）**

---

## 综合对比

| 维度 | B1: VFdriver CLI | B2: VFdriver f2py | B3: VFdriver C++ | C1: SciPy | C2: SK 迭代 | C3: C++ 有理 |
|------|------------------|-------------------|------------------|-----------|-------------|--------------|
| **开发时间** | 1 天 | 2-3 天 | 3-5 天 | 2 天 | 3-4 天 | 5-7 天 |
| **维护难度** | 低 | 中 | 高 | 低 | 中 | 中 |
| **预期精度** | ★★★★★ | ★★★★★ | ★★★★★ | ★★★ | ★★★★ | ★★★★ |
| **性能** | ★★★ | ★★★ | ★★★★★ | ★★ | ★★★ | ★★★★★ |
| **灵活性** | 低 | 中 | 高 | 高 | 高 | 高 |
| **平台依赖** | 有 | 有 | 有 | 无 | 无 | 无 |

---

## 我的建议

### 短期（本周内交付）
**推荐 B1: VFdriver 命令行调用**

理由：
1. 最快获得高质量结果（相关性 > 0.99）
2. 实施简单，只需编写 Python 包装器
3. 验证算法正确性后，可再考虑其他方案

### 中期（1-2 周）
**推荐 C2: Sanathanan-Koerner 迭代**

理由：
1. 纯 Python，无外部依赖
2. 比 scikit-rf 更稳定
3. 可以转换为 pole-residue 供 C++ 使用

### 长期（1 个月）
**推荐 B3: VFdriver C++ 集成**

理由：
1. 一劳永逸的解决方案
2. 最高性能和精度
3. 符合 SerDes 项目的 C++ 核心定位

---

## 下一步行动

请选择：

**选项 B1**（推荐）：我帮你实现 VFdriver 命令行调用方案，1 天内完成

**选项 C2**：我帮你实现 Sanathanan-Koerner 迭代，2-3 天内完成

**其他**：告诉我你更倾向的方案
