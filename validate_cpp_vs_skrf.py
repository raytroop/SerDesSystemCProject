#!/usr/bin/env python3
"""
Validate C++ PoleResidueFilter against scikit-rf VectorFitting.

This script compares the frequency response of the C++ implementation
with the analytical calculation from scikit-rf pole-residue representation.

Usage:
    python validate_cpp_vs_skrf.py <config.json> <cpp_response.csv> <s4p_file>

Output:
    - Correlation coefficient between C++ and scikit-rf responses
    - RMSE in dB
    - Max error in dB
    - Pass/fail status based on thresholds
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import skrf


def load_cpp_response(csv_file):
    """Load C++ frequency response from CSV."""
    data = np.loadtxt(csv_file, delimiter=',', skiprows=1)
    return {
        'freq': data[:, 0],
        'mag': data[:, 1],
        'mag_db': data[:, 2],
        'phase': data[:, 3],
        'real': data[:, 4],
        'imag': data[:, 5]
    }


def compute_skrf_response(config_file, frequencies):
    """Compute frequency response from scikit-rf pole-residue representation."""
    with open(config_file) as f:
        config = json.load(f)
    
    pr = config['pole_residue']
    poles = np.array(pr['poles_real']) + 1j * np.array(pr['poles_imag'])
    residues = np.array(pr['residues_real']) + 1j * np.array(pr['residues_imag'])
    constant = pr['constant']
    proportional = pr['proportional']
    
    # Compute H(f) = sum(r_i / (j*2*pi*f - p_i)) + constant + proportional*j*2*pi*f
    s = 1j * 2 * np.pi * frequencies
    
    h = np.zeros_like(s, dtype=complex)
    for r, p in zip(residues, poles):
        h += r / (s - p)
    
    h += constant + proportional * s
    
    return {
        'freq': frequencies,
        'mag': np.abs(h),
        'mag_db': 20 * np.log10(np.abs(h) + 1e-20),
        'phase': np.angle(h, deg=True),
        'real': np.real(h),
        'imag': np.imag(h)
    }


def compute_metrics(cpp_resp, skrf_resp):
    """Compute comparison metrics between C++ and scikit-rf responses."""
    # Ensure same frequency points
    assert np.allclose(cpp_resp['freq'], skrf_resp['freq']), "Frequency points must match"
    
    # Magnitude correlation (in linear scale for better accuracy)
    mag_corr = np.corrcoef(cpp_resp['mag'], skrf_resp['mag'])[0, 1]
    
    # Magnitude correlation in dB
    mag_db_corr = np.corrcoef(cpp_resp['mag_db'], skrf_resp['mag_db'])[0, 1]
    
    # RMSE in dB
    rmse_db = np.sqrt(np.mean((cpp_resp['mag_db'] - skrf_resp['mag_db'])**2))
    
    # Max error in dB
    max_error_db = np.max(np.abs(cpp_resp['mag_db'] - skrf_resp['mag_db']))
    
    # Phase correlation (unwrap phase first)
    cpp_phase_unwrapped = np.unwrap(np.radians(cpp_resp['phase']))
    skrf_phase_unwrapped = np.unwrap(np.radians(skrf_resp['phase']))
    phase_corr = np.corrcoef(cpp_phase_unwrapped, skrf_phase_unwrapped)[0, 1]
    
    return {
        'magnitude_correlation_linear': mag_corr,
        'magnitude_correlation_db': mag_db_corr,
        'phase_correlation': phase_corr,
        'rmse_db': rmse_db,
        'max_error_db': max_error_db
    }


def validate_against_s4p(cpp_resp, s4p_file):
    """Validate C++ response against original S4P file."""
    nw = skrf.Network(s4p_file)
    
    # Get S21
    freq_orig = nw.frequency.f
    s21_orig = nw.s[:, 1, 0]
    s21_mag_orig = np.abs(s21_orig)
    s21_mag_db_orig = 20 * np.log10(s21_mag_orig + 1e-20)
    
    # Interpolate to C++ frequency points
    s21_mag_db_interp = np.interp(cpp_resp['freq'], freq_orig, s21_mag_db_orig)
    
    # Compute correlation
    corr = np.corrcoef(cpp_resp['mag_db'], s21_mag_db_interp)[0, 1]
    rmse = np.sqrt(np.mean((cpp_resp['mag_db'] - s21_mag_db_interp)**2))
    max_error = np.max(np.abs(cpp_resp['mag_db'] - s21_mag_db_interp))
    
    return {
        'correlation': corr,
        'rmse_db': rmse,
        'max_error_db': max_error
    }


def main():
    parser = argparse.ArgumentParser(
        description='Validate C++ PoleResidueFilter against scikit-rf'
    )
    parser.add_argument('config_json', help='Pole-residue JSON config file')
    parser.add_argument('cpp_csv', help='C++ frequency response CSV file')
    parser.add_argument('s4p_file', help='Original S4P file')
    parser.add_argument('--output', '-o', default='validation_results.json',
                        help='Output JSON file for results')
    parser.add_argument('--plot', '-p', action='store_true',
                        help='Generate comparison plots')
    
    args = parser.parse_args()
    
    print("=" * 60)
    print("C++ PoleResidueFilter Validation (Scheme C)")
    print("=" * 60)
    
    # Load C++ response
    print("\n[1] Loading C++ frequency response...")
    cpp_resp = load_cpp_response(args.cpp_csv)
    print(f"    Loaded {len(cpp_resp['freq'])} frequency points")
    print(f"    Range: {cpp_resp['freq'][0]/1e6:.2f} MHz to {cpp_resp['freq'][-1]/1e9:.2f} GHz")
    
    # Compute scikit-rf response
    print("\n[2] Computing scikit-rf analytical response...")
    skrf_resp = compute_skrf_response(args.config_json, cpp_resp['freq'])
    print(f"    Computed using pole-residue representation")
    
    # Compare C++ vs scikit-rf
    print("\n[3] Comparing C++ vs scikit-rf...")
    metrics_cpp_vs_skrf = compute_metrics(cpp_resp, skrf_resp)
    
    print("    Magnitude Correlation (linear): {:.6f}".format(
        metrics_cpp_vs_skrf['magnitude_correlation_linear']))
    print("    Magnitude Correlation (dB):     {:.6f}".format(
        metrics_cpp_vs_skrf['magnitude_correlation_db']))
    print("    Phase Correlation:              {:.6f}".format(
        metrics_cpp_vs_skrf['phase_correlation']))
    print("    RMSE (dB):                      {:.4f}".format(
        metrics_cpp_vs_skrf['rmse_db']))
    print("    Max Error (dB):                 {:.4f}".format(
        metrics_cpp_vs_skrf['max_error_db']))
    
    # Validate against original S4P
    print("\n[4] Validating against original S4P file...")
    metrics_vs_s4p = validate_against_s4p(cpp_resp, args.s4p_file)
    
    print("    Correlation:   {:.6f}".format(metrics_vs_s4p['correlation']))
    print("    RMSE (dB):     {:.4f}".format(metrics_vs_s4p['rmse_db']))
    print("    Max Error (dB): {:.4f}".format(metrics_vs_s4p['max_error_db']))
    
    # Apply thresholds
    print("\n[5] Applying thresholds...")
    thresholds = {
        'correlation': 0.95,
        'rmse_db': 3.0,
        'max_error_db': 6.0
    }
    
    results = {
        'scheme': 'C',
        'description': 'C++ PoleResidueFilter vs scikit-rf VectorFitting',
        'cpp_vs_skrf': metrics_cpp_vs_skrf,
        'cpp_vs_s4p': metrics_vs_s4p,
        'thresholds': thresholds,
        'pass': {
            'correlation_cpp_vs_s4p': metrics_vs_s4p['correlation'] > thresholds['correlation'],
            'rmse_cpp_vs_s4p': metrics_vs_s4p['rmse_db'] < thresholds['rmse_db'],
            'max_error_cpp_vs_s4p': metrics_vs_s4p['max_error_db'] < thresholds['max_error_db'],
            'correlation_cpp_vs_skrf': metrics_cpp_vs_skrf['magnitude_correlation_linear'] > thresholds['correlation']
        }
    }
    
    # Overall pass/fail (convert numpy bool to Python bool)
    overall_pass = bool(all(results['pass'].values()))
    results['overall_pass'] = overall_pass
    # Convert all numpy bools to Python bools for JSON serialization
    for key in results['pass']:
        results['pass'][key] = bool(results['pass'][key])
    
    # Print results
    print("\n" + "=" * 60)
    print("VALIDATION RESULTS")
    print("=" * 60)
    
    print("\nC++ vs scikit-rf (implementation consistency):")
    print("  Magnitude Correlation (linear): {:.6f} {}".format(
        metrics_cpp_vs_skrf['magnitude_correlation_linear'],
        "✓ PASS" if metrics_cpp_vs_skrf['magnitude_correlation_linear'] > thresholds['correlation'] else "✗ FAIL"
    ))
    
    print("\nC++ vs Original S4P (accuracy):")
    print("  Correlation:   {:.6f} {}".format(
        metrics_vs_s4p['correlation'],
        "✓ PASS" if metrics_vs_s4p['correlation'] > thresholds['correlation'] else "✗ FAIL"
    ))
    print("  RMSE (dB):     {:.4f} {}".format(
        metrics_vs_s4p['rmse_db'],
        "✓ PASS" if metrics_vs_s4p['rmse_db'] < thresholds['rmse_db'] else "✗ FAIL"
    ))
    print("  Max Error (dB): {:.4f} {}".format(
        metrics_vs_s4p['max_error_db'],
        "✓ PASS" if metrics_vs_s4p['max_error_db'] < thresholds['max_error_db'] else "✗ FAIL"
    ))
    
    print("\n" + "=" * 60)
    if overall_pass:
        print("OVERALL: ✓ PASS - C++ implementation meets requirements")
    else:
        print("OVERALL: ✗ FAIL - C++ implementation does not meet requirements")
    print("=" * 60)
    
    # Save results
    with open(args.output, 'w') as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to: {args.output}")
    
    # Generate plots if requested
    if args.plot:
        try:
            import matplotlib.pyplot as plt
            
            fig, axes = plt.subplots(2, 2, figsize=(14, 10))
            
            # Plot 1: Magnitude comparison (dB)
            ax = axes[0, 0]
            ax.semilogx(cpp_resp['freq'] / 1e9, cpp_resp['mag_db'], 'b-', label='C++', linewidth=2)
            ax.semilogx(skrf_resp['freq'] / 1e9, skrf_resp['mag_db'], 'r--', label='scikit-rf', linewidth=1.5)
            ax.set_xlabel('Frequency (GHz)')
            ax.set_ylabel('Magnitude (dB)')
            ax.set_title('Magnitude Response Comparison')
            ax.legend()
            ax.grid(True, which='both', alpha=0.3)
            
            # Plot 2: Phase comparison
            ax = axes[0, 1]
            ax.semilogx(cpp_resp['freq'] / 1e9, cpp_resp['phase'], 'b-', label='C++', linewidth=2)
            ax.semilogx(skrf_resp['freq'] / 1e9, skrf_resp['phase'], 'r--', label='scikit-rf', linewidth=1.5)
            ax.set_xlabel('Frequency (GHz)')
            ax.set_ylabel('Phase (degrees)')
            ax.set_title('Phase Response Comparison')
            ax.legend()
            ax.grid(True, which='both', alpha=0.3)
            
            # Plot 3: Error in dB
            ax = axes[1, 0]
            error_db = cpp_resp['mag_db'] - skrf_resp['mag_db']
            ax.semilogx(cpp_resp['freq'] / 1e9, error_db, 'g-', linewidth=1)
            ax.axhline(y=0, color='k', linestyle='--', alpha=0.5)
            ax.axhline(y=thresholds['rmse_db'], color='r', linestyle=':', alpha=0.5, label=f"RMSE threshold ({thresholds['rmse_db']} dB)")
            ax.axhline(y=-thresholds['rmse_db'], color='r', linestyle=':', alpha=0.5)
            ax.set_xlabel('Frequency (GHz)')
            ax.set_ylabel('Error (dB)')
            ax.set_title('Magnitude Error (C++ - scikit-rf)')
            ax.legend()
            ax.grid(True, which='both', alpha=0.3)
            
            # Plot 4: C++ vs S4P
            ax = axes[1, 1]
            nw = skrf.Network(args.s4p_file)
            freq_orig = nw.frequency.f
            s21_mag_db_orig = 20 * np.log10(np.abs(nw.s[:, 1, 0]) + 1e-20)
            ax.semilogx(freq_orig / 1e9, s21_mag_db_orig, 'k-', label='Original S4P', linewidth=1.5, alpha=0.7)
            ax.semilogx(cpp_resp['freq'] / 1e9, cpp_resp['mag_db'], 'b-', label='C++', linewidth=2)
            ax.set_xlabel('Frequency (GHz)')
            ax.set_ylabel('Magnitude (dB)')
            ax.set_title('C++ vs Original S4P')
            ax.legend()
            ax.grid(True, which='both', alpha=0.3)
            
            plt.tight_layout()
            plot_file = args.output.replace('.json', '.png')
            plt.savefig(plot_file, dpi=150)
            print(f"Plot saved to: {plot_file}")
            plt.close()
        except Exception as e:
            print(f"Warning: Failed to generate plot: {e}")
    
    return 0 if overall_pass else 1


if __name__ == '__main__':
    sys.exit(main())
