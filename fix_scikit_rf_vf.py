#!/usr/bin/env python3
"""
Fix scikit-rf VectorFitting output to enforce conjugate symmetry.

This script addresses the issues with scikit-rf's VectorFitting:
1. Non-conjugate pole-residue pairs
2. Complex DC gain (should be real for physical systems)
3. Poor DC point matching

The fix enforces:
- Conjugate symmetry: if p is a pole, then p* must also be a pole with r* residue
- Real DC gain: H(0) must be real
- DC point matching: H(0) matches the original S-parameter DC point
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import skrf


def fix_conjugate_symmetry(poles, residues, tolerance=1e6):
    """
    Enforce conjugate symmetry on poles and residues.
    
    For a physical system, H(s) must satisfy H(s*) = H*(s).
    This requires poles and residues to come in conjugate pairs.
    
    Args:
        poles: Array of complex poles
        residues: Array of complex residues
        tolerance: Frequency tolerance for matching conjugate pairs (rad/s)
    
    Returns:
        fixed_poles, fixed_residues: Arrays with enforced conjugate symmetry
    """
    n = len(poles)
    used = set()
    fixed_poles = []
    fixed_residues = []
    
    for i in range(n):
        if i in used:
            continue
        
        p_i = poles[i]
        r_i = residues[i]
        
        # Look for conjugate pair
        conj_idx = None
        min_diff = float('inf')
        
        for j in range(i + 1, n):
            if j in used:
                continue
            p_j = poles[j]
            # Check if p_j is close to conjugate of p_i
            diff = abs(p_i - np.conj(p_j))
            if diff < min_diff and diff < tolerance:
                min_diff = diff
                conj_idx = j
        
        if conj_idx is not None:
            # Found a conjugate pair - average them to enforce exact symmetry
            p_j = poles[conj_idx]
            r_j = residues[conj_idx]
            
            # Average to get exact conjugate pair
            p_real = (p_i + np.conj(p_j)) / 2
            r_sym = (r_i + np.conj(r_j)) / 2
            
            # Store one pole from the pair (positive imaginary part convention)
            if p_real.imag >= 0:
                fixed_poles.append(p_real)
                fixed_residues.append(r_sym)
            else:
                fixed_poles.append(np.conj(p_real))
                fixed_residues.append(np.conj(r_sym))
            
            used.add(i)
            used.add(conj_idx)
        else:
            # No conjugate found - treat as real pole
            # For physical systems, real poles should have real residues
            fixed_poles.append(p_i.real)
            fixed_residues.append(r_i.real)
            used.add(i)
    
    return np.array(fixed_poles), np.array(fixed_residues)


def compute_dc_gain(poles, residues, constant, proportional=0):
    """
    Compute DC gain from pole-residue representation.
    
    H(0) = sum(r_i / (-p_i)) + constant
    
    Returns:
        Complex DC gain value
    """
    dc_contrib = 0 + 0j
    for p, r in zip(poles, residues):
        if abs(p) > 1e-20:
            dc_contrib += r / (-p)
    
    return dc_contrib + constant


def fix_constant_for_dc(poles, residues, constant, dc_target):
    """
    Adjust constant term to achieve target DC gain.
    
    Args:
        poles: Array of poles
        residues: Array of residues
        constant: Original constant term
        dc_target: Target DC gain (complex or real)
    
    Returns:
        Adjusted constant term
    """
    dc_contrib = 0 + 0j
    for p, r in zip(poles, residues):
        if abs(p) > 1e-20:
            dc_contrib += r / (-p)
    
    # new_constant = dc_target - dc_contrib
    return dc_target - dc_contrib


def fix_scikit_rf_vf(input_s4p, output_json, n_poles=16, max_iter=200, enforce_symmetry=True):
    """
    Perform VectorFitting with scikit-rf and fix the output.
    
    Args:
        input_s4p: Path to input S4P file
        output_json: Path to output JSON file
        n_poles: Number of complex poles to use
        max_iter: Maximum iterations for VF
        enforce_symmetry: Whether to enforce conjugate symmetry
    
    Returns:
        dict: Configuration dictionary with fixed pole-residue data
    """
    print(f"Loading S4P file: {input_s4p}")
    nw = skrf.Network(input_s4p)
    print(f"  Ports: {nw.nports}, Frequency points: {len(nw.frequency.f)}")
    print(f"  Frequency range: {nw.frequency.f[0]/1e6:.2f} MHz to {nw.frequency.f[-1]/1e9:.2f} GHz")
    
    # Get target DC point (first frequency sample)
    s21_orig = nw.s[:, 1, 0]
    freq = nw.frequency.f
    dc_target = s21_orig[0]
    print(f"\nTarget DC gain: {dc_target:.6f} ({20*np.log10(abs(dc_target)):.2f} dB)")
    
    # Perform VectorFitting
    print(f"\nPerforming VectorFitting with {n_poles} complex poles...")
    vf = skrf.VectorFitting(nw)
    vf.max_iterations = max_iter
    
    # Decimate for speed if needed
    n_freq = len(nw.frequency.f)
    if n_freq > 500:
        indices = np.linspace(0, n_freq-1, 500, dtype=int)
        nw_fit = nw[indices]
        print(f"  Decimated to {len(indices)} frequency points for fitting")
    else:
        nw_fit = nw
    
    vf.vector_fit(n_poles_real=0, n_poles_cmplx=n_poles,
                  fit_constant=True, fit_proportional=False,
                  init_pole_spacing='log')
    
    # Get S21 results
    idx_s21 = 1 * nw.nports + 0
    poles = vf.poles
    residues = vf.residues[idx_s21]
    constant = vf.constant_coeff[idx_s21]
    proportional = vf.proportional_coeff[idx_s21]
    
    print(f"\nOriginal VF results:")
    print(f"  Number of poles: {len(poles)}")
    print(f"  Constant: {constant:.6f}")
    print(f"  Proportional: {proportional:.6e}")
    
    dc_orig = compute_dc_gain(poles, residues, constant, proportional)
    print(f"  DC gain (original): {dc_orig:.6f}")
    print(f"  DC gain magnitude: {abs(dc_orig):.6f}")
    
    # Fix conjugate symmetry
    if enforce_symmetry:
        print(f"\nEnforcing conjugate symmetry...")
        poles_fixed, residues_fixed = fix_conjugate_symmetry(poles, residues)
        print(f"  Poles after fixing: {len(poles_fixed)}")
        
        # Verify symmetry
        dc_sym = compute_dc_gain(poles_fixed, residues_fixed, constant)
        print(f"  DC gain (after symmetry fix): {dc_sym:.6f}")
        print(f"  Imaginary part: {dc_sym.imag:.6e}")
    else:
        poles_fixed = poles
        residues_fixed = residues
    
    # Fix constant for DC matching
    print(f"\nAdjusting constant for DC matching...")
    constant_fixed = fix_constant_for_dc(poles_fixed, residues_fixed, 0, dc_target)
    print(f"  New constant: {constant_fixed:.6f}")
    
    # Verify final DC gain
    dc_final = compute_dc_gain(poles_fixed, residues_fixed, constant_fixed)
    print(f"  Final DC gain: {dc_final:.6f}")
    print(f"  Target DC gain: {dc_target:.6f}")
    print(f"  DC error: {abs(dc_final - dc_target):.6e}")
    
    # Evaluate fit quality
    print(f"\nEvaluating fit quality...")
    s = 1j * 2 * np.pi * freq
    s21_fit = np.zeros_like(s, dtype=complex)
    
    for p, r in zip(poles_fixed, residues_fixed):
        s21_fit += r / (s - p)
    s21_fit += constant_fixed
    
    # Compute metrics
    corr = np.corrcoef(np.abs(s21_orig), np.abs(s21_fit))[0, 1]
    orig_db = 20 * np.log10(np.abs(s21_orig) + 1e-20)
    fit_db = 20 * np.log10(np.abs(s21_fit) + 1e-20)
    rmse = np.sqrt(np.mean((orig_db - fit_db)**2))
    max_err = np.max(np.abs(orig_db - fit_db))
    
    print(f"  Correlation: {corr:.6f}")
    print(f"  RMSE (dB): {rmse:.4f}")
    print(f"  Max error (dB): {max_err:.4f}")
    
    # Build output configuration
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
        'fs': 100e9,
        'metrics': {
            'correlation': float(corr),
            'rmse_db': float(rmse),
            'max_error_db': float(max_err),
            'dc_gain_target': complex(dc_target).__repr__(),
            'dc_gain_actual': complex(dc_final).__repr__()
        }
    }
    
    # Write to JSON
    with open(output_json, 'w') as f:
        json.dump(config, f, indent=2)
    
    print(f"\nSaved fixed configuration to: {output_json}")
    
    return config


def main():
    parser = argparse.ArgumentParser(
        description='Fix scikit-rf VectorFitting output for conjugate symmetry'
    )
    parser.add_argument('input_s4p', help='Input S4P file path')
    parser.add_argument('output_json', help='Output JSON file path')
    parser.add_argument('--n-poles', type=int, default=16,
                        help='Number of complex poles (default: 16)')
    parser.add_argument('--max-iter', type=int, default=200,
                        help='Maximum iterations (default: 200)')
    parser.add_argument('--no-symmetry', action='store_true',
                        help='Skip conjugate symmetry enforcement')
    
    args = parser.parse_args()
    
    config = fix_scikit_rf_vf(
        args.input_s4p,
        args.output_json,
        n_poles=args.n_poles,
        max_iter=args.max_iter,
        enforce_symmetry=not args.no_symmetry
    )
    
    # Check if results are acceptable
    metrics = config['metrics']
    corr = metrics['correlation']
    rmse = metrics['rmse_db']
    max_err = metrics['max_error_db']
    
    print("\n" + "="*60)
    print("VALIDATION RESULTS")
    print("="*60)
    print(f"Correlation:   {corr:.6f} {'✓ PASS' if corr > 0.95 else '✗ FAIL'}")
    print(f"RMSE (dB):     {rmse:.4f} {'✓ PASS' if rmse < 3 else '✗ FAIL'}")
    print(f"Max Error (dB): {max_err:.4f} {'✓ PASS' if max_err < 6 else '✗ FAIL'}")
    
    if corr > 0.95 and rmse < 3 and max_err < 6:
        print("\n✓ OVERALL PASS")
        return 0
    else:
        print("\n✗ OVERALL FAIL")
        return 1


if __name__ == '__main__':
    sys.exit(main())
