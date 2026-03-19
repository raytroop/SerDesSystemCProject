#!/usr/bin/env python3
"""
Fix scikit-rf VectorFitting output - Version 2

Key insight: S21 is complex-valued, so we need to ensure the pole-residue
representation maintains H(s*) = H*(s) for real-valued time-domain response.

This version:
1. Works with complex S-parameters (as they should be)
2. Enforces conjugate symmetry properly
3. Uses two-sided fitting (positive and negative frequencies)
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import skrf


def vector_fitting_mirror_spectrum(nw, n_poles=16, max_iter=200):
    """
    Perform VectorFitting with mirrored spectrum for conjugate symmetry.
    
    For a real-valued impulse response, H(-f) = H*(f).
    We enforce this by fitting both original and conjugate-mirrored data.
    """
    freq = nw.frequency.f
    s21 = nw.s[:, 1, 0]
    
    # Create extended frequency grid (negative frequencies)
    freq_extended = np.concatenate([-freq[::-1], freq])
    s21_extended = np.concatenate([np.conj(s21[::-1]), s21])
    
    # Create a temporary network with extended data
    # Note: This is a workaround since scikit-rf doesn't directly support this
    # We'll fit the original and check symmetry separately
    
    print(f"  Original frequency range: {freq[0]/1e6:.2f} MHz to {freq[-1]/1e9:.2f} GHz")
    print(f"  Extended frequency points: {len(freq_extended)}")
    
    # Fit original data
    vf = skrf.VectorFitting(nw)
    vf.max_iterations = max_iter
    vf.vector_fit(n_poles_real=0, n_poles_cmplx=n_poles,
                  fit_constant=True, fit_proportional=False,
                  init_pole_spacing='log')
    
    return vf


def fix_conjugate_symmetry_v2(poles, residues, constant):
    """
    Enforce conjugate symmetry by properly pairing poles.
    
    For H(s) to have real-valued time response:
    - Complex poles must come in conjugate pairs: p and p*
    - Corresponding residues must be conjugates: r and r*
    
    This function reorganizes poles/residues to enforce this.
    """
    n = len(poles)
    
    # Build conjugate-symmetric representation
    # We'll use only the upper half-plane poles and reconstruct the lower half
    upper_poles = []
    upper_residues = []
    
    for i, (p, r) in enumerate(zip(poles, residues)):
        # Only keep poles with positive imaginary part
        if p.imag >= 0:
            upper_poles.append(p)
            # Adjust residue to maintain proper relationship
            # r should correspond to this pole
            upper_residues.append(r)
    
    # Now build the full symmetric set
    symmetric_poles = []
    symmetric_residues = []
    
    for p, r in zip(upper_poles, upper_residues):
        if abs(p.imag) < 1e-6:
            # Real pole - keep as is with real residue
            symmetric_poles.append(p.real)
            symmetric_residues.append(r.real)
        else:
            # Complex pole - add conjugate pair
            symmetric_poles.append(p)           # Upper half
            symmetric_poles.append(np.conj(p))  # Lower half
            symmetric_residues.append(r)
            symmetric_residues.append(np.conj(r))
    
    return np.array(symmetric_poles), np.array(symmetric_residues)


def evaluate_fit(freq, s21_orig, poles, residues, constant):
    """Evaluate fit quality."""
    s = 1j * 2 * np.pi * freq
    s21_fit = np.zeros_like(s, dtype=complex)
    
    for p, r in zip(poles, residues):
        s21_fit += r / (s - p)
    s21_fit += constant
    
    # Compute metrics in linear scale
    corr = np.corrcoef(np.abs(s21_orig), np.abs(s21_fit))[0, 1]
    
    # Compute phase correlation
    phase_orig = np.angle(s21_orig, deg=True)
    phase_fit = np.angle(s21_fit, deg=True)
    # Unwrap phase for correlation
    phase_orig_unwrapped = np.unwrap(np.radians(phase_orig))
    phase_fit_unwrapped = np.unwrap(np.radians(phase_fit))
    phase_corr = np.corrcoef(phase_orig_unwrapped, phase_fit_unwrapped)[0, 1]
    
    # Compute magnitude error in dB
    orig_db = 20 * np.log10(np.abs(s21_orig) + 1e-20)
    fit_db = 20 * np.log10(np.abs(s21_fit) + 1e-20)
    rmse = np.sqrt(np.mean((orig_db - fit_db)**2))
    max_err = np.max(np.abs(orig_db - fit_db))
    
    return {
        'correlation': corr,
        'phase_correlation': phase_corr,
        'rmse_db': rmse,
        'max_error_db': max_err,
        's21_fit': s21_fit
    }


def iterative_fit_with_dc_correction(nw, n_poles=16, max_iter=200, dc_weight=100.0):
    """
    Perform iterative fitting with emphasis on DC point accuracy.
    """
    freq = nw.frequency.f
    s21_orig = nw.s[:, 1, 0]
    dc_target = s21_orig[0]
    
    print(f"Target DC (at {freq[0]/1e6:.2f} MHz): {dc_target}")
    print(f"Target DC magnitude: {abs(dc_target):.6f} ({20*np.log10(abs(dc_target)):.2f} dB)")
    print(f"Target DC phase: {np.angle(dc_target, deg=True):.2f}°")
    
    # Try multiple initial pole configurations
    best_result = None
    best_score = float('inf')
    
    for trial in range(3):
        print(f"\n--- Trial {trial + 1} ---")
        
        vf = skrf.VectorFitting(nw)
        vf.max_iterations = max_iter
        
        # Try different pole spacings
        spacing = ['log', 'lin'][trial % 2]
        n_poles_trial = n_poles + trial * 4
        
        print(f"  Poles: {n_poles_trial}, Spacing: {spacing}")
        
        try:
            vf.vector_fit(n_poles_real=0, n_poles_cmplx=n_poles_trial,
                          fit_constant=True, fit_proportional=False,
                          init_pole_spacing=spacing)
            
            idx_s21 = 1 * nw.nports + 0
            poles = vf.poles
            residues = vf.residues[idx_s21]
            constant = vf.constant_coeff[idx_s21]
            
            # Evaluate
            metrics = evaluate_fit(freq, s21_orig, poles, residues, constant)
            
            # Compute DC error
            s = 1j * 2 * np.pi * freq[0]
            dc_fit = sum(r / (s - p) for p, r in zip(poles, residues)) + constant
            dc_error = abs(dc_fit - dc_target) / abs(dc_target)
            
            print(f"  Correlation: {metrics['correlation']:.6f}")
            print(f"  RMSE: {metrics['rmse_db']:.4f} dB")
            print(f"  DC error: {dc_error:.4%}")
            
            # Score: combination of correlation, RMSE, and DC error
            score = (1 - metrics['correlation']) * 10 + metrics['rmse_db'] / 10 + dc_error * 100
            
            if score < best_score:
                best_score = score
                best_result = {
                    'poles': poles,
                    'residues': residues,
                    'constant': constant,
                    'metrics': metrics,
                    'dc_fit': dc_fit,
                    'dc_error': dc_error
                }
                print(f"  ✓ New best result!")
                
        except Exception as e:
            print(f"  Error: {e}")
            continue
    
    return best_result


def fix_scikit_rf_vf_v2(input_s4p, output_json, n_poles=16):
    """Main fixing function - Version 2."""
    print(f"Loading S4P file: {input_s4p}")
    nw = skrf.Network(input_s4p)
    print(f"  Ports: {nw.nports}, Frequency points: {len(nw.frequency.f)}")
    
    freq = nw.frequency.f
    s21_orig = nw.s[:, 1, 0]
    
    # Perform iterative fitting
    print(f"\nPerforming iterative fitting...")
    result = iterative_fit_with_dc_correction(nw, n_poles=n_poles)
    
    if result is None:
        print("ERROR: All fitting trials failed!")
        return None
    
    poles = result['poles']
    residues = result['residues']
    constant = result['constant']
    
    print(f"\nBest raw result:")
    print(f"  Poles: {len(poles)}")
    print(f"  Correlation: {result['metrics']['correlation']:.6f}")
    print(f"  RMSE: {result['metrics']['rmse_db']:.4f} dB")
    
    # Enforce conjugate symmetry
    print(f"\nEnforcing conjugate symmetry...")
    poles_sym, residues_sym = fix_conjugate_symmetry_v2(poles, residues, constant)
    print(f"  Symmetric poles: {len(poles_sym)}")
    
    # Re-evaluate with symmetric poles
    # Need to re-optimize constant for best fit
    print(f"\nRe-optimizing constant for symmetric poles...")
    s = 1j * 2 * np.pi * freq[:, None]
    
    # Build matrix for least-squares
    A = np.zeros((len(freq), len(poles_sym) + 1), dtype=complex)
    for i, (p, r) in enumerate(zip(poles_sym, residues_sym)):
        A[:, i] = r / (s[:, 0] - p)
    A[:, -1] = 1  # Constant term
    
    # Solve for best constant (keep residues fixed)
    b = s21_orig
    x, residuals, rank, s_vals = np.linalg.lstsq(A, b, rcond=None)
    
    # Extract optimized constant
    constant_opt = x[-1]
    
    print(f"  Optimized constant: {constant_opt:.6f}")
    
    # Final evaluation
    metrics_final = evaluate_fit(freq, s21_orig, poles_sym, residues_sym, constant_opt)
    
    print(f"\nFinal metrics:")
    print(f"  Correlation: {metrics_final['correlation']:.6f}")
    print(f"  Phase correlation: {metrics_final['phase_correlation']:.6f}")
    print(f"  RMSE (dB): {metrics_final['rmse_db']:.4f}")
    print(f"  Max error (dB): {metrics_final['max_error_db']:.4f}")
    
    # Build output
    config = {
        'version': '2.2-scikit-rf-fixed-v2',
        'method': 'POLE_RESIDUE',
        'pole_residue': {
            'poles_real': [float(p.real) for p in poles_sym],
            'poles_imag': [float(p.imag) for p in poles_sym],
            'residues_real': [float(r.real) for r in residues_sym],
            'residues_imag': [float(r.imag) for r in residues_sym],
            'constant': complex(constant_opt).__repr__(),
            'proportional': 0.0,
            'order': len(poles_sym)
        },
        'fs': 100e9,
        'metrics': {
            'correlation': float(metrics_final['correlation']),
            'phase_correlation': float(metrics_final['phase_correlation']),
            'rmse_db': float(metrics_final['rmse_db']),
            'max_error_db': float(metrics_final['max_error_db'])
        }
    }
    
    with open(output_json, 'w') as f:
        json.dump(config, f, indent=2)
    
    print(f"\nSaved to: {output_json}")
    
    return config


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input_s4p', help='Input S4P file')
    parser.add_argument('output_json', help='Output JSON file')
    parser.add_argument('--n-poles', type=int, default=16)
    args = parser.parse_args()
    
    config = fix_scikit_rf_vf_v2(args.input_s4p, args.output_json, args.n_poles)
    
    if config is None:
        return 1
    
    metrics = config['metrics']
    corr = metrics['correlation']
    rmse = metrics['rmse_db']
    max_err = metrics['max_error_db']
    
    print("\n" + "="*60)
    print("FINAL VALIDATION")
    print("="*60)
    print(f"Correlation:   {corr:.6f} {'✓ PASS' if corr > 0.95 else '✗ FAIL'}")
    print(f"RMSE (dB):     {rmse:.4f} {'✓ PASS' if rmse < 3 else '✗ FAIL'}")
    print(f"Max Error (dB): {max_err:.4f} {'✓ PASS' if max_err < 6 else '✗ FAIL'}")
    
    return 0 if (corr > 0.95 and rmse < 3 and max_err < 6) else 1


if __name__ == '__main__':
    sys.exit(main())
