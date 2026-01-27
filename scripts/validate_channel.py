#!/usr/bin/env python3
"""
Validation script for S-parameter channel processing pipeline.
This script validates the end-to-end flow from S-parameter files to
SystemC-AMS simulation configuration.
"""

import argparse
import json
import logging
import os
import sys
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend
import matplotlib.pyplot as plt

# Add scripts directory to path
sys.path.insert(0, str(Path(__file__).parent))

from vector_fitting import VectorFitting, check_passivity
from process_sparam import (TouchstoneReader, DCCompletion, 
                            ImpulseResponseGenerator, ConfigGenerator)

logger = logging.getLogger(__name__)


def generate_synthetic_sparam(n_freq=1001, f_max=50e9, 
                               loss_db_at_nyquist=20.0,
                               delay_ps=50.0):
    """
    Generate synthetic S-parameter data for testing.
    
    Creates a realistic channel model with:
    - Frequency-dependent loss (skin effect approximation)
    - Propagation delay
    - Return loss
    
    Args:
        n_freq: Number of frequency points
        f_max: Maximum frequency (Hz)
        loss_db_at_nyquist: Insertion loss at f_max (dB)
        delay_ps: Propagation delay (picoseconds)
        
    Returns:
        freq: Frequency array (Hz)
        S21: Forward transmission S-parameter
        S11: Return loss S-parameter
    """
    freq = np.linspace(1e6, f_max, n_freq)  # Start from 1 MHz (not DC)
    
    # Insertion loss model: sqrt(f) dependence (skin effect)
    loss_db = loss_db_at_nyquist * np.sqrt(freq / f_max)
    loss_linear = 10 ** (-loss_db / 20)
    
    # Propagation delay
    delay_s = delay_ps * 1e-12
    phase = -2 * np.pi * freq * delay_s
    
    # S21: forward transmission
    S21 = loss_linear * np.exp(1j * phase)
    
    # S11: return loss (simplified model)
    # Higher return loss at high frequencies
    rl_db = 20 - 10 * (freq / f_max)  # 20 dB at DC, 10 dB at f_max
    rl_linear = 10 ** (-rl_db / 20)
    S11 = rl_linear * np.exp(1j * (phase + np.pi/4))
    
    return freq, S21, S11


def validate_vector_fitting(freq, S_data, orders=[6, 8, 12, 16]):
    """
    Validate vector fitting with different orders.
    
    Args:
        freq: Frequency array
        S_data: S-parameter data
        orders: List of fitting orders to test
        
    Returns:
        results: Dictionary of fitting results for each order
    """
    results = {}
    
    for order in orders:
        logger.info(f"Testing VF with order={order}")
        
        vf = VectorFitting(order=order, max_iterations=15)
        result = vf.fit(freq, S_data, enforce_stability=True)
        
        # Evaluate fit
        H_fit = vf.evaluate(freq)
        
        # Compute metrics
        error = np.abs(S_data - H_fit)
        mse = np.mean(error ** 2)
        max_error = np.max(error)
        
        # Verify stability (all poles in LHP)
        stable = np.all(np.real(result['poles']) < 0)
        
        results[order] = {
            'mse': mse,
            'max_error': max_error,
            'dc_gain': vf.get_dc_gain(),
            'stable': stable,
            'poles': result['poles'],
            'H_fit': H_fit
        }
        
        logger.info(f"  MSE: {mse:.2e}, max_error: {max_error:.2e}, stable: {stable}")
    
    return results


def validate_impulse_response(freq, S_data, fs=100e9, n_samples_list=[1024, 2048, 4096]):
    """
    Validate impulse response generation with different sample counts.
    
    Args:
        freq: Frequency array
        S_data: S-parameter data
        fs: Sampling frequency
        n_samples_list: List of sample counts to test
        
    Returns:
        results: Dictionary of IR results for each sample count
    """
    results = {}
    
    for n_samples in n_samples_list:
        logger.info(f"Testing IR with n_samples={n_samples}")
        
        ir_gen = ImpulseResponseGenerator(fs, n_samples)
        ir_result = ir_gen.generate(freq, S_data)
        
        # Compute metrics
        energy = ir_result['energy']
        length = ir_result['length']
        peak_time = ir_result['peak_time']
        
        results[n_samples] = {
            'energy': energy,
            'length': length,
            'peak_time': peak_time,
            'impulse': np.array(ir_result['impulse']),
            'time': np.array(ir_result['time'])
        }
        
        logger.info(f"  Length: {length}, Energy: {energy:.4f}, Peak: {peak_time*1e12:.2f} ps")
    
    return results


def validate_consistency(freq, S_data, fs=100e9, vf_order=8, ir_samples=4096):
    """
    Validate consistency between rational fitting and impulse response methods.
    
    Args:
        freq: Frequency array
        S_data: S-parameter data
        fs: Sampling frequency
        vf_order: Vector fitting order
        ir_samples: Impulse response samples
        
    Returns:
        consistency_metrics: Dictionary of consistency metrics
    """
    logger.info("Validating consistency between methods")
    
    # Vector fitting
    vf = VectorFitting(order=vf_order)
    vf.fit(freq, S_data, enforce_stability=True)
    H_vf = vf.evaluate(freq)
    
    # Impulse response
    ir_gen = ImpulseResponseGenerator(fs, ir_samples)
    ir_result = ir_gen.generate(freq, S_data)
    
    # Convert IR back to frequency domain for comparison
    impulse = np.array(ir_result['impulse'])
    dt = ir_result['dt']
    
    # Zero-pad and FFT
    N_fft = ir_samples
    h_padded = np.zeros(N_fft)
    h_padded[:len(impulse)] = impulse
    H_ir_fft = np.fft.fft(h_padded)
    freq_ir = np.fft.fftfreq(N_fft, dt)
    
    # Interpolate to original frequency points
    freq_pos = freq_ir[:N_fft//2]
    H_ir_pos = H_ir_fft[:N_fft//2]
    
    # Compare on common frequency range
    f_max = min(freq.max(), freq_pos.max())
    freq_common = freq[freq <= f_max]
    
    # Interpolate both to common frequencies
    from scipy.interpolate import interp1d
    
    H_vf_common = vf.evaluate(freq_common)
    
    interp_real = interp1d(freq_pos, H_ir_pos.real, bounds_error=False, fill_value=0)
    interp_imag = interp1d(freq_pos, H_ir_pos.imag, bounds_error=False, fill_value=0)
    H_ir_common = interp_real(freq_common) + 1j * interp_imag(freq_common)
    
    # Compute consistency metrics
    error_vf_ir = np.abs(H_vf_common - H_ir_common)
    error_vf_orig = np.abs(H_vf_common - S_data[:len(freq_common)])
    error_ir_orig = np.abs(H_ir_common - S_data[:len(freq_common)])
    
    metrics = {
        'vf_ir_mse': np.mean(error_vf_ir ** 2),
        'vf_ir_max': np.max(error_vf_ir),
        'vf_orig_mse': np.mean(error_vf_orig ** 2),
        'ir_orig_mse': np.mean(error_ir_orig ** 2),
        'dc_gain_vf': vf.get_dc_gain(),
        'dc_gain_ir': np.sum(impulse) * dt
    }
    
    logger.info(f"VF-IR MSE: {metrics['vf_ir_mse']:.2e}")
    logger.info(f"VF-Orig MSE: {metrics['vf_orig_mse']:.2e}")
    logger.info(f"IR-Orig MSE: {metrics['ir_orig_mse']:.2e}")
    logger.info(f"DC gains: VF={metrics['dc_gain_vf']:.4f}, IR={metrics['dc_gain_ir']:.4f}")
    
    return metrics


def generate_validation_plots(freq, S_data, vf_results, ir_results, output_dir):
    """
    Generate validation plots.
    
    Args:
        freq: Frequency array
        S_data: Original S-parameter data
        vf_results: Vector fitting results
        ir_results: Impulse response results
        output_dir: Output directory for plots
    """
    os.makedirs(output_dir, exist_ok=True)
    
    # Plot 1: Frequency response comparison
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    
    # Magnitude
    ax = axes[0, 0]
    ax.semilogx(freq / 1e9, 20 * np.log10(np.abs(S_data)), 'b-', label='Original', linewidth=2)
    for order, result in vf_results.items():
        ax.semilogx(freq / 1e9, 20 * np.log10(np.abs(result['H_fit'])), 
                   '--', label=f'VF order={order}', alpha=0.7)
    ax.set_xlabel('Frequency (GHz)')
    ax.set_ylabel('Magnitude (dB)')
    ax.set_title('Frequency Response - Magnitude')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Phase
    ax = axes[0, 1]
    ax.semilogx(freq / 1e9, np.unwrap(np.angle(S_data)) * 180 / np.pi, 
               'b-', label='Original', linewidth=2)
    for order, result in vf_results.items():
        ax.semilogx(freq / 1e9, np.unwrap(np.angle(result['H_fit'])) * 180 / np.pi,
                   '--', label=f'VF order={order}', alpha=0.7)
    ax.set_xlabel('Frequency (GHz)')
    ax.set_ylabel('Phase (degrees)')
    ax.set_title('Frequency Response - Phase')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Impulse response
    ax = axes[1, 0]
    for n_samples, result in ir_results.items():
        time_ps = result['time'] * 1e12
        ax.plot(time_ps, result['impulse'], label=f'N={n_samples}')
    ax.set_xlabel('Time (ps)')
    ax.set_ylabel('Amplitude')
    ax.set_title('Impulse Response')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Fitting error
    ax = axes[1, 1]
    for order, result in vf_results.items():
        error = np.abs(S_data - result['H_fit'])
        ax.semilogy(freq / 1e9, error, label=f'VF order={order}')
    ax.set_xlabel('Frequency (GHz)')
    ax.set_ylabel('Absolute Error')
    ax.set_title('Fitting Error')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'validation_plots.png'), dpi=150)
    plt.close()
    
    logger.info(f"Plots saved to {output_dir}/validation_plots.png")


def run_full_validation(output_dir='validation_output'):
    """
    Run complete validation pipeline.
    
    Args:
        output_dir: Output directory for results
    """
    logger.info("=" * 60)
    logger.info("Starting Full Validation Pipeline")
    logger.info("=" * 60)
    
    # Generate synthetic S-parameters
    logger.info("\n1. Generating synthetic S-parameter data")
    freq, S21, S11 = generate_synthetic_sparam(
        n_freq=1001, f_max=50e9, loss_db_at_nyquist=15.0, delay_ps=100.0
    )
    
    # Validate vector fitting
    logger.info("\n2. Validating Vector Fitting")
    vf_results = validate_vector_fitting(freq, S21, orders=[6, 8, 12, 16])
    
    # Validate impulse response
    logger.info("\n3. Validating Impulse Response Generation")
    ir_results = validate_impulse_response(freq, S21, fs=100e9, 
                                           n_samples_list=[1024, 2048, 4096])
    
    # Validate consistency
    logger.info("\n4. Validating Method Consistency")
    consistency = validate_consistency(freq, S21, fs=100e9, vf_order=12, ir_samples=4096)
    
    # Generate plots
    logger.info("\n5. Generating Validation Plots")
    generate_validation_plots(freq, S21, vf_results, ir_results, output_dir)
    
    # Generate configuration file
    logger.info("\n6. Generating Configuration File")
    config_gen = ConfigGenerator(100e9)
    
    # Add best VF result
    vf_best = VectorFitting(order=12)
    vf_result = vf_best.fit(freq, S21, enforce_stability=True)
    config_gen.add_rational_filter('S21', vf_result)
    
    # Add IR result
    ir_gen = ImpulseResponseGenerator(100e9, 4096)
    ir_result = ir_gen.generate(freq, S21)
    config_gen.add_impulse_response('S21', ir_result)
    
    config_gen.set_metadata('synthetic', 2, (freq[0], freq[-1]))
    config_gen.save(os.path.join(output_dir, 'channel_config.json'))
    
    # Summary
    logger.info("\n" + "=" * 60)
    logger.info("Validation Summary")
    logger.info("=" * 60)
    logger.info(f"Best VF order: 12 (MSE={vf_results[12]['mse']:.2e})")
    logger.info(f"IR length: {ir_results[4096]['length']} samples")
    logger.info(f"Method consistency MSE: {consistency['vf_ir_mse']:.2e}")
    logger.info(f"All outputs saved to: {output_dir}/")
    
    return {
        'vf_results': vf_results,
        'ir_results': ir_results,
        'consistency': consistency
    }


def main():
    """Command-line interface."""
    parser = argparse.ArgumentParser(
        description='Validate S-parameter channel processing pipeline'
    )
    parser.add_argument('-o', '--output', default='validation_output',
                       help='Output directory (default: validation_output)')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Enable verbose output')
    parser.add_argument('--input', help='Input Touchstone file (optional)')
    
    args = parser.parse_args()
    
    # Configure logging
    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=level, format='%(levelname)s: %(message)s')
    
    try:
        results = run_full_validation(args.output)
        print("\nValidation completed successfully!")
        return 0
    except Exception as e:
        logger.error(f"Validation failed: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
