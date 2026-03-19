#!/usr/bin/env python3
"""Verify C++ pole-residue implementation against Python reference"""
import json
import numpy as np
import pandas as pd
import skrf as rf
from scipy import signal
import sys

def analyze_cpp_time_response():
    """Analyze C++ time domain output to estimate frequency response"""
    # Load C++ output waveform
    df = pd.read_csv('/mnt/d/systemCProjects/SerDesSystemCProject/.worktrees/channel-p1-batch3-scikit-rf/build/channel_pr_waveform.csv')
    
    # Get last portion for steady-state analysis
    t = df['time_s'].values
    input_sig = df['input_V'].values
    output_sig = df['output_V'].values
    
    # Use FFT to estimate frequency response
    fs = 1.0 / (t[1] - t[0])
    n = len(t)
    
    # Window to reduce spectral leakage
    window = np.hanning(n)
    
    # FFT of input and output
    fft_in = np.fft.fft(input_sig * window)
    fft_out = np.fft.fft(output_sig * window)
    
    # Frequency response estimate
    freqs = np.fft.fftfreq(n, 1/fs)
    H_cpp = fft_out / fft_in
    
    # Keep positive frequencies only
    pos_idx = freqs > 0
    freqs = freqs[pos_idx]
    H_cpp = H_cpp[pos_idx]
    
    return freqs, H_cpp, fs

def compute_python_response(freqs):
    """Compute Python pole-residue response at given frequencies"""
    with open('/mnt/d/systemCProjects/SerDesSystemCProject/.worktrees/channel-p1-batch3-scikit-rf/build/channel_pole_residue.json') as f:
        data = json.load(f)
    
    pr_data = data['pole_residue']
    poles = np.array(pr_data['poles_real']) + 1j * np.array(pr_data['poles_imag'])
    residues = np.array(pr_data['residues_real']) + 1j * np.array(pr_data['residues_imag'])
    constant = pr_data['constant']
    
    s = 1j * 2 * np.pi * freqs
    H_py = np.zeros(len(s), dtype=complex)
    
    for p, r in zip(poles, residues):
        H_py += r / (s - p)
    H_py += constant
    
    return H_py

def main():
    print("=" * 60)
    print("Pole-Residue Implementation Verification")
    print("=" * 60)
    
    # Load S-parameter reference
    ntwk = rf.Network('/mnt/d/systemCProjects/SerDesSystemCProject/peters_01_0605_B12_thru.s4p')
    s21 = ntwk.s[:, 1, 0]
    s21_freqs = ntwk.f
    
    # C++ response from time domain
    print("\n1. Analyzing C++ time domain output...")
    cpp_freqs, H_cpp, fs = analyze_cpp_time_response()
    print(f"   C++ sampling rate: {fs/1e9:.2f} GHz")
    print(f"   C++ frequency points: {len(cpp_freqs)}")
    
    # Python pole-residue response
    print("\n2. Computing Python pole-residue response...")
    
    # Match frequencies to S-parameter range
    f_min, f_max = s21_freqs[0], s21_freqs[-1]
    valid_idx = (cpp_freqs >= f_min) & (cpp_freqs <= f_max)
    cpp_freqs_matched = cpp_freqs[valid_idx]
    H_cpp_matched = H_cpp[valid_idx]
    
    H_py = compute_python_response(cpp_freqs_matched)
    
    # Interpolate S21 to matched frequencies for comparison
    s21_interp = np.interp(cpp_freqs_matched, s21_freqs, np.abs(s21))
    
    # Compute correlations
    # Compare magnitudes
    corr_cpp_s21 = np.corrcoef(np.abs(H_cpp_matched), s21_interp)[0, 1]
    corr_py_s21 = np.corrcoef(np.abs(H_py), s21_interp)[0, 1]
    corr_cpp_py = np.corrcoef(np.abs(H_cpp_matched), np.abs(H_py))[0, 1]
    
    print(f"\n3. Correlation Results:")
    print(f"   Python vs S21:  {corr_py_s21:.6f}")
    print(f"   C++ vs S21:     {corr_cpp_s21:.6f}")
    print(f"   C++ vs Python:  {corr_cpp_py:.6f}")
    
    # Summary
    print("\n" + "=" * 60)
    print("Summary:")
    print("=" * 60)
    
    if corr_cpp_py > 0.95:
        print(f"✓ C++ implementation matches Python (corr={corr_cpp_py:.4f})")
    elif corr_cpp_py > 0.90:
        print(f"⚠ C++ implementation acceptable (corr={corr_cpp_py:.4f})")
    else:
        print(f"✗ C++ implementation needs improvement (corr={corr_cpp_py:.4f})")
    
    if corr_cpp_s21 > 0.90:
        print(f"✓ C++ matches S21 reference (corr={corr_cpp_s21:.4f})")
    else:
        print(f"✗ C++ doesn't match S21 well (corr={corr_cpp_s21:.4f})")
    
    return 0 if corr_cpp_py > 0.90 else 1

if __name__ == "__main__":
    sys.exit(main())
