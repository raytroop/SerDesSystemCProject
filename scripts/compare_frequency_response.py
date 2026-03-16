#!/usr/bin/env python3
"""
Compare frequency response between original S-parameters and simulation output.
Batch 3: RATIONAL method validation.
"""

import numpy as np
import matplotlib.pyplot as plt
import json
import sys
from pathlib import Path

# Add scripts directory to path
sys.path.insert(0, str(Path(__file__).parent))
from process_sparam import TouchstoneReader

def load_simulation_output(csv_file):
    """Load simulation output and extract frequency response."""
    data = np.loadtxt(csv_file, delimiter=',', skiprows=1)
    time = data[:, 0]
    input_signal = data[:, 1]
    output_signal = data[:, 2]
    
    # Compute FFT
    dt = time[1] - time[0]
    fs = 1.0 / dt
    
    N = len(time)
    fft_input = np.fft.fft(input_signal)
    fft_output = np.fft.fft(output_signal)
    
    # Frequency axis
    freqs = np.fft.fftfreq(N, dt)
    
    # Transfer function
    H_sim = fft_output / (fft_input + 1e-12)
    
    # Keep only positive frequencies
    positive = freqs > 0
    return freqs[positive], H_sim[positive]

def compare_frequency_response(s4p_file, csv_file, output_dir='.'):
    """Compare original S21 with simulation result."""
    print("="*60)
    print("Batch 3: RATIONAL Method Frequency Response Validation")
    print("="*60)
    
    # Load original S-parameters
    print(f"\n[1/4] Loading original S-parameters from {s4p_file}")
    reader = TouchstoneReader(s4p_file)
    freq_orig, s_matrix = reader.read()
    S21_orig = s_matrix[:, 1, 0]  # S21 = S[1,0] (0-indexed)
    
    print(f"  Frequency range: {freq_orig[0]/1e9:.2f} - {freq_orig[-1]/1e9:.2f} GHz")
    print(f"  Data points: {len(freq_orig)}")
    
    # Load simulation output
    print(f"\n[2/4] Loading simulation output from {csv_file}")
    freq_sim, H_sim = load_simulation_output(csv_file)
    
    print(f"  Frequency resolution: {freq_sim[1]/1e6:.2f} MHz")
    print(f"  Max frequency: {freq_sim[-1]/1e9:.2f} GHz")
    
    # Interpolate to common frequency grid
    print(f"\n[3/4] Interpolating to common grid")
    f_min = max(freq_orig[0], freq_sim[0])
    f_max = min(freq_orig[-1], freq_sim[-1])
    
    freq_common = np.linspace(f_min, f_max, 500)
    
    # Interpolate original S21
    S21_interp = np.interp(freq_common, freq_orig, np.abs(S21_orig))
    S21_phase_interp = np.interp(freq_common, freq_orig, np.unwrap(np.angle(S21_orig)))
    
    # Interpolate simulation result
    H_interp = np.interp(freq_common, freq_sim, np.abs(H_sim))
    H_phase_interp = np.interp(freq_common, freq_sim, np.unwrap(np.angle(H_sim)))
    
    # Calculate errors
    mag_error_db = 20 * np.log10(H_interp / (S21_interp + 1e-12))
    mag_rmse_db = np.sqrt(np.mean(mag_error_db**2))
    mag_max_error_db = np.max(np.abs(mag_error_db))
    
    # Correlation
    correlation = np.corrcoef(np.abs(S21_interp), H_interp)[0, 1]
    
    print(f"\n[4/4] Validation Results")
    print(f"  Magnitude RMSE: {mag_rmse_db:.2f} dB")
    print(f"  Max magnitude error: {mag_max_error_db:.2f} dB")
    print(f"  Correlation: {correlation:.4f}")
    
    # Plot
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    # Magnitude
    ax = axes[0, 0]
    ax.semilogx(freq_common/1e9, 20*np.log10(S21_interp), 'b-', label='Original S21', linewidth=2)
    ax.semilogx(freq_common/1e9, 20*np.log10(H_interp), 'r--', label='Simulation', linewidth=2)
    ax.set_xlabel('Frequency (GHz)')
    ax.set_ylabel('Magnitude (dB)')
    ax.set_title('Magnitude Response Comparison')
    ax.legend()
    ax.grid(True, which='both', alpha=0.3)
    
    # Phase
    ax = axes[0, 1]
    ax.semilogx(freq_common/1e9, np.degrees(S21_phase_interp), 'b-', label='Original S21', linewidth=2)
    ax.semilogx(freq_common/1e9, np.degrees(H_phase_interp), 'r--', label='Simulation', linewidth=2)
    ax.set_xlabel('Frequency (GHz)')
    ax.set_ylabel('Phase (degrees)')
    ax.set_title('Phase Response Comparison')
    ax.legend()
    ax.grid(True, which='both', alpha=0.3)
    
    # Magnitude error
    ax = axes[1, 0]
    ax.semilogx(freq_common/1e9, mag_error_db, 'g-', linewidth=1)
    ax.axhline(y=0, color='k', linestyle='--', alpha=0.5)
    ax.set_xlabel('Frequency (GHz)')
    ax.set_ylabel('Magnitude Error (dB)')
    ax.set_title(f'Magnitude Error (RMSE: {mag_rmse_db:.2f} dB)')
    ax.grid(True, which='both', alpha=0.3)
    
    # Group delay
    ax = axes[1, 1]
    group_delay_orig = -np.gradient(S21_phase_interp, freq_common) / (2*np.pi)
    group_delay_sim = -np.gradient(H_phase_interp, freq_common) / (2*np.pi)
    ax.semilogx(freq_common/1e9, group_delay_orig*1e12, 'b-', label='Original', linewidth=2)
    ax.semilogx(freq_common/1e9, group_delay_sim*1e12, 'r--', label='Simulation', linewidth=2)
    ax.set_xlabel('Frequency (GHz)')
    ax.set_ylabel('Group Delay (ps)')
    ax.set_title('Group Delay Comparison')
    ax.legend()
    ax.grid(True, which='both', alpha=0.3)
    
    plt.tight_layout()
    plot_file = Path(output_dir) / 'batch3_frequency_response_comparison.png'
    plt.savefig(plot_file, dpi=150)
    print(f"\n  Plot saved to: {plot_file}")
    
    # Save results
    results = {
        'batch': 'Batch 3: RATIONAL Method Validation',
        'metrics': {
            'magnitude_rmse_db': float(mag_rmse_db),
            'magnitude_max_error_db': float(mag_max_error_db),
            'correlation': float(correlation),
            'frequency_range_hz': [float(f_min), float(f_max)]
        },
        'thresholds': {
            'magnitude_rmse_db': 3.0,
            'magnitude_max_error_db': 6.0,
            'correlation': 0.95
        },
        'pass': {
            'magnitude_rmse': bool(mag_rmse_db <= 3.0),
            'magnitude_max_error': bool(mag_max_error_db <= 6.0),
            'correlation': bool(correlation >= 0.95)
        }
    }
    
    results_file = Path(output_dir) / 'batch3_validation_results.json'
    with open(results_file, 'w') as f:
        json.dump(results, f, indent=2)
    print(f"  Results saved to: {results_file}")
    
    # Overall verdict
    print("\n" + "="*60)
    print("VALIDATION SUMMARY")
    print("="*60)
    all_pass = all(results['pass'].values())
    for test, passed in results['pass'].items():
        status = "✅ PASS" if passed else "❌ FAIL"
        print(f"  {test}: {status}")
    print("="*60)
    if all_pass:
        print("🎉 OVERALL: PASS - RATIONAL method validated successfully!")
    else:
        print("⚠️ OVERALL: FAIL - Some validation criteria not met")
    print("="*60)
    
    return all_pass

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Batch 3: RATIONAL method validation')
    parser.add_argument('--s4p', default='peters_01_0605_B12_thru.s4p', help='Input S4P file')
    parser.add_argument('--csv', default='channel_sparam_waveform.csv', help='Simulation CSV output')
    parser.add_argument('--output-dir', default='.', help='Output directory')
    args = parser.parse_args()
    
    success = compare_frequency_response(args.s4p, args.csv, args.output_dir)
    sys.exit(0 if success else 1)
