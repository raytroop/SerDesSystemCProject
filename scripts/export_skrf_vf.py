#!/usr/bin/env python3
"""
Export S-parameter S21 to pole-residue format using scikit-rf VectorFitting.

This script loads an S4P file, performs vector fitting on the S21 parameter
(port 2, port 1), and exports the pole-residue representation to JSON.
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import skrf
from skrf.vectorFitting import VectorFitting


def export_sparameter_to_pole_residue(input_s4p, output_json, n_poles=24, n_real=0, max_freq_points=400):
    """
    Export S21 from S4P file to pole-residue format using scikit-rf VectorFitting.
    
    Args:
        input_s4p: Path to input S4P file
        output_json: Path to output JSON file
        n_poles: Number of complex poles (default 24)
        n_real: Number of real poles (default 0)
        max_freq_points: Maximum frequency points to use for fitting (default 500)
        
    Returns:
        dict: Configuration dictionary with pole-residue data
    """
    # Load S-parameter network
    nw_full = skrf.Network(input_s4p)
    
    # Decimate frequency points if needed for speed
    n_freq = len(nw_full.frequency.f)
    if n_freq > max_freq_points:
        step = n_freq // max_freq_points
        indices = np.arange(0, n_freq, step)[:max_freq_points]
        nw = nw_full[indices]
        print(f"  Decimated from {n_freq} to {len(indices)} frequency points")
    else:
        nw = nw_full
    
    # Extract S21 (port 2, port 1) - row 1, col 0 in 0-indexed
    s21 = nw.s[:, 1, 0]
    freq = nw.frequency.f
    
    # Perform vector fitting with optimized settings
    vf = VectorFitting(nw)
    vf.max_iterations = 100  # Full convergence for good correlation
    vf.vector_fit(
        n_poles_real=n_real,
        n_poles_cmplx=n_poles,
        fit_constant=True,
        fit_proportional=True
    )
    
    # Get S21 index in flattened response
    n_ports = nw.nports
    idx_s21 = 1 * n_ports + 0  # row 1, col 0
    
    # Build output configuration
    config = {
        'version': '2.1-scikit-rf',
        'method': 'POLE_RESIDUE',
        'pole_residue': {
            'poles_real': vf.poles.real.tolist(),
            'poles_imag': vf.poles.imag.tolist(),
            'residues_real': vf.residues[idx_s21].real.tolist(),
            'residues_imag': vf.residues[idx_s21].imag.tolist(),
            'constant': float(vf.constant_coeff[idx_s21]),
            'proportional': float(vf.proportional_coeff[idx_s21]),
            'order': len(vf.poles)
        },
        'fs': 100e9
    }
    
    # Write to JSON file
    with open(output_json, 'w') as f:
        json.dump(config, f, indent=2)
    
    return config


def verify_fitting(input_s4p, output_json):
    """
    Verify fitting accuracy by comparing original and fitted S21 magnitude.
    
    Args:
        input_s4p: Path to input S4P file
        output_json: Path to output JSON file with fitted parameters
        
    Returns:
        float: Correlation coefficient between original and fitted magnitude
    """
    # Load original S21 (use full frequency range for verification)
    nw = skrf.Network(input_s4p)
    s21_orig = nw.s[:, 1, 0]
    s21_mag_orig = np.abs(s21_orig)
    freq = nw.frequency.f
    
    # Load fitted parameters
    with open(output_json) as f:
        config = json.load(f)
    
    pr = config['pole_residue']
    poles = np.array(pr['poles_real']) + 1j * np.array(pr['poles_imag'])
    residues = np.array(pr['residues_real']) + 1j * np.array(pr['residues_imag'])
    constant = pr['constant']
    proportional = pr['proportional']
    
    # Reconstruct fitted response
    s = 1j * 2 * np.pi * freq
    s21_fit = np.zeros_like(s, dtype=complex)
    
    for r, p in zip(residues, poles):
        s21_fit += r / (s - p)
    
    s21_fit += constant + proportional * s
    s21_mag_fit = np.abs(s21_fit)
    
    # Calculate correlation
    correlation = np.corrcoef(s21_mag_orig, s21_mag_fit)[0, 1]
    
    return correlation


def main():
    parser = argparse.ArgumentParser(
        description='Export S-parameter S21 to pole-residue format using scikit-rf VectorFitting'
    )
    parser.add_argument('input_s4p', help='Input S4P file path')
    parser.add_argument('output_json', help='Output JSON file path')
    parser.add_argument('--n-poles', type=int, default=24,
                        help='Number of complex poles (default: 24)')
    parser.add_argument('--n-real', type=int, default=0,
                        help='Number of real poles (default: 0)')
    parser.add_argument('--verify', action='store_true',
                        help='Verify fitting accuracy and print correlation')
    
    args = parser.parse_args()
    
    # Validate input file
    input_path = Path(args.input_s4p)
    if not input_path.exists():
        print(f"Error: Input file not found: {args.input_s4p}", file=sys.stderr)
        sys.exit(1)
    
    # Perform export
    print(f"Loading S4P file: {args.input_s4p}")
    nw = skrf.Network(args.input_s4p)
    print(f"  Ports: {nw.nports}, Frequency points: {len(nw.frequency.f)}")
    
    print(f"\nPerforming VectorFitting with {args.n_poles} complex poles...")
    config = export_sparameter_to_pole_residue(
        args.input_s4p,
        args.output_json,
        n_poles=args.n_poles,
        n_real=args.n_real,
        max_freq_points=400
    )
    
    print(f"Exported to: {args.output_json}")
    print(f"  Version: {config['version']}")
    print(f"  Method: {config['method']}")
    print(f"  Order: {config['pole_residue']['order']}")
    
    # Verify if requested
    if args.verify:
        print("\nVerifying fitting accuracy...")
        correlation = verify_fitting(args.input_s4p, args.output_json)
        print(f"  Correlation: {correlation:.6f}")
        
        if correlation > 0.95:
            print(f"  ✓ Correlation > 0.95 (excellent fit)")
        elif correlation > 0.90:
            print(f"  ✓ Correlation > 0.90 (good fit)")
        else:
            print(f"  ⚠ Correlation < 0.90 (poor fit)")
        
        # Assert for automated testing (0.93 is achievable with 100 iterations)
        assert correlation > 0.93, f"Correlation too low: {correlation}"
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
