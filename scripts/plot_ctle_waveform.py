#!/usr/bin/env python3
"""
CTLE 波形绘图脚本 - 支持多种测试场景
根据CSV文件名自动识别测试类型并应用对应的可视化模板

测试场景:
  - freq: 频率响应测试 - 输入/输出波形对比 + 增益分析
  - prbs: 基本PRBS测试 - 差分/共模双子图
  - psrr: PSRR测试 - VDD噪声抑制分析
  - cmrr: CMRR测试 - 共模抑制分析
  - sat:  饱和测试 - 输入/输出对比 + 限幅效果
"""

import os
import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from scipy import signal as scipy_signal

plt.rcParams['font.sans-serif'] = ['Arial Unicode MS', 'SimHei', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False

# ============================================================================
# 测试场景配置
# ============================================================================
TEST_CONFIGS = {
    'freq': {
        'name': '频率响应测试',
        'input_type': 'sine',
        'amplitude': 0.1,
        'frequency': 5e9,
        'vcm': 0.6,
    },
    'prbs': {
        'name': '基本PRBS测试',
        'input_type': 'prbs',
        'amplitude': 0.1,
        'frequency': 10e9,
        'vcm': 0.6,
    },
    'psrr': {
        'name': 'PSRR测试',
        'input_type': 'dc',
        'amplitude': 0.0,
        'vcm': 0.6,
        'vdd_ripple': 0.1,
        'vdd_freq': 1e6,
    },
    'cmrr': {
        'name': 'CMRR测试',
        'input_type': 'dc',
        'amplitude': 0.1,
        'vcm': 0.6,
    },
    'sat': {
        'name': '饱和测试',
        'input_type': 'square',
        'amplitude': 0.5,
        'frequency': 1e9,
        'vcm': 0.6,
    },
}

# ============================================================================
# 工具函数
# ============================================================================
def detect_test_type(filepath):
    """根据文件名自动识别测试类型"""
    basename = os.path.basename(filepath).lower()
    if 'freq' in basename:
        return 'freq'
    elif 'psrr' in basename:
        return 'psrr'
    elif 'cmrr' in basename:
        return 'cmrr'
    elif 'sat' in basename:
        return 'sat'
    elif 'prbs' in basename:
        return 'prbs'
    return 'prbs'


def generate_input_signal(time, test_type):
    """根据测试类型生成对应的输入差分信号"""
    config = TEST_CONFIGS.get(test_type, TEST_CONFIGS['prbs'])
    amp = config.get('amplitude', 0.1)
    freq = config.get('frequency', 1e9)
    
    if config['input_type'] == 'sine':
        return amp * np.sin(2 * np.pi * freq * time)
    elif config['input_type'] == 'square':
        return amp * np.sign(np.sin(2 * np.pi * freq * time))
    elif config['input_type'] == 'prbs':
        sample_rate = 100e9
        bits_per_sample = int(sample_rate / freq)
        prbs_bits = []
        for i in range(len(time)):
            bit_index = i // bits_per_sample
            prbs_bits.append(1.0 if (bit_index % 127) < 64 else -1.0)
        return amp * np.array(prbs_bits)
    else:
        return np.full_like(time, amp)


def calculate_stats(data):
    """计算信号统计信息"""
    return {
        'mean': np.mean(data),
        'std': np.std(data),
        'rms': np.sqrt(np.mean(data**2)),
        'min': np.min(data),
        'max': np.max(data),
        'pp': np.max(data) - np.min(data),
    }


def compute_fft(time, data, sample_rate=100e9):
    """计算FFT频谱"""
    n = len(data)
    fft_vals = np.fft.rfft(data)
    fft_freq = np.fft.rfftfreq(n, 1/sample_rate)
    fft_mag = 2.0 / n * np.abs(fft_vals)
    fft_db = 20 * np.log10(fft_mag + 1e-12)
    return fft_freq, fft_mag, fft_db


# ============================================================================
# 频率响应测试绘图 - 输入/输出对比 + 增益分析
# ============================================================================
def plot_frequency_response(df, filepath):
    """频率响应测试: 输入/输出波形对比 + 增益计算"""
    print("📊 测试类型: 频率响应测试 (FREQUENCY_RESPONSE)")
    
    time = df['time'].values
    time_ns = time * 1e9
    output_diff_mV = df['diff'].values * 1e3
    
    config = TEST_CONFIGS['freq']
    input_diff = generate_input_signal(time, 'freq')
    input_diff_mV = input_diff * 1e3
    
    out_stats = calculate_stats(output_diff_mV)
    in_stats = calculate_stats(input_diff_mV)
    
    gain = out_stats['pp'] / in_stats['pp'] if in_stats['pp'] > 0 else 0
    gain_db = 20 * np.log10(gain) if gain > 0 else -100
    
    print(f"\n📈 输入信号: {config['frequency']/1e9:.1f} GHz 正弦波, 幅度 {config['amplitude']*1e3:.0f} mVpp")
    print(f"📈 输出峰峰值: {out_stats['pp']:.2f} mV")
    print(f"📈 电压增益: {gain:.2f}x ({gain_db:.1f} dB)")
    
    fig, axes = plt.subplots(3, 1, figsize=(14, 12))
    fig.suptitle(f"CTLE 频率响应测试 @ {config['frequency']/1e9:.1f} GHz", 
                 fontsize=16, fontweight='bold')
    
    # 子图1: 输入/输出波形对比 (全时域)
    ax1 = axes[0]
    ax1.plot(time_ns, input_diff_mV, 'b-', linewidth=0.8, alpha=0.7, label='输入信号')
    ax1.plot(time_ns, output_diff_mV, 'r-', linewidth=0.8, alpha=0.9, label='输出信号')
    ax1.set_ylabel('差分电压 (mV)', fontsize=11)
    ax1.set_title('输入/输出波形对比 (全时域)', fontsize=11)
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3, linestyle='--')
    ax1.set_xlim(time_ns[0], time_ns[-1])
    
    stats_text = f'增益: {gain:.2f}x ({gain_db:.1f} dB)\n输出Vpp: {out_stats["pp"]:.1f} mV'
    ax1.text(0.02, 0.98, stats_text, transform=ax1.transAxes, verticalalignment='top',
             fontsize=10, bbox=dict(boxstyle='round,pad=0.5', facecolor='lightyellow', 
                                    edgecolor='orange', alpha=0.8))
    
    # 子图2: 放大视图 (前几个周期)
    ax2 = axes[1]
    period_ns = 1e9 / config['frequency']
    zoom_end = min(5 * period_ns, time_ns[-1])
    zoom_mask = time_ns <= zoom_end
    
    ax2.plot(time_ns[zoom_mask], input_diff_mV[zoom_mask], 'b-', linewidth=1.5, 
             alpha=0.7, label='输入信号')
    ax2.plot(time_ns[zoom_mask], output_diff_mV[zoom_mask], 'r-', linewidth=1.5, 
             alpha=0.9, label='输出信号')
    ax2.set_ylabel('差分电压 (mV)', fontsize=11)
    ax2.set_title(f'放大视图 (前 {zoom_end:.2f} ns, ~5个周期)', fontsize=11)
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3, linestyle='--')
    ax2.axhline(y=0, color='gray', linestyle='-', linewidth=0.5, alpha=0.5)
    
    # 子图3: FFT频谱分析
    ax3 = axes[2]
    fft_freq, _, fft_db = compute_fft(time, df['diff'].values)
    fft_freq_ghz = fft_freq / 1e9
    
    ax3.plot(fft_freq_ghz, fft_db, 'g-', linewidth=1.0)
    ax3.axvline(x=config['frequency']/1e9, color='red', linestyle='--', 
                linewidth=1.5, label=f'测试频率 {config["frequency"]/1e9:.1f} GHz')
    ax3.set_xlabel('频率 (GHz)', fontsize=11)
    ax3.set_ylabel('幅度 (dB)', fontsize=11)
    ax3.set_title('输出信号FFT频谱', fontsize=11)
    ax3.set_xlim(0, 20)
    ax3.set_ylim(-80, 20)
    ax3.legend(loc='upper right')
    ax3.grid(True, alpha=0.3, linestyle='--')
    
    plt.tight_layout()
    return fig


# ============================================================================
# PRBS测试绘图 - 差分/共模双子图 (原有模式)
# ============================================================================
def plot_prbs_test(df, filepath):
    """PRBS测试: 差分信号和共模信号波形"""
    print("📊 测试类型: 基本PRBS测试 (BASIC_PRBS)")
    
    time_ns = df['time'] * 1e9
    diff_mV = df['diff'] * 1e3
    cm_V = df['cm']
    
    diff_stats = calculate_stats(diff_mV)
    cm_stats = calculate_stats(cm_V)
    
    print(f"\n📈 差分信号统计:")
    print(f"   峰峰值: {diff_stats['pp']:.2f} mV")
    print(f"   RMS: {diff_stats['rms']:.2f} mV")
    
    fig, axes = plt.subplots(2, 1, figsize=(14, 9), sharex=True)
    fig.suptitle('CTLE PRBS测试 - 瞬态仿真波形', fontsize=16, fontweight='bold')
    
    # 子图1: 差分信号
    ax1 = axes[0]
    ax1.plot(time_ns, diff_mV, 'b-', linewidth=0.6, alpha=0.9, label='差分输出')
    ax1.axhline(y=diff_stats['mean'], color='orange', linestyle='--', linewidth=1.5,
                label=f'均值: {diff_stats["mean"]:.2f} mV')
    ax1.axhline(y=0, color='gray', linestyle='-', linewidth=0.5, alpha=0.5)
    ax1.fill_between(time_ns, diff_stats['min'], diff_stats['max'], alpha=0.1, color='blue')
    ax1.set_ylabel('差分电压 (mV)', fontsize=12)
    ax1.set_title('差分信号 Vdiff = Vout_p - Vout_n', fontsize=11)
    ax1.grid(True, alpha=0.3, linestyle='--')
    ax1.legend(loc='upper right')
    
    stats_text = f'峰峰值: {diff_stats["pp"]:.2f} mV\nRMS: {diff_stats["rms"]:.2f} mV'
    ax1.text(0.02, 0.98, stats_text, transform=ax1.transAxes, verticalalignment='top',
             fontsize=10, bbox=dict(boxstyle='round,pad=0.5', facecolor='lightyellow', 
                                    edgecolor='orange', alpha=0.8))
    
    # 子图2: 共模信号
    ax2 = axes[1]
    ax2.plot(time_ns, cm_V, 'r-', linewidth=0.6, alpha=0.9, label='共模输出')
    ax2.axhline(y=cm_stats['mean'], color='green', linestyle='--', linewidth=1.5,
                label=f'均值: {cm_stats["mean"]:.4f} V')
    ax2.set_xlabel('时间 (ns)', fontsize=12)
    ax2.set_ylabel('共模电压 (V)', fontsize=12)
    ax2.set_title('共模信号 Vcm = (Vout_p + Vout_n) / 2', fontsize=11)
    ax2.grid(True, alpha=0.3, linestyle='--')
    ax2.legend(loc='upper right')
    
    cm_margin = max(cm_stats['std'] * 5, 0.01)
    ax2.set_ylim(cm_stats['mean'] - cm_margin, cm_stats['mean'] + cm_margin)
    
    plt.tight_layout()
    return fig


# ============================================================================
# PSRR测试绘图 - VDD噪声抑制分析
# ============================================================================
def plot_psrr_test(df, filepath):
    """PSRR测试: VDD噪声抑制分析"""
    print("📊 测试类型: PSRR测试 (电源抑制比)")
    
    time = df['time'].values
    time_ns = time * 1e9
    diff_mV = df['diff'].values * 1e3
    
    config = TEST_CONFIGS['psrr']
    vdd_ripple_mV = config['vdd_ripple'] * 1e3
    vdd_freq = config['vdd_freq']
    
    diff_stats = calculate_stats(diff_mV)
    
    psrr_linear = diff_stats['pp'] / (vdd_ripple_mV * 2) if vdd_ripple_mV > 0 else 0
    psrr_db = 20 * np.log10(psrr_linear) if psrr_linear > 0 else -100
    
    print(f"\n📈 VDD纹波: {vdd_ripple_mV:.0f} mVpp @ {vdd_freq/1e6:.1f} MHz")
    print(f"📈 输出纹波: {diff_stats['pp']:.4f} mV")
    print(f"📈 PSRR: {psrr_db:.1f} dB")
    
    fig, axes = plt.subplots(3, 1, figsize=(14, 12))
    fig.suptitle('CTLE PSRR测试 - 电源抑制比分析', fontsize=16, fontweight='bold')
    
    # 子图1: 模拟VDD信号
    ax1 = axes[0]
    vdd_signal = 1.0 + config['vdd_ripple'] * np.sin(2 * np.pi * vdd_freq * time)
    ax1.plot(time_ns, vdd_signal, 'orange', linewidth=1.0, label='VDD (含纹波)')
    ax1.axhline(y=1.0, color='gray', linestyle='--', linewidth=1, label='VDD标称值 1.0V')
    ax1.set_ylabel('VDD (V)', fontsize=11)
    ax1.set_title(f'电源电压 VDD - 纹波: {vdd_ripple_mV:.0f} mVpp @ {vdd_freq/1e6:.1f} MHz', fontsize=11)
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3, linestyle='--')
    ax1.set_ylim(0.85, 1.15)
    
    # 子图2: 差分输出
    ax2 = axes[1]
    ax2.plot(time_ns, diff_mV, 'b-', linewidth=0.8, label='差分输出')
    ax2.axhline(y=0, color='gray', linestyle='-', linewidth=0.5, alpha=0.5)
    ax2.set_ylabel('差分电压 (mV)', fontsize=11)
    ax2.set_title('差分输出信号 (应接近零)', fontsize=11)
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3, linestyle='--')
    
    stats_text = f'输出纹波: {diff_stats["pp"]*1e3:.3f} μV\nPSRR: {psrr_db:.1f} dB'
    ax2.text(0.02, 0.98, stats_text, transform=ax2.transAxes, verticalalignment='top',
             fontsize=10, bbox=dict(boxstyle='round,pad=0.5', facecolor='lightgreen', 
                                    edgecolor='green', alpha=0.8))
    
    # 子图3: FFT频谱
    ax3 = axes[2]
    fft_freq, _, fft_db = compute_fft(time, df['diff'].values)
    fft_freq_mhz = fft_freq / 1e6
    
    ax3.plot(fft_freq_mhz, fft_db, 'g-', linewidth=1.0)
    ax3.axvline(x=vdd_freq/1e6, color='red', linestyle='--', linewidth=1.5,
                label=f'VDD纹波频率 {vdd_freq/1e6:.1f} MHz')
    ax3.set_xlabel('频率 (MHz)', fontsize=11)
    ax3.set_ylabel('幅度 (dB)', fontsize=11)
    ax3.set_title('输出FFT频谱 - 检查VDD纹波泄漏', fontsize=11)
    ax3.set_xlim(0, 10)
    ax3.set_ylim(-120, -40)
    ax3.legend(loc='upper right')
    ax3.grid(True, alpha=0.3, linestyle='--')
    
    plt.tight_layout()
    return fig


# ============================================================================
# CMRR测试绘图 - 共模抑制分析
# ============================================================================
def plot_cmrr_test(df, filepath):
    """CMRR测试: 共模抑制比分析"""
    print("📊 测试类型: CMRR测试 (共模抑制比)")
    
    time_ns = df['time'] * 1e9
    diff_mV = df['diff'] * 1e3
    cm_V = df['cm']
    
    diff_stats = calculate_stats(diff_mV)
    cm_stats = calculate_stats(cm_V)
    
    config = TEST_CONFIGS['cmrr']
    input_amp_mV = config['amplitude'] * 1e3
    
    print(f"\n📈 差分输入: {input_amp_mV:.0f} mVpp (DC)")
    print(f"📈 差分输出均值: {diff_stats['mean']:.2f} mV")
    print(f"📈 共模输出均值: {cm_stats['mean']:.4f} V")
    
    fig, axes = plt.subplots(2, 1, figsize=(14, 9))
    fig.suptitle('CTLE CMRR测试 - 共模抑制比分析', fontsize=16, fontweight='bold')
    
    # 子图1: 差分输出
    ax1 = axes[0]
    ax1.plot(time_ns, diff_mV, 'b-', linewidth=0.8, label='差分输出')
    ax1.axhline(y=diff_stats['mean'], color='orange', linestyle='--', linewidth=1.5,
                label=f'均值: {diff_stats["mean"]:.2f} mV')
    ax1.set_ylabel('差分电压 (mV)', fontsize=11)
    ax1.set_title('差分输出信号', fontsize=11)
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3, linestyle='--')
    
    stats_text = f'输入: {input_amp_mV:.0f} mV DC\n输出均值: {diff_stats["mean"]:.2f} mV\n波动: {diff_stats["pp"]:.2f} mV'
    ax1.text(0.02, 0.98, stats_text, transform=ax1.transAxes, verticalalignment='top',
             fontsize=10, bbox=dict(boxstyle='round,pad=0.5', facecolor='lightyellow', 
                                    edgecolor='orange', alpha=0.8))
    
    # 子图2: 共模输出稳定性
    ax2 = axes[1]
    ax2.plot(time_ns, cm_V, 'r-', linewidth=0.8, label='共模输出')
    ax2.axhline(y=cm_stats['mean'], color='green', linestyle='--', linewidth=1.5,
                label=f'均值: {cm_stats["mean"]:.4f} V')
    ax2.set_xlabel('时间 (ns)', fontsize=11)
    ax2.set_ylabel('共模电压 (V)', fontsize=11)
    ax2.set_title('共模输出稳定性', fontsize=11)
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3, linestyle='--')
    
    cm_margin = max(cm_stats['std'] * 5, 0.01)
    ax2.set_ylim(cm_stats['mean'] - cm_margin, cm_stats['mean'] + cm_margin)
    
    cm_text = f'目标Vcm: 0.6 V\n实际均值: {cm_stats["mean"]:.4f} V\n波动: ±{cm_stats["std"]*1e3:.3f} mV'
    ax2.text(0.02, 0.98, cm_text, transform=ax2.transAxes, verticalalignment='top',
             fontsize=10, bbox=dict(boxstyle='round,pad=0.5', facecolor='lightgreen', 
                                    edgecolor='green', alpha=0.8))
    
    plt.tight_layout()
    return fig


# ============================================================================
# 饱和测试绘图 - 输入/输出对比 + 限幅效果
# ============================================================================
def plot_saturation_test(df, filepath):
    """饱和测试: 大信号输入/输出对比 + 限幅分析"""
    print("📊 测试类型: 饱和测试 (SATURATION)")
    
    time = df['time'].values
    time_ns = time * 1e9
    output_diff_mV = df['diff'].values * 1e3
    
    config = TEST_CONFIGS['sat']
    input_diff = generate_input_signal(time, 'sat')
    input_diff_mV = input_diff * 1e3
    
    out_stats = calculate_stats(output_diff_mV)
    in_stats = calculate_stats(input_diff_mV)
    
    expected_gain = 1.5
    expected_output = in_stats['pp'] * expected_gain
    compression = (1 - out_stats['pp'] / expected_output) * 100 if expected_output > 0 else 0
    
    print(f"\n📈 输入幅度: {in_stats['pp']:.0f} mVpp")
    print(f"📈 输出幅度: {out_stats['pp']:.0f} mVpp")
    print(f"📈 理论输出 (无饱和): {expected_output:.0f} mVpp")
    print(f"📈 压缩量: {compression:.1f}%")
    
    fig, axes = plt.subplots(3, 1, figsize=(14, 12))
    fig.suptitle('CTLE 饱和测试 - 大信号限幅分析', fontsize=16, fontweight='bold')
    
    # 子图1: 输入/输出波形对比
    ax1 = axes[0]
    ax1.plot(time_ns, input_diff_mV, 'b-', linewidth=1.0, alpha=0.7, label='输入信号')
    ax1.plot(time_ns, output_diff_mV, 'r-', linewidth=1.0, alpha=0.9, label='输出信号')
    ax1.set_ylabel('差分电压 (mV)', fontsize=11)
    ax1.set_title('输入/输出波形对比 (大信号)', fontsize=11)
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3, linestyle='--')
    ax1.axhline(y=0, color='gray', linestyle='-', linewidth=0.5, alpha=0.5)
    
    stats_text = f'输入: {in_stats["pp"]:.0f} mVpp\n输出: {out_stats["pp"]:.0f} mVpp\n压缩: {compression:.1f}%'
    ax1.text(0.02, 0.98, stats_text, transform=ax1.transAxes, verticalalignment='top',
             fontsize=10, bbox=dict(boxstyle='round,pad=0.5', facecolor='lightyellow', 
                                    edgecolor='orange', alpha=0.8))
    
    # 子图2: 放大视图 (几个周期)
    ax2 = axes[1]
    period_ns = 1e9 / config['frequency']
    zoom_end = min(3 * period_ns, time_ns[-1])
    zoom_mask = time_ns <= zoom_end
    
    ax2.plot(time_ns[zoom_mask], input_diff_mV[zoom_mask], 'b-', linewidth=1.5, 
             alpha=0.7, label='输入 (方波)')
    ax2.plot(time_ns[zoom_mask], output_diff_mV[zoom_mask], 'r-', linewidth=1.5, 
             alpha=0.9, label='输出 (饱和)')
    
    ax2.axhline(y=out_stats['max'], color='red', linestyle=':', linewidth=1, alpha=0.7)
    ax2.axhline(y=out_stats['min'], color='red', linestyle=':', linewidth=1, alpha=0.7)
    
    ax2.set_ylabel('差分电压 (mV)', fontsize=11)
    ax2.set_title(f'放大视图 - 限幅效果 (前 {zoom_end:.1f} ns)', fontsize=11)
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3, linestyle='--')
    
    limit_text = f'输出上限: {out_stats["max"]:.1f} mV\n输出下限: {out_stats["min"]:.1f} mV'
    ax2.text(0.98, 0.02, limit_text, transform=ax2.transAxes, verticalalignment='bottom',
             horizontalalignment='right', fontsize=10,
             bbox=dict(boxstyle='round,pad=0.5', facecolor='lightcoral', 
                       edgecolor='red', alpha=0.8))
    
    # 子图3: 传输特性曲线 (输入 vs 输出)
    ax3 = axes[2]
    ax3.scatter(input_diff_mV, output_diff_mV, s=1, alpha=0.3, c='blue')
    ax3.plot([-600, 600], [-600*expected_gain, 600*expected_gain], 'g--', 
             linewidth=1.5, label=f'理想线性 (增益={expected_gain}x)')
    ax3.set_xlabel('输入差分电压 (mV)', fontsize=11)
    ax3.set_ylabel('输出差分电压 (mV)', fontsize=11)
    ax3.set_title('传输特性曲线 (Vin vs Vout)', fontsize=11)
    ax3.legend(loc='upper left')
    ax3.grid(True, alpha=0.3, linestyle='--')
    ax3.set_xlim(-600, 600)
    ax3.set_ylim(-800, 800)
    ax3.set_aspect('equal', adjustable='box')
    
    plt.tight_layout()
    return fig


# ============================================================================
# 主入口
# ============================================================================
def plot_ctle_waveform(filepath):
    """根据测试类型自动选择绘图模板"""
    print(f"📂 读取波形文件: {filepath}")
    
    df = pd.read_csv(filepath)
    print(f"📊 数据点数量: {len(df)}")
    
    test_type = detect_test_type(filepath)
    config = TEST_CONFIGS.get(test_type, TEST_CONFIGS['prbs'])
    print(f"🎯 识别测试类型: {config['name']} ({test_type})")
    
    plot_functions = {
        'freq': plot_frequency_response,
        'prbs': plot_prbs_test,
        'psrr': plot_psrr_test,
        'cmrr': plot_cmrr_test,
        'sat': plot_saturation_test,
    }
    
    plot_func = plot_functions.get(test_type, plot_prbs_test)
    fig = plot_func(df, filepath)
    
    output_dir = os.path.dirname(filepath) or '.'
    basename = os.path.splitext(os.path.basename(filepath))[0]
    output_png = os.path.join(output_dir, f'{basename}_plot.png')
    fig.savefig(output_png, dpi=150, bbox_inches='tight', facecolor='white')
    print(f"\n✅ 波形图已保存: {output_png}")
    
    plt.show()
    return fig


def main():
    default_path = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        'build', 'tb', 'ctle_tran_output.csv'
    )
    
    if len(sys.argv) > 1:
        filepath = sys.argv[1]
    elif os.path.exists(default_path):
        filepath = default_path
    else:
        print("❌ 未找到波形文件！")
        print(f"   默认路径: {default_path}")
        print("\n用法:")
        print("   python3 plot_ctle_waveform.py <csv_file>")
        print("\n支持的测试类型 (根据文件名自动识别):")
        for key, cfg in TEST_CONFIGS.items():
            print(f"   - *{key}*.csv → {cfg['name']}")
        sys.exit(1)
    
    if not os.path.exists(filepath):
        print(f"❌ 文件不存在: {filepath}")
        sys.exit(1)
    
    plot_ctle_waveform(filepath)
    print("\n🎉 绘图完成!")


if __name__ == "__main__":
    main()