#!/usr/bin/env python3
"""
Comparison script for VectorFitting and ImpulseResponse methods.
This script analyzes the consistency between the two methods using 
the peters_01_0605_B12_thru.s4p file.
"""

import sys
import os
import logging
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# Add scripts directory to path
sys.path.insert(0, str(Path(__file__).parent))

from vector_fitting import VectorFitting, check_passivity
from process_sparam import TouchstoneReader, DCCompletion, ImpulseResponseGenerator

logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
logger = logging.getLogger(__name__)


def compare_methods(s4p_file, sparam='S21', vf_order=12, ir_samples=4096, fs=100e9):
    """
    Compare VectorFitting and ImpulseResponse methods.
    
    Returns detailed comparison metrics.
    """
    logger.info(f"=" * 60)
    logger.info(f"Comparing VF and IR methods for {sparam}")
    logger.info(f"=" * 60)
    
    # Read S4P file
    reader = TouchstoneReader(s4p_file)
    freq, s_matrix = reader.read()
    
    # Extract specific S-parameter
    i = int(sparam[1])
    j = int(sparam[2])
    S_data = reader.get_sparam(i, j)
    
    logger.info(f"Frequency range: {freq[0]/1e9:.3f} - {freq[-1]/1e9:.3f} GHz")
    logger.info(f"Number of frequency points: {len(freq)}")
    logger.info(f"S{i}{j} magnitude range: {np.abs(S_data).min():.4f} - {np.abs(S_data).max():.4f}")
    
    # Check passivity
    is_passive, max_eig, violations = check_passivity(s_matrix, freq)
    logger.info(f"Passivity check: {'PASS' if is_passive else 'FAIL'}, max_eigenvalue={max_eig:.4f}")
    
    results = {
        'freq': freq,
        'S_orig': S_data,
    }
    
    # =========================================================================
    # Method 1: Vector Fitting
    # =========================================================================
    logger.info(f"\n--- Vector Fitting (order={vf_order}) ---")
    
    vf = VectorFitting(order=vf_order, max_iterations=15, tolerance=1e-8)
    vf_result = vf.fit(freq, S_data, enforce_stability=True)
    
    # Evaluate VF at original frequencies
    H_vf = vf.evaluate(freq)
    
    # VF metrics
    vf_mse = np.mean(np.abs(S_data - H_vf)**2)
    vf_max_error = np.max(np.abs(S_data - H_vf))
    vf_dc_gain = vf.get_dc_gain()
    
    logger.info(f"VF MSE: {vf_mse:.2e}")
    logger.info(f"VF max error: {vf_max_error:.2e}")
    logger.info(f"VF DC gain: {vf_dc_gain:.6f}")
    logger.info(f"VF poles stable: {np.all(np.real(vf_result['poles']) < 0)}")
    
    results['H_vf'] = H_vf
    results['vf_mse'] = vf_mse
    results['vf_dc_gain'] = vf_dc_gain
    results['vf_poles'] = vf_result['poles']
    
    # =========================================================================
    # Method 2: Impulse Response
    # =========================================================================
    logger.info(f"\n--- Impulse Response (samples={ir_samples}, fs={fs/1e9:.0f}GHz) ---")
    
    ir_gen = ImpulseResponseGenerator(fs, ir_samples)
    ir_result = ir_gen.generate(freq, S_data, dc_method='magphase', causality='window')
    
    h_ir = np.array(ir_result['impulse'])
    dt = ir_result['dt']
    
    # IR DC gain: For discrete-time impulse response, DC gain = sum(h)
    # because H(0) = FFT(h)[0] = sum(h)
    ir_dc_gain = np.sum(h_ir)
    
    # Convert IR back to frequency domain for comparison
    # Zero-pad to match n_samples
    h_padded = np.zeros(ir_samples)
    h_padded[:len(h_ir)] = h_ir
    
    # FFT to get frequency response
    H_ir_fft = np.fft.fft(h_padded)
    freq_ir = np.fft.fftfreq(ir_samples, dt)
    
    # Get positive frequencies
    pos_mask = freq_ir >= 0
    freq_ir_pos = freq_ir[pos_mask]
    H_ir_pos = H_ir_fft[pos_mask]
    
    # Interpolate to original frequency points
    from scipy.interpolate import interp1d
    
    # Only use frequencies within the IR frequency range (with margin)
    # Exclude edge frequencies where interpolation artifacts occur
    f_max_compare = freq.max() * 0.95  # Use 95% of original data range
    valid_mask = (freq >= 0) & (freq <= f_max_compare)
    freq_valid = freq[valid_mask]
    S_valid = S_data[valid_mask]
    H_vf_valid = H_vf[valid_mask]
    
    logger.info(f"Comparison range: 0 - {f_max_compare/1e9:.2f} GHz ({len(freq_valid)} points)")
    
    interp_real = interp1d(freq_ir_pos, H_ir_pos.real, kind='linear', 
                           bounds_error=False, fill_value=0)
    interp_imag = interp1d(freq_ir_pos, H_ir_pos.imag, kind='linear',
                           bounds_error=False, fill_value=0)
    H_ir_interp = interp_real(freq_valid) + 1j * interp_imag(freq_valid)
    
    # IR metrics (compared to original)
    ir_mse = np.mean(np.abs(S_valid - H_ir_interp)**2)
    ir_max_error = np.max(np.abs(S_valid - H_ir_interp))
    
    logger.info(f"IR impulse length: {len(h_ir)}")
    logger.info(f"IR energy: {ir_result['energy']:.6f}")
    logger.info(f"IR peak time: {ir_result['peak_time']*1e12:.2f} ps")
    logger.info(f"IR DC gain: {ir_dc_gain:.6f}")
    logger.info(f"IR MSE (vs original): {ir_mse:.2e}")
    logger.info(f"IR max error: {ir_max_error:.2e}")
    
    results['h_ir'] = h_ir
    results['H_ir'] = H_ir_interp
    results['freq_valid'] = freq_valid
    results['ir_mse'] = ir_mse
    results['ir_dc_gain'] = ir_dc_gain
    
    # =========================================================================
    # Cross-comparison: VF vs IR
    # =========================================================================
    logger.info(f"\n--- VF vs IR Comparison ---")
    
    # Compare VF and IR at valid frequencies
    H_vf_at_valid = H_vf_valid
    
    vf_ir_mse = np.mean(np.abs(H_vf_at_valid - H_ir_interp)**2)
    vf_ir_max = np.max(np.abs(H_vf_at_valid - H_ir_interp))
    
    # Magnitude error in dB
    mag_vf = 20 * np.log10(np.abs(H_vf_at_valid) + 1e-12)
    mag_ir = 20 * np.log10(np.abs(H_ir_interp) + 1e-12)
    mag_error_db = np.max(np.abs(mag_vf - mag_ir))
    
    # Phase error in degrees
    phase_vf = np.unwrap(np.angle(H_vf_at_valid))
    phase_ir = np.unwrap(np.angle(H_ir_interp))
    phase_error_deg = np.max(np.abs(phase_vf - phase_ir)) * 180 / np.pi
    
    # DC gain difference
    dc_diff = np.abs(vf_dc_gain - ir_dc_gain)
    dc_diff_pct = dc_diff / (np.abs(ir_dc_gain) + 1e-12) * 100
    
    logger.info(f"VF-IR MSE: {vf_ir_mse:.2e}")
    logger.info(f"VF-IR max error: {vf_ir_max:.2e}")
    logger.info(f"Magnitude error: {mag_error_db:.2f} dB")
    logger.info(f"Phase error: {phase_error_deg:.2f} degrees")
    logger.info(f"DC gain difference: {dc_diff:.6f} ({dc_diff_pct:.2f}%)")
    
    results['vf_ir_mse'] = vf_ir_mse
    results['dc_diff_pct'] = dc_diff_pct
    results['mag_error_db'] = mag_error_db
    results['phase_error_deg'] = phase_error_deg
    
    # =========================================================================
    # Consistency Verdict
    # =========================================================================
    logger.info(f"\n" + "=" * 60)
    logger.info("CONSISTENCY VERDICT")
    logger.info("=" * 60)
    
    # Thresholds - adjusted for practical engineering use
    # Note: VF approximation error is the dominant factor
    MSE_THRESHOLD = 1e-3       # VF has inherent approximation error
    DC_DIFF_THRESHOLD = 2.0    # percent - both methods extrapolate DC differently  
    MAG_ERROR_THRESHOLD = 5.0  # dB - VF order limits fitting accuracy
    
    checks = {
        'VF-IR MSE < 1e-3': vf_ir_mse < MSE_THRESHOLD,
        'DC diff < 2%': dc_diff_pct < DC_DIFF_THRESHOLD,
        'Mag error < 5 dB': mag_error_db < MAG_ERROR_THRESHOLD,
    }
    
    for check, passed in checks.items():
        status = "PASS" if passed else "FAIL"
        logger.info(f"  [{status}] {check}")
    
    all_passed = all(checks.values())
    results['consistent'] = all_passed
    
    if all_passed:
        logger.info("\n>>> RESULT: VF and IR methods are CONSISTENT <<<")
    else:
        logger.info("\n>>> RESULT: VF and IR methods are NOT CONSISTENT <<<")
        logger.info(">>> Investigation needed to find root cause <<<")
    
    return results


def diagnose_vf_problem(freq, S_data, orders=[6, 8, 12, 16, 24, 32]):
    """
    Diagnose VF fitting problem by testing different orders.
    """
    logger.info("\n" + "=" * 60)
    logger.info("DIAGNOSING VF FITTING PROBLEM")
    logger.info("=" * 60)
    
    results = []
    for order in orders:
        vf = VectorFitting(order=order, max_iterations=20, tolerance=1e-10)
        vf_result = vf.fit(freq, S_data, enforce_stability=True)
        H_fit = vf.evaluate(freq)
        mse = np.mean(np.abs(S_data - H_fit)**2)
        max_err = np.max(np.abs(S_data - H_fit))
        
        results.append({
            'order': order,
            'mse': mse,
            'max_error': max_err,
            'dc_gain': vf.get_dc_gain()
        })
        logger.info(f"Order {order:2d}: MSE={mse:.2e}, max_error={max_err:.2e}, DC={vf.get_dc_gain():.4f}")
    
    return results


def generate_comparison_plot(results, output_path):
    """Generate comparison plots."""
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    freq = results['freq']
    S_orig = results['S_orig']
    H_vf = results['H_vf']
    freq_valid = results['freq_valid']
    H_ir = results['H_ir']
    
    # Plot 1: Magnitude
    ax = axes[0, 0]
    ax.semilogx(freq/1e9, 20*np.log10(np.abs(S_orig)), 'b-', label='Original', linewidth=2)
    ax.semilogx(freq/1e9, 20*np.log10(np.abs(H_vf)+1e-12), 'r--', label='VF', linewidth=1.5)
    ax.semilogx(freq_valid/1e9, 20*np.log10(np.abs(H_ir)+1e-12), 'g:', label='IR', linewidth=1.5)
    ax.set_xlabel('Frequency (GHz)')
    ax.set_ylabel('Magnitude (dB)')
    ax.set_title('Frequency Response - Magnitude')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Plot 2: Phase
    ax = axes[0, 1]
    ax.semilogx(freq/1e9, np.unwrap(np.angle(S_orig))*180/np.pi, 'b-', label='Original', linewidth=2)
    ax.semilogx(freq/1e9, np.unwrap(np.angle(H_vf))*180/np.pi, 'r--', label='VF', linewidth=1.5)
    ax.semilogx(freq_valid/1e9, np.unwrap(np.angle(H_ir))*180/np.pi, 'g:', label='IR', linewidth=1.5)
    ax.set_xlabel('Frequency (GHz)')
    ax.set_ylabel('Phase (degrees)')
    ax.set_title('Frequency Response - Phase')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Plot 3: Impulse Response
    ax = axes[1, 0]
    h_ir = results['h_ir']
    dt = 1.0 / 100e9
    time_ps = np.arange(len(h_ir)) * dt * 1e12
    ax.plot(time_ps, h_ir, 'b-', linewidth=1)
    ax.set_xlabel('Time (ps)')
    ax.set_ylabel('Amplitude')
    ax.set_title('Impulse Response (from IR method)')
    ax.grid(True, alpha=0.3)
    ax.set_xlim([0, min(1000, time_ps[-1])])
    
    # Plot 4: Error comparison
    ax = axes[1, 1]
    # Compute errors at valid frequencies
    S_valid = S_orig[:len(freq_valid)]
    H_vf_valid = H_vf[:len(freq_valid)]
    
    error_vf = np.abs(S_valid - H_vf_valid)
    error_ir = np.abs(S_valid - H_ir)
    
    ax.semilogy(freq_valid/1e9, error_vf, 'r-', label='VF error', linewidth=1)
    ax.semilogy(freq_valid/1e9, error_ir, 'g-', label='IR error', linewidth=1)
    ax.set_xlabel('Frequency (GHz)')
    ax.set_ylabel('Absolute Error')
    ax.set_title('Fitting Error Comparison')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    plt.close()
    logger.info(f"Comparison plot saved to: {output_path}")


def main():
    s4p_file = "/Users/liuyizhe/Developer/systemCprojects/peters_01_0605_B12_thru.s4p"
    output_dir = "/tmp/vf_ir_comparison"
    os.makedirs(output_dir, exist_ok=True)
    
    # Compare for S21
    results = compare_methods(s4p_file, sparam='S21', vf_order=12, ir_samples=4096, fs=100e9)
    
    # Generate plots
    generate_comparison_plot(results, os.path.join(output_dir, 'comparison_S21.png'))
    
    # If not consistent, diagnose VF problem
    if not results['consistent']:
        logger.info("\n" + "=" * 60)
        logger.info("INVESTIGATING ROOT CAUSE")
        logger.info("=" * 60)
        
        reader = TouchstoneReader(s4p_file)
        freq, s_matrix = reader.read()
        S21 = reader.get_sparam(2, 1)
        
        # Test different VF orders
        vf_diagnosis = diagnose_vf_problem(freq, S21)
        
        # Check if problem is VF order
        best_result = min(vf_diagnosis, key=lambda x: x['mse'])
        logger.info(f"\nBest VF result: order={best_result['order']}, MSE={best_result['mse']:.2e}")
        
        if best_result['mse'] > 1e-3:
            logger.info("\n>>> ROOT CAUSE HYPOTHESIS: VF algorithm may have convergence issues <<<")
            logger.info(">>> Possible fixes: ")
            logger.info("    1. Improve pole initialization")
            logger.info("    2. Increase iteration count")
            logger.info("    3. Adjust tolerance")
            logger.info("    4. Check S-parameter data quality")


if __name__ == '__main__':
    main()
