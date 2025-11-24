# CTLE 模块迁移指南

## 概述
本指南说明如何从旧版单端 CTLE 模块迁移到新版差分 CTLE 模块。

## 破坏性更改

### 1. 端口接口变更

**旧版接口**（已移除）:
```cpp
sca_tdf::sca_in<double> in;
sca_tdf::sca_out<double> out;
```

**新版接口**:
```cpp
// 差分输入
sca_tdf::sca_in<double> in_p;
sca_tdf::sca_in<double> in_n;

// 电源输入（可选，用于PSRR）
sca_tdf::sca_in<double> vdd;

// 差分输出
sca_tdf::sca_out<double> out_p;
sca_tdf::sca_out<double> out_n;
```

### 2. 参数结构体扩展

**新增必填参数**:
- `vcm_out` (double): 差分输出共模电压
- `vos` (double): 输入偏移电压
- `vnoise_sigma` (double): 噪声标准差
- `sat_min` (double): 输出最小电压
- `sat_max` (double): 输出最大电压

**新增可选参数结构**:
- `psrr` (PsrrParams): 电源抑制比配置
- `cmfb` (CmfbParams): 共模反馈配置
- `cmrr` (CmrrParams): 共模抑制比配置

## 迁移步骤

### 步骤 1: 更新测试平台代码

#### 旧版实例化（需修改）:
```cpp
// 旧版 - 单端信号
sca_tdf::sca_signal<double> tx_out;
sca_tdf::sca_signal<double> ctle_out;

RxCtleTdf ctle("ctle", ctle_params);
ctle.in(tx_out);
ctle.out(ctle_out);
```

#### 新版实例化（推荐）:
```cpp
// 新版 - 差分信号
sca_tdf::sca_signal<double> tx_out_p;
sca_tdf::sca_signal<double> tx_out_n;
sca_tdf::sca_signal<double> vdd_supply;  // 电源电压
sca_tdf::sca_signal<double> ctle_out_p;
sca_tdf::sca_signal<double> ctle_out_n;

RxCtleTdf ctle("ctle", ctle_params);
ctle.in_p(tx_out_p);
ctle.in_n(tx_out_n);
ctle.vdd(vdd_supply);  // 必须连接，即使不使用PSRR
ctle.out_p(ctle_out_p);
ctle.out_n(ctle_out_n);
```

### 步骤 2: 更新配置文件

#### 最小配置（使用默认值）:
```json
{
  "rx": {
    "ctle": {
      "zeros": [2e9],
      "poles": [30e9],
      "dc_gain": 1.5,
      "vcm_out": 0.6,
      "offset_enable": false,
      "vos": 0.0,
      "noise_enable": false,
      "vnoise_sigma": 0.0,
      "sat_min": -0.5,
      "sat_max": 0.5
    }
  }
}
```

#### 完整配置（启用所有功能）:
```json
{
  "rx": {
    "ctle": {
      "zeros": [2e9, 5e9],
      "poles": [30e9, 50e9],
      "dc_gain": 2.0,
      "vcm_out": 0.6,
      "offset_enable": true,
      "vos": 0.005,
      "noise_enable": true,
      "vnoise_sigma": 0.001,
      "sat_min": -0.4,
      "sat_max": 0.8,
      "psrr": {
        "enable": true,
        "gain": 0.05,
        "zeros": [1e6],
        "poles": [1e3, 1e7],
        "vdd_nom": 1.0
      },
      "cmfb": {
        "enable": true,
        "bandwidth": 1e6,
        "loop_gain": 2.0
      },
      "cmrr": {
        "enable": true,
        "gain": 1e-3,
        "zeros": [],
        "poles": [1e5]
      }
    }
  }
}
```

### 步骤 3: 单端到差分信号转换

如果现有系统使用单端信号，需要添加转换模块:

```cpp
// 单端转差分转换器（示例）
SC_MODULE(Single2Diff) {
    sca_tdf::sca_in<double> in;
    sca_tdf::sca_out<double> out_p;
    sca_tdf::sca_out<double> out_n;
    
    double vcm;  // 共模电压
    
    void processing() {
        double sig = in.read();
        out_p.write(vcm + 0.5 * sig);
        out_n.write(vcm - 0.5 * sig);
    }
    
    SCA_CTOR(Single2Diff) : vcm(0.6) {}
};

// 使用
Single2Diff s2d("s2d");
s2d.in(tx_out_single);
s2d.out_p(tx_out_p);
s2d.out_n(tx_out_n);
```

### 步骤 4: 添加电源信号源

即使不使用 PSRR 功能，`vdd` 端口也必须连接:

```cpp
// 方法 1: 使用常数源
sca_tdf::sca_source<double> vdd_source("vdd_source");
vdd_source.set_value(1.0);  // 1.0V 电源
sca_tdf::sca_signal<double> vdd_sig;
vdd_source.y(vdd_sig);
ctle.vdd(vdd_sig);

// 方法 2: 使用带纹波的电源（用于PSRR测试）
SC_MODULE(VddSource) {
    sca_tdf::sca_out<double> vdd;
    
    void processing() {
        double t = get_time().to_seconds();
        // 1.0V DC + 10mV @ 1MHz 纹波
        double v = 1.0 + 0.01 * sin(2*M_PI*1e6*t);
        vdd.write(v);
    }
    
    SCA_CTOR(VddSource) {}
};
```

## 常见问题

### Q1: 旧版测试平台无法编译
**原因**: 端口名称和接口已更改

**解决方案**: 按照步骤 1 更新测试平台代码，将单端连接改为差分连接

### Q2: vdd 端口未连接导致编译错误
**原因**: SystemC-AMS 要求所有端口必须连接

**解决方案**: 
- 如果不使用 PSRR，连接到常数电源源（见步骤 4 方法 1）
- 如果使用 PSRR，连接到带纹波的电源源（见步骤 4 方法 2）

### Q3: 配置文件加载失败
**原因**: 缺少新增的参数字段

**解决方案**: 
- 方案 A: 更新配置文件，添加所有新字段（参见步骤 2）
- 方案 B: 修改 config_loader 支持可选字段和默认值

### Q4: 如何验证 PSRR 功能？
**测试方法**:
1. 启用 PSRR: `psrr.enable = true`
2. 配置 PSRR 参数（增益、零极点）
3. 在 vdd 端口注入单频或多频纹波
4. 测量差分输出的纹波幅度
5. 计算抑制比：`PSRR_dB = 20*log10(Vdd_ripple / Vout_ripple)`

### Q5: 如何验证 CMFB 功能？
**测试方法**:
1. 启用 CMFB: `cmfb.enable = true`
2. 测量输出共模电压：`vcm_meas = 0.5 * (out_p + out_n)`
3. 施加阶跃或慢变扰动
4. 验证 `vcm_meas` 最终收敛到 `vcm_out`
5. 测量建立时间与配置的 `bandwidth` 一致

### Q6: 差分输出如何转回单端？
**方法**:
```cpp
SC_MODULE(Diff2Single) {
    sca_tdf::sca_in<double> in_p;
    sca_tdf::sca_in<double> in_n;
    sca_tdf::sca_out<double> out;
    
    void processing() {
        double diff = in_p.read() - in_n.read();
        out.write(diff);
    }
    
    SCA_CTOR(Diff2Single) {}
};
```

## 向后兼容性建议

如果需要同时支持旧版和新版代码:

```cpp
#define USE_NEW_CTLE  // 注释掉则使用旧版

#ifdef USE_NEW_CTLE
    // 新版差分接口
    sca_tdf::sca_signal<double> ctle_out_p, ctle_out_n, vdd_sig;
    RxCtleTdf ctle("ctle", ctle_params);
    ctle.in_p(tx_out_p);
    ctle.in_n(tx_out_n);
    ctle.vdd(vdd_sig);
    ctle.out_p(ctle_out_p);
    ctle.out_n(ctle_out_n);
#else
    // 旧版单端接口（需保留旧版代码）
    sca_tdf::sca_signal<double> ctle_out;
    RxCtleTdf_Old ctle("ctle", ctle_params);
    ctle.in(tx_out);
    ctle.out(ctle_out);
#endif
```

## 性能注意事项

1. **采样率设置**:
   - 新版 CTLE 在 `set_attributes()` 中设置了默认时间步 10ps
   - 对于超高速应用 (>50GHz)，可能需要手动调整
   - 确保采样率 ≥ 20-50× 最高极点频率

2. **滤波器复杂度**:
   - 零极点数量影响仿真速度
   - 建议零极点总数 ≤ 10
   - 高阶滤波器可能导致数值不稳定

3. **内存使用**:
   - 每个 CTLE 实例最多创建 4 个 `ltf_nd` 对象
   - 大规模仿真时注意内存消耗

## 测试检查清单

迁移完成后，请验证以下功能:

- [ ] 基本差分信号传输正常
- [ ] 频响曲线与零极点配置一致
- [ ] 输出共模电压为 `vcm_out`
- [ ] 偏移开关功能正常（`offset_enable`）
- [ ] 噪声开关功能正常（`noise_enable`）
- [ ] 饱和限制工作正常（大信号输入）
- [ ] PSRR 功能（如果启用）
- [ ] CMFB 功能（如果启用）
- [ ] CMRR 功能（如果启用）
- [ ] 配置文件加载正常
- [ ] 与下游模块（VGA、Sampler）连接正常

## 示例参考

完整的测试平台示例可参考:
- `tb/simple_link_tb.cpp` - 需要更新为差分接口
- `config/default.json` - 已更新的配置示例
- `config/default.yaml` - 已更新的配置示例

## 技术支持

如遇到迁移问题，请参考:
1. `docs/modules/ctle.md` - 完整设计文档
2. `docs/CTLE_UPDATE_SUMMARY.md` - 更新总结
3. 项目 GitHub Issues

## 版本信息

- 旧版 CTLE: v0.1 (单端接口)
- 新版 CTLE: v0.3 (差分接口，完整功能)
- 迁移指南版本: 1.0
- 更新日期: 2025-11-23
