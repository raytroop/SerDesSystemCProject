#!/usr/bin/env python3
"""
Deep diagnostic script to identify root causes of VF and IR inconsistency.
"""
import sys
import numpy as np
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from vector_fitting import VectorFitting
from process_sparam import TouchstoneReader, DCCompletion, ImpulseResponseGenerator

def diagnose_ir_issue():
    """Diagnose why IR method produces zero DC gain."""
    print("=" * 70)
    print("DIAGNOSING IR ZERO DC GAIN ISSUE")
    print("=" * 70)
    
    s4p_file = "/Users/liuyizhe/Developer/systemCprojects/peters_01_0605_B12_thru.s4p"
    reader = TouchstoneReader(s4p_file)
    freq, s_matrix = reader.read()
    S21 = reader.get_sparam(2, 1)
    
    print(f"\n1. Original S21 data:")
    print(f"   Frequency range: {freq[0]/1e9:.3f} - {freq[-1]/1e9:.3f} GHz")
    print(f"   S21[0] (lowest freq): {S21[0]:.6f}")
    print(f"   |S21[0]|: {np.abs(S21[0]):.6f}")
    print(f"   S21 mean magnitude: {np.mean(np.abs(S21)):.6f}")
    
    # Step-by-step IR generation
    fs = 100e9
    n_samples = 4096
    
    print(f"\n2. DC Completion step:")
    freq_dc, S_dc = DCCompletion.complete(freq, S21, method='vf')
    print(f"   DC value added: {S_dc[0]:.6f}")
    print(f"   |S_dc[0]|: {np.abs(S_dc[0]):.6f}")
    print(f"   S_dc[1:5]: {S_dc[1:5]}")
    
    ir_gen = ImpulseResponseGenerator(fs, n_samples)
    
    print(f"\n3. Resampling to uniform grid:")
    freq_grid, S_grid = ir_gen.resample_to_grid(freq_dc, S_dc)
    print(f"   Grid size: {len(freq_grid)} points")
    print(f"   Grid range: {freq_grid[0]/1e9:.3f} - {freq_grid[-1]/1e9:.3f} GHz")
    print(f"   S_grid[0]: {S_grid[0]:.6f}")
    print(f"   S_grid mean: {np.mean(np.abs(S_grid)):.6f}")
    print(f"   S_grid sum: {np.sum(np.abs(S_grid)):.6f}")
    
    print(f"\n4. Bandlimit windowing:")
    S_windowed = ir_gen.apply_bandlimit(S_grid)
    print(f"   S_windowed[0]: {S_windowed[0]:.6f}")
    print(f"   S_windowed mean: {np.mean(np.abs(S_windowed)):.6f}")
    print(f"   S_windowed[-10:] (edge): {np.abs(S_windowed[-10:])}")
    
    print(f"\n5. IFFT computation (detailed):")
    N = ir_gen.n_samples
    N_pos = len(S_grid)
    N_half = N // 2 + 1
    
    print(f"   N (total samples): {N}")
    print(f"   N_pos (positive freq points): {N_pos}")
    print(f"   N_half (expected positive bins): {N_half}")
    
    # Build bilateral spectrum manually to debug
    S_bilateral = np.zeros(N, dtype=complex)
    n_fill = min(N_pos, N_half)
    print(f"   n_fill: {n_fill}")
    
    S_bilateral[:n_fill] = S_windowed[:n_fill]
    print(f"   S_bilateral[0] (DC): {S_bilateral[0]:.6f}")
    print(f"   S_bilateral[1]: {S_bilateral[1]:.6f}")
    print(f"   S_bilateral[:5]: {S_bilateral[:5]}")
    
    # Mirror for negative frequencies
    for k in range(1, n_fill):
        if N - k < N:
            S_bilateral[N - k] = np.conj(S_bilateral[k])
    
    print(f"   S_bilateral[N-1] (should be conj of [1]): {S_bilateral[N-1]:.6f}")
    print(f"   S_bilateral energy: {np.sum(np.abs(S_bilateral)**2):.6f}")
    
    # IFFT
    h_complex = np.fft.ifft(S_bilateral)
    h_real = np.real(h_complex)
    
    print(f"\n6. IFFT result:")
    print(f"   h_real shape: {h_real.shape}")
    print(f"   h_real[0]: {h_real[0]:.10e}")
    print(f"   h_real max: {np.max(np.abs(h_real)):.10e}")
    print(f"   h_real sum: {np.sum(h_real):.10e}")
    print(f"   h_real energy (sum of squares): {np.sum(h_real**2):.10e}")
    print(f"   h_real[:10]: {h_real[:10]}")
    
    # Find peak
    peak_idx = np.argmax(np.abs(h_real))
    dt = 1.0 / fs
    print(f"   Peak index: {peak_idx}")
    print(f"   Peak time: {peak_idx * dt * 1e12:.2f} ps")
    print(f"   Peak value: {h_real[peak_idx]:.10e}")
    
    print(f"\n7. DC gain calculation:")
    dc_gain = np.sum(h_real) * dt
    print(f"   DC gain = sum(h) * dt = {dc_gain:.10e}")
    print(f"   Expected from S21[0]: ~{np.abs(S21[0]):.6f}")
    
    # Verify FFT roundtrip
    print(f"\n8. FFT roundtrip verification:")
    H_fft = np.fft.fft(h_real)
    print(f"   H_fft[0] (DC): {H_fft[0]:.6f}")
    print(f"   H_fft[1]: {H_fft[1]:.6f}")
    print(f"   Original S_bilateral[0]: {S_bilateral[0]:.6f}")
    print(f"   Original S_bilateral[1]: {S_bilateral[1]:.6f}")
    
    return h_real, S_bilateral


def diagnose_vf_issue():
    """Diagnose why VF has high MSE and unstable DC gain."""
    print("\n" + "=" * 70)
    print("DIAGNOSING VF HIGH MSE AND UNSTABLE DC GAIN")
    print("=" * 70)
    
    s4p_file = "/Users/liuyizhe/Developer/systemCprojects/peters_01_0605_B12_thru.s4p"
    reader = TouchstoneReader(s4p_file)
    freq, s_matrix = reader.read()
    S21 = reader.get_sparam(2, 1)
    
    print(f"\n1. S21 characteristics:")
    print(f"   DC value (extrapolated): ~{np.abs(S21[0]):.6f}")
    
    # Phase analysis
    phase = np.unwrap(np.angle(S21))
    omega = 2 * np.pi * freq
    
    # Linear fit to estimate delay
    poly = np.polyfit(omega, phase, 1)
    delay = -poly[0]
    print(f"\n2. Phase analysis:")
    print(f"   Phase at f_min: {phase[0] * 180/np.pi:.2f} deg")
    print(f"   Phase at f_max: {phase[-1] * 180/np.pi:.2f} deg")
    print(f"   Total phase change: {(phase[-1] - phase[0]) * 180/np.pi:.2f} deg")
    print(f"   Estimated group delay: {delay * 1e12:.2f} ps")
    
    # Remove delay and see residual phase
    S21_no_delay = S21 * np.exp(1j * omega * delay)
    phase_residual = np.unwrap(np.angle(S21_no_delay))
    
    print(f"\n3. Residual phase after delay removal:")
    print(f"   Residual phase at f_min: {phase_residual[0] * 180/np.pi:.2f} deg")
    print(f"   Residual phase at f_max: {phase_residual[-1] * 180/np.pi:.2f} deg")
    print(f"   Residual phase range: {(phase_residual.max() - phase_residual.min()) * 180/np.pi:.2f} deg")
    
    # Try VF on original vs delay-removed data
    print(f"\n4. VF fitting comparison:")
    
    orders = [8, 12, 16]
    for order in orders:
        # Original data
        vf_orig = VectorFitting(order=order, max_iterations=20, tolerance=1e-10)
        vf_orig.fit(freq, S21, enforce_stability=True)
        H_orig = vf_orig.evaluate(freq)
        mse_orig = np.mean(np.abs(S21 - H_orig)**2)
        
        # Delay-removed data
        vf_nodelay = VectorFitting(order=order, max_iterations=20, tolerance=1e-10)
        vf_nodelay.fit(freq, S21_no_delay, enforce_stability=True)
        H_nodelay = vf_nodelay.evaluate(freq)
        mse_nodelay = np.mean(np.abs(S21_no_delay - H_nodelay)**2)
        
        # Reconstruct with delay
        H_reconstructed = H_nodelay * np.exp(-1j * omega * delay)
        mse_recon = np.mean(np.abs(S21 - H_reconstructed)**2)
        
        print(f"   Order {order}:")
        print(f"      VF on original: MSE={mse_orig:.2e}, DC={vf_orig.get_dc_gain():.4f}")
        print(f"      VF on delay-removed: MSE={mse_nodelay:.2e}, DC={vf_nodelay.get_dc_gain():.4f}")
        print(f"      Reconstructed MSE: {mse_recon:.2e}")
    
    return S21_no_delay, delay


if __name__ == '__main__':
    h_ir, S_bilateral = diagnose_ir_issue()
    S21_no_delay, delay = diagnose_vf_issue()
