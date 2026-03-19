#!/usr/bin/env python3
"""
Channel Model Validation Script

Compares Vector Fitting model response against original S-parameter data.
This script validates the accuracy of pole-residue channel modeling.

Usage:
    python validate_channel_model.py <s4p_file> [options]
    
Example:
    python validate_channel_model.py ../peters_01_0605_B12_thru.s4p --order 16 --output validation.png
"""

import argparse
import json
import logging
import os
import sys
from pathlib import Path
from typing import Dict, Tuple, Optional

import numpy as np

# Optional imports with fallback
try:
    import skrf as rf
    HAS_SKRF = True
except ImportError:
    HAS_SKRF = False
    print("Warning: scikit-rf not installed. Install with: pip install scikit-rf")

try:
    import matplotlib.pyplot as plt
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not installed. Plotting disabled.")

# Import local vector fitting module
from vector_fitting import VectorFitting

logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
logger = logging.getLogger(__name__)


def load_sparam(s4p_path: str, port_pair: Tuple[int, int] = (1, 0)) -> Tuple[np.ndarray, np.ndarray]:
    """
    Load S-parameter file and extract specified port pair.
    
    Args:
        s4p_path: Path to .s4p or .s2p file
        port_pair: Tuple of (output_port, input_port), 0-indexed. 
                   Default (1, 0) = S21 for differential pair
    
    Returns:
        frequencies: Frequency array (Hz)
        s_data: Complex S-parameter array
    """
    if not HAS_SKRF:
        raise ImportError("scikit-rf is required. Install with: pip install scikit-rf")
    
    logger.info(f"Loading S-parameter file: {s4p_path}")
    network = rf.Network(s4p_path)
    
    frequencies = network.f  # Hz
    s_matrix = network.s     # Shape: (n_freq, n_ports, n_ports)
    
    out_port, in_port = port_pair
    s_data = s_matrix[:, out_port, in_port]
    
    logger.info(f"  Frequency range: {frequencies[0]/1e9:.2f} - {frequencies[-1]/1e9:.2f} GHz")
    logger.info(f"  Number of points: {len(frequencies)}")
    logger.info(f"  Extracted S{out_port+1}{in_port+1}")
    
    return frequencies, s_data


def compute_model_response(poles: np.ndarray, residues: np.ndarray, 
                           d: float, e: float, 
                           frequencies: np.ndarray) -> np.ndarray:
    """
    Compute pole-residue model response at given frequencies.
    
    H(s) = sum(r_i / (s - p_i)) + d + e*s
    
    Args:
        poles: Complex pole array (rad/s)
        residues: Complex residue array
        d: Constant term
        e: Proportional term (s coefficient)
        frequencies: Frequency array (Hz)
    
    Returns:
        Complex frequency response
    """
    s = 1j * 2 * np.pi * frequencies
    H = np.full_like(s, d, dtype=complex)
    
    # Add proportional term
    if np.abs(e) > 1e-20:
        H += e * s
    
    # Add pole-residue terms
    for p, r in zip(poles, residues):
        H += r / (s - p)
    
    return H


def compare_responses(freq: np.ndarray, s_original: np.ndarray, 
                      s_model: np.ndarray) -> Dict[str, float]:
    """
    Compare original and modeled S-parameter responses.
    
    Args:
        freq: Frequency array (Hz)
        s_original: Original S-parameter (complex)
        s_model: Modeled S-parameter (complex)
    
    Returns:
        Dictionary with error metrics
    """
    # Magnitude comparison (dB)
    mag_orig_db = 20 * np.log10(np.maximum(np.abs(s_original), 1e-15))
    mag_model_db = 20 * np.log10(np.maximum(np.abs(s_model), 1e-15))
    mag_error_db = mag_model_db - mag_orig_db
    
    # Phase comparison (degrees)
    phase_orig = np.angle(s_original, deg=True)
    phase_model = np.angle(s_model, deg=True)
    # Handle phase wrapping
    phase_error = phase_model - phase_orig
    phase_error = np.mod(phase_error + 180, 360) - 180
    
    # Complex error (relative)
    complex_error = np.abs(s_model - s_original) / (np.abs(s_original) + 1e-15)
    
    metrics = {
        'max_mag_error_db': float(np.max(np.abs(mag_error_db))),
        'rms_mag_error_db': float(np.sqrt(np.mean(mag_error_db**2))),
        'mean_mag_error_db': float(np.mean(mag_error_db)),
        'max_phase_error_deg': float(np.max(np.abs(phase_error))),
        'rms_phase_error_deg': float(np.sqrt(np.mean(phase_error**2))),
        'max_relative_error': float(np.max(complex_error)),
        'rms_relative_error': float(np.sqrt(np.mean(complex_error**2))),
    }
    
    return metrics


def plot_comparison(freq: np.ndarray, s_original: np.ndarray, 
                    s_model: np.ndarray, output_path: str,
                    title: str = "Channel Model Validation"):
    """
    Plot comparison between original and modeled S-parameter.
    
    Args:
        freq: Frequency array (Hz)
        s_original: Original S-parameter (complex)
        s_model: Modeled S-parameter (complex)
        output_path: Output image file path
        title: Plot title
    """
    if not HAS_MATPLOTLIB:
        logger.warning("matplotlib not available, skipping plot")
        return
    
    freq_ghz = freq / 1e9
    
    mag_orig_db = 20 * np.log10(np.maximum(np.abs(s_original), 1e-15))
    mag_model_db = 20 * np.log10(np.maximum(np.abs(s_model), 1e-15))
    
    phase_orig = np.unwrap(np.angle(s_original)) * 180 / np.pi
    phase_model = np.unwrap(np.angle(s_model)) * 180 / np.pi
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    # Magnitude comparison
    ax1 = axes[0, 0]
    ax1.plot(freq_ghz, mag_orig_db, 'b-', label='Original S-param', linewidth=1.5)
    ax1.plot(freq_ghz, mag_model_db, 'r--', label='VF Model', linewidth=1.5)
    ax1.set_xlabel('Frequency (GHz)')
    ax1.set_ylabel('Magnitude (dB)')
    ax1.set_title('Magnitude Response')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # Magnitude error
    ax2 = axes[0, 1]
    mag_error = mag_model_db - mag_orig_db
    ax2.plot(freq_ghz, mag_error, 'g-', linewidth=1)
    ax2.axhline(y=0, color='k', linestyle='--', linewidth=0.5)
    ax2.set_xlabel('Frequency (GHz)')
    ax2.set_ylabel('Error (dB)')
    ax2.set_title(f'Magnitude Error (RMS: {np.sqrt(np.mean(mag_error**2)):.3f} dB)')
    ax2.grid(True, alpha=0.3)
    
    # Phase comparison
    ax3 = axes[1, 0]
    ax3.plot(freq_ghz, phase_orig, 'b-', label='Original S-param', linewidth=1.5)
    ax3.plot(freq_ghz, phase_model, 'r--', label='VF Model', linewidth=1.5)
    ax3.set_xlabel('Frequency (GHz)')
    ax3.set_ylabel('Phase (degrees)')
    ax3.set_title('Phase Response (unwrapped)')
    ax3.legend()
    ax3.grid(True, alpha=0.3)
    
    # Phase error
    ax4 = axes[1, 1]
    phase_error = phase_model - phase_orig
    ax4.plot(freq_ghz, phase_error, 'm-', linewidth=1)
    ax4.axhline(y=0, color='k', linestyle='--', linewidth=0.5)
    ax4.set_xlabel('Frequency (GHz)')
    ax4.set_ylabel('Error (degrees)')
    ax4.set_title(f'Phase Error (RMS: {np.sqrt(np.mean(phase_error**2)):.2f} deg)')
    ax4.grid(True, alpha=0.3)
    
    plt.suptitle(title, fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    logger.info(f"Saved comparison plot to: {output_path}")
    plt.close()


def export_pole_residue_json(poles: np.ndarray, residues: np.ndarray,
                              d: float, e: float, fs: float,
                              output_path: str):
    """
    Export pole-residue data to JSON format for C++ channel model.
    
    Args:
        poles: Complex pole array (rad/s)
        residues: Complex residue array
        d: Constant term
        e: Proportional term
        fs: Sampling frequency (Hz)
        output_path: Output JSON file path
    """
    config = {
        "version": "2.1-vf",
        "method": "pole_residue",
        "fs": float(fs),
        "pole_residue": {
            "poles_real": [float(p.real) for p in poles],
            "poles_imag": [float(p.imag) for p in poles],
            "residues_real": [float(r.real) for r in residues],
            "residues_imag": [float(r.imag) for r in residues],
            "constant": float(d),
            "proportional": float(e),
            "order": len(poles)
        }
    }
    
    with open(output_path, 'w') as f:
        json.dump(config, f, indent=2)
    
    logger.info(f"Exported pole-residue config to: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Validate Vector Fitting channel model against S-parameter data',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s ../peters_01_0605_B12_thru.s4p
  %(prog)s channel.s4p --order 16 --output validation.png
  %(prog)s channel.s4p --port 2 1 --export-json channel_config.json
        '''
    )
    
    parser.add_argument('s4p_file', help='S-parameter file (.s4p, .s2p, etc.)')
    parser.add_argument('--order', type=int, default=16,
                        help='Vector Fitting order (default: 16)')
    parser.add_argument('--port', type=int, nargs=2, default=[2, 1], metavar=('OUT', 'IN'),
                        help='Port pair (1-indexed, default: 2 1 for S21)')
    parser.add_argument('--output', '-o', default='validation_result.png',
                        help='Output plot file (default: validation_result.png)')
    parser.add_argument('--export-json', type=str, default=None,
                        help='Export pole-residue to JSON config file')
    parser.add_argument('--fs', type=float, default=100e9,
                        help='Sampling frequency for JSON export (default: 100e9)')
    parser.add_argument('--max-iter', type=int, default=10,
                        help='Max VF iterations (default: 10)')
    parser.add_argument('--quiet', '-q', action='store_true',
                        help='Suppress detailed output')
    
    args = parser.parse_args()
    
    if args.quiet:
        logging.getLogger().setLevel(logging.WARNING)
    
    # Check file exists
    if not os.path.exists(args.s4p_file):
        logger.error(f"File not found: {args.s4p_file}")
        sys.exit(1)
    
    # Load S-parameter
    port_pair = (args.port[0] - 1, args.port[1] - 1)  # Convert to 0-indexed
    frequencies, s_original = load_sparam(args.s4p_file, port_pair)
    
    # Run Vector Fitting
    logger.info(f"Running Vector Fitting with order={args.order}")
    vf = VectorFitting(order=args.order, max_iterations=args.max_iter)
    
    s = 1j * 2 * np.pi * frequencies  # Convert to rad/s
    result = vf.fit(s, s_original)
    
    poles = result['poles']
    residues = result['residues']
    d = result['d']
    e = result['e']
    
    logger.info(f"  Fitted {len(poles)} poles")
    logger.info(f"  Constant (d): {d:.6e}")
    logger.info(f"  Proportional (e): {e:.6e}")
    
    # Compute model response
    s_model = compute_model_response(poles, residues, d, e, frequencies)
    
    # Compare responses
    metrics = compare_responses(frequencies, s_original, s_model)
    
    print("\n" + "="*60)
    print("Channel Model Validation Report")
    print("="*60)
    print(f"S-parameter file: {args.s4p_file}")
    print(f"Port pair: S{args.port[0]}{args.port[1]}")
    print(f"VF order: {args.order}")
    print("-"*60)
    print("Magnitude Error:")
    print(f"  Max:  {metrics['max_mag_error_db']:+.4f} dB")
    print(f"  RMS:  {metrics['rms_mag_error_db']:.4f} dB")
    print(f"  Mean: {metrics['mean_mag_error_db']:+.4f} dB")
    print("Phase Error:")
    print(f"  Max:  {metrics['max_phase_error_deg']:.2f} deg")
    print(f"  RMS:  {metrics['rms_phase_error_deg']:.2f} deg")
    print("Relative Error:")
    print(f"  Max:  {metrics['max_relative_error']*100:.2f}%")
    print(f"  RMS:  {metrics['rms_relative_error']*100:.2f}%")
    print("="*60)
    
    # Quality assessment
    if metrics['rms_mag_error_db'] < 0.5:
        quality = "EXCELLENT"
    elif metrics['rms_mag_error_db'] < 1.0:
        quality = "GOOD"
    elif metrics['rms_mag_error_db'] < 2.0:
        quality = "ACCEPTABLE"
    else:
        quality = "POOR - consider increasing order"
    print(f"Fit Quality: {quality}")
    print("="*60 + "\n")
    
    # Generate plot
    title = f"Channel Model Validation: S{args.port[0]}{args.port[1]} (Order={args.order})"
    plot_comparison(frequencies, s_original, s_model, args.output, title)
    
    # Export JSON if requested
    if args.export_json:
        export_pole_residue_json(poles, residues, d, e, args.fs, args.export_json)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
