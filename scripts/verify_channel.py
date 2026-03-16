#!/usr/bin/env python3
"""
Channel Verification Script for End-to-End Integration Testing

This script verifies the channel simulation results by:
1. Reading the simulation output waveform (CSV format)
2. Computing FFT to obtain frequency response
3. Comparing with original S-parameters or expected channel response
4. Calculating error metrics

Usage:
    python verify_channel.py [waveform_csv] [metadata_json]
    
Examples:
    python verify_channel.py                    # Use default files
    python verify_channel.py channel_waveform.csv channel_metadata.json
"""

import argparse
import json
import logging
import sys
from pathlib import Path

import numpy as np
from scipy import signal
from scipy.interpolate import interp1d
from scipy.signal.windows import hann as window_hann, hamming as window_hamming, blackman as window_blackman

# Import process_sparam for S-parameter reading
sys.path.insert(0, str(Path(__file__).parent))
from process_sparam import TouchstoneReader

logger = logging.getLogger(__name__)


def load_waveform(csv_file):
    """
    Load waveform data from CSV file.
    
    Args:
        csv_file: Path to CSV file with columns: time_s, input_V, output_V
        
    Returns:
        time: Time array (s)
        input_sig: Input signal (V)
        output_sig: Output signal (V)
        sample_rate: Computed sample rate (Hz)
    """
    logger.info(f"Loading waveform from: {csv_file}")
    
    data = np.loadtxt(csv_file, delimiter=',', skiprows=1)
    
    time = data[:, 0]
    input_sig = data[:, 1]
    output_sig = data[:, 2]
    
    # Compute sample rate from time array
    dt = np.mean(np.diff(time))
    sample_rate = 1.0 / dt
    
    logger.info(f"  Samples: {len(time)}")
    logger.info(f"  Time range: {time[0]*1e9:.3f} - {time[-1]*1e9:.3f} ns")
    logger.info(f"  Sample rate: {sample_rate/1e9:.2f} GHz")
    
    return time, input_sig, output_sig, sample_rate


def load_metadata(json_file):
    """
    Load metadata from JSON file.
    
    Args:
        json_file: Path to metadata JSON file
        
    Returns:
        metadata dictionary
    """
    logger.info(f"Loading metadata from: {json_file}")
    
    with open(json_file, 'r') as f:
        metadata = json.load(f)
    
    return metadata


def compute_frequency_response(time, input_sig, output_sig, sample_rate,
                                window='hann', n_fft=None):
    """
    Compute frequency response from input/output waveforms using FFT.
    
    Uses the spectral division method:
        H(f) = FFT(output) / FFT(input)
    
    Args:
        time: Time array
        input_sig: Input signal
        output_sig: Output signal
        sample_rate: Sample rate (Hz)
        window: Window function name ('hann', 'hamming', 'blackman', or None)
        n_fft: FFT size (None for auto)
        
    Returns:
        freq: Frequency array (Hz)
        H: Complex frequency response
        H_mag: Magnitude response (dB)
        H_phase: Phase response (degrees)
    """
    logger.info("Computing frequency response...")
    
    # Determine FFT size
    if n_fft is None:
        n_fft = len(input_sig)
    
    # Apply window to reduce spectral leakage
    if window == 'hann':
        win = window_hann(len(input_sig))
    elif window == 'hamming':
        win = window_hamming(len(input_sig))
    elif window == 'blackman':
        win = window_blackman(len(input_sig))
    else:
        win = np.ones(len(input_sig))
    
    # Apply window
    input_windowed = input_sig * win
    output_windowed = output_sig * win
    
    # Compute FFT
    fft_input = np.fft.fft(input_windowed, n=n_fft)
    fft_output = np.fft.fft(output_windowed, n=n_fft)
    
    # Compute frequency response (avoid division by zero)
    # Add small epsilon to prevent division by zero
    epsilon = 1e-12 * np.max(np.abs(fft_input))
    H = fft_output / (fft_input + epsilon)
    
    # Get frequency axis (positive frequencies only)
    freq = np.fft.fftfreq(n_fft, 1.0/sample_rate)
    
    # Keep only positive frequencies
    pos_idx = freq >= 0
    freq = freq[pos_idx]
    H = H[pos_idx]
    
    # Compute magnitude and phase
    H_mag = 20.0 * np.log10(np.abs(H) + 1e-12)
    H_phase = np.angle(H, deg=True)
    
    logger.info(f"  Frequency range: {freq[0]/1e9:.3f} - {freq[-1]/1e9:.3f} GHz")
    logger.info(f"  Frequency resolution: {(freq[1]-freq[0])/1e6:.3f} MHz")
    
    return freq, H, H_mag, H_phase


def compute_simple_channel_response(freq, attenuation_db, bandwidth_hz):
    """
    Compute expected frequency response for simple channel model.
    
    The simple model is a first-order low-pass filter:
        H(s) = 10^(-A/20) / (1 + s/wc)
    
    where A is attenuation in dB and wc = 2*pi*bandwidth.
    
    Args:
        freq: Frequency array (Hz)
        attenuation_db: DC attenuation (dB)
        bandwidth_hz: Bandwidth (Hz, -3dB point)
        
    Returns:
        H_mag: Magnitude response (dB)
        H_phase: Phase response (degrees)
    """
    # DC gain (linear)
    dc_gain = 10.0 ** (-attenuation_db / 20.0)
    
    # First-order low-pass filter
    wc = 2.0 * np.pi * bandwidth_hz
    s = 1j * 2.0 * np.pi * freq
    
    H = dc_gain / (1.0 + s / wc)
    
    H_mag = 20.0 * np.log10(np.abs(H))
    H_phase = np.angle(H, deg=True)
    
    return H_mag, H_phase


def compare_frequency_responses(freq_meas, H_meas_mag, H_meas_phase,
                                 freq_ref, H_ref_mag, H_ref_phase,
                                 freq_min=1e6, freq_max=20e9):
    """
    Compare measured and reference frequency responses.
    
    Args:
        freq_meas: Measured frequency array
        H_meas_mag: Measured magnitude (dB)
        H_meas_phase: Measured phase (degrees)
        freq_ref: Reference frequency array
        H_ref_mag: Reference magnitude (dB)
        H_ref_phase: Reference phase (degrees)
        freq_min: Minimum frequency for comparison (Hz)
        freq_max: Maximum frequency for comparison (Hz)
        
    Returns:
        comparison dictionary with error metrics
    """
    logger.info("Comparing frequency responses...")
    
    # Interpolate reference to measured frequency grid
    # (measured usually has finer resolution from FFT)
    interp_mag = interp1d(freq_ref, H_ref_mag, kind='cubic',
                          bounds_error=False, fill_value='extrapolate')
    interp_phase = interp1d(freq_ref, H_ref_phase, kind='cubic',
                            bounds_error=False, fill_value='extrapolate')
    
    H_ref_mag_interp = interp_mag(freq_meas)
    H_ref_phase_interp = interp_phase(freq_meas)
    
    # Select frequency range for comparison
    valid_idx = (freq_meas >= freq_min) & (freq_meas <= freq_max)
    
    freq_comp = freq_meas[valid_idx]
    meas_mag_comp = H_meas_mag[valid_idx]
    ref_mag_comp = H_ref_mag_interp[valid_idx]
    meas_phase_comp = H_meas_phase[valid_idx]
    ref_phase_comp = H_ref_phase_interp[valid_idx]
    
    # Compute magnitude error
    mag_error = meas_mag_comp - ref_mag_comp
    mag_error_mean = np.mean(mag_error)
    mag_error_std = np.std(mag_error)
    mag_error_max = np.max(np.abs(mag_error))
    mag_error_rmse = np.sqrt(np.mean(mag_error ** 2))
    
    # Compute phase error (unwrap first)
    meas_phase_unwrap = np.unwrap(meas_phase_comp * np.pi / 180) * 180 / np.pi
    ref_phase_unwrap = np.unwrap(ref_phase_comp * np.pi / 180) * 180 / np.pi
    phase_error = meas_phase_unwrap - ref_phase_unwrap
    phase_error_mean = np.mean(phase_error)
    phase_error_std = np.std(phase_error)
    phase_error_max = np.max(np.abs(phase_error))
    
    # Compute correlation coefficient
    corr_mag = np.corrcoef(meas_mag_comp, ref_mag_comp)[0, 1]
    
    results = {
        'frequency_range_hz': (float(freq_comp[0]), float(freq_comp[-1])),
        'magnitude_error_db': {
            'mean': float(mag_error_mean),
            'std': float(mag_error_std),
            'max': float(mag_error_max),
            'rmse': float(mag_error_rmse)
        },
        'phase_error_deg': {
            'mean': float(phase_error_mean),
            'std': float(phase_error_std),
            'max': float(phase_error_max)
        },
        'correlation': float(corr_mag),
        'num_points': int(len(freq_comp))
    }
    
    return results


def verify_simple_channel(waveform_file, metadata, output_dir):
    """
    Verify simple channel model results.
    
    Args:
        waveform_file: Path to waveform CSV
        metadata: Metadata dictionary
        output_dir: Output directory for results
        
    Returns:
        verification results dictionary
    """
    logger.info("=" * 60)
    logger.info("Verifying Simple Channel Model")
    logger.info("=" * 60)
    
    # Load waveform
    time, input_sig, output_sig, sample_rate = load_waveform(waveform_file)
    
    # Compute frequency response
    freq, H, H_mag, H_phase = compute_frequency_response(
        time, input_sig, output_sig, sample_rate
    )
    
    # Get expected channel parameters
    attenuation_db = metadata.get('channel', {}).get('attenuation_db', 6.0)
    bandwidth_hz = metadata.get('channel', {}).get('bandwidth_hz', 15e9)
    
    logger.info(f"Expected channel parameters:")
    logger.info(f"  Attenuation: {attenuation_db} dB")
    logger.info(f"  Bandwidth: {bandwidth_hz/1e9} GHz")
    
    # Compute expected response
    H_ref_mag, H_ref_phase = compute_simple_channel_response(
        freq, attenuation_db, bandwidth_hz
    )
    
    # Compare responses
    comparison = compare_frequency_responses(
        freq, H_mag, H_phase,
        freq, H_ref_mag, H_ref_phase,
        freq_min=1e6, freq_max=min(bandwidth_hz * 2, sample_rate / 2)
    )
    
    # Determine pass/fail
    pass_criteria = {
        'mag_error_rmse_db': 3.0,      # RMSE < 3 dB
        'mag_error_max_db': 6.0,       # Max error < 6 dB
        'correlation': 0.95            # Correlation > 0.95
    }
    
    passed = True
    if comparison['magnitude_error_db']['rmse'] > pass_criteria['mag_error_rmse_db']:
        logger.warning(f"FAIL: Magnitude RMSE {comparison['magnitude_error_db']['rmse']:.2f} dB > {pass_criteria['mag_error_rmse_db']} dB")
        passed = False
    else:
        logger.info(f"PASS: Magnitude RMSE {comparison['magnitude_error_db']['rmse']:.2f} dB <= {pass_criteria['mag_error_rmse_db']} dB")
    
    if comparison['magnitude_error_db']['max'] > pass_criteria['mag_error_max_db']:
        logger.warning(f"FAIL: Max magnitude error {comparison['magnitude_error_db']['max']:.2f} dB > {pass_criteria['mag_error_max_db']} dB")
        passed = False
    else:
        logger.info(f"PASS: Max magnitude error {comparison['magnitude_error_db']['max']:.2f} dB <= {pass_criteria['mag_error_max_db']} dB")
    
    if comparison['correlation'] < pass_criteria['correlation']:
        logger.warning(f"FAIL: Correlation {comparison['correlation']:.4f} < {pass_criteria['correlation']}")
        passed = False
    else:
        logger.info(f"PASS: Correlation {comparison['correlation']:.4f} >= {pass_criteria['correlation']}")
    
    comparison['passed'] = passed
    comparison['channel_type'] = 'simple'
    
    return comparison, freq, H_mag, H_phase, H_ref_mag, H_ref_phase


def verify_sparam_channel(waveform_file, metadata, output_dir):
    """
    Verify S-parameter channel model results.
    
    Args:
        waveform_file: Path to waveform CSV
        metadata: Metadata dictionary
        output_dir: Output directory for results
        
    Returns:
        verification results dictionary
    """
    logger.info("=" * 60)
    logger.info("Verifying S-Parameter Channel Model")
    logger.info("=" * 60)
    
    # Load waveform
    time, input_sig, output_sig, sample_rate = load_waveform(waveform_file)
    
    # Compute frequency response from simulation
    freq_sim, H_sim, H_sim_mag, H_sim_phase = compute_frequency_response(
        time, input_sig, output_sig, sample_rate
    )
    
    # Get config file path
    config_file = metadata.get('config_file', '')
    
    if not config_file:
        logger.error("No config file specified in metadata")
        return {'passed': False, 'error': 'No config file'}, None, None, None, None, None
    
    # Load config file to get S-parameter source
    config_path = Path(config_file)
    if not config_path.is_absolute():
        # Assume relative to project root
        config_path = Path(__file__).parent.parent / config_file
    
    try:
        with open(config_path, 'r') as f:
            config = json.load(f)
    except Exception as e:
        logger.error(f"Failed to load config file: {e}")
        return {'passed': False, 'error': str(e)}, None, None, None, None, None
    
    # Get source S-parameter file from metadata
    source_file = config.get('metadata', {}).get('source_file', '')
    
    if not source_file or source_file == 'example.s2p':
        # No original S-parameter file available, compare with rational fit
        logger.info("No original S-parameter file, comparing with rational fit from config")
        
        # Get rational filter coefficients
        filter_data = config.get('filters', {}).get('S21', {})
        if not filter_data:
            logger.error("No S21 filter data in config")
            return {'passed': False, 'error': 'No filter data'}, None, None, None, None, None
        
        num = np.array(filter_data.get('num', [1.0]))
        den = np.array(filter_data.get('den', [1.0]))
        
        # Compute frequency response of rational fit
        s = 1j * 2.0 * np.pi * freq_sim
        H_ref = np.polyval(num, s) / np.polyval(den, s)
        H_ref_mag = 20.0 * np.log10(np.abs(H_ref) + 1e-12)
        H_ref_phase = np.angle(H_ref, deg=True)
        
    else:
        # Load original S-parameter file
        sparam_path = Path(source_file)
        if not sparam_path.is_absolute():
            sparam_path = Path(__file__).parent.parent / source_file
        
        logger.info(f"Loading original S-parameters from: {sparam_path}")
        
        try:
            reader = TouchstoneReader(str(sparam_path))
            freq_orig, s_matrix = reader.read()
            S21 = reader.get_sparam(2, 1)  # S21
            
            H_ref_mag = 20.0 * np.log10(np.abs(S21) + 1e-12)
            H_ref_phase = np.angle(S21, deg=True)
            
        except Exception as e:
            logger.error(f"Failed to load S-parameter file: {e}")
            return {'passed': False, 'error': str(e)}, None, None, None, None, None
    
    # Compare responses
    comparison = compare_frequency_responses(
        freq_sim, H_sim_mag, H_sim_phase,
        freq_sim, H_ref_mag, H_ref_phase,
        freq_min=1e6, freq_max=min(20e9, sample_rate / 2)
    )
    
    # Determine pass/fail (more relaxed criteria for S-param due to fitting errors)
    pass_criteria = {
        'mag_error_rmse_db': 5.0,      # RMSE < 5 dB
        'mag_error_max_db': 10.0,      # Max error < 10 dB
        'correlation': 0.90            # Correlation > 0.90
    }
    
    passed = True
    if comparison['magnitude_error_db']['rmse'] > pass_criteria['mag_error_rmse_db']:
        logger.warning(f"FAIL: Magnitude RMSE {comparison['magnitude_error_db']['rmse']:.2f} dB > {pass_criteria['mag_error_rmse_db']} dB")
        passed = False
    else:
        logger.info(f"PASS: Magnitude RMSE {comparison['magnitude_error_db']['rmse']:.2f} dB <= {pass_criteria['mag_error_rmse_db']} dB")
    
    if comparison['magnitude_error_db']['max'] > pass_criteria['mag_error_max_db']:
        logger.warning(f"FAIL: Max magnitude error {comparison['magnitude_error_db']['max']:.2f} dB > {pass_criteria['mag_error_max_db']} dB")
        passed = False
    else:
        logger.info(f"PASS: Max magnitude error {comparison['magnitude_error_db']['max']:.2f} dB <= {pass_criteria['mag_error_max_db']} dB")
    
    if comparison['correlation'] < pass_criteria['correlation']:
        logger.warning(f"FAIL: Correlation {comparison['correlation']:.4f} < {pass_criteria['correlation']}")
        passed = False
    else:
        logger.info(f"PASS: Correlation {comparison['correlation']:.4f} >= {pass_criteria['correlation']}")
    
    comparison['passed'] = passed
    comparison['channel_type'] = 'sparam'
    
    return comparison, freq_sim, H_sim_mag, H_sim_phase, H_ref_mag, H_ref_phase


def save_verification_results(results, output_file):
    """
    Save verification results to JSON file.
    
    Args:
        results: Results dictionary
        output_file: Output file path
    """
    with open(output_file, 'w') as f:
        json.dump(results, f, indent=2)
    logger.info(f"Verification results saved to: {output_file}")


def print_summary(results):
    """
    Print verification summary.
    
    Args:
        results: Results dictionary
    """
    print("\n" + "=" * 60)
    print("Channel Verification Summary")
    print("=" * 60)
    
    if 'error' in results:
        print(f"ERROR: {results['error']}")
        return
    
    print(f"Channel Type: {results.get('channel_type', 'unknown')}")
    print(f"Frequency Range: {results['frequency_range_hz'][0]/1e9:.3f} - {results['frequency_range_hz'][1]/1e9:.3f} GHz")
    print(f"Number of Points: {results['num_points']}")
    
    print("\nMagnitude Error (dB):")
    print(f"  Mean:  {results['magnitude_error_db']['mean']:.3f}")
    print(f"  Std:   {results['magnitude_error_db']['std']:.3f}")
    print(f"  Max:   {results['magnitude_error_db']['max']:.3f}")
    print(f"  RMSE:  {results['magnitude_error_db']['rmse']:.3f}")
    
    print("\nPhase Error (degrees):")
    print(f"  Mean:  {results['phase_error_deg']['mean']:.3f}")
    print(f"  Std:   {results['phase_error_deg']['std']:.3f}")
    print(f"  Max:   {results['phase_error_deg']['max']:.3f}")
    
    print(f"\nCorrelation: {results['correlation']:.4f}")
    
    status = "PASS" if results['passed'] else "FAIL"
    print(f"\nOverall Result: {status}")
    print("=" * 60)


def main():
    """Command-line interface."""
    parser = argparse.ArgumentParser(
        description='Verify channel simulation results against expected response'
    )
    parser.add_argument('waveform', nargs='?',
                       help='Waveform CSV file (default: channel_sparam_waveform.csv)')
    parser.add_argument('metadata', nargs='?',
                       help='Metadata JSON file (default: channel_sparam_metadata.json)')
    parser.add_argument('-o', '--output', default='channel_verification_results.json',
                       help='Output results file (default: channel_verification_results.json)')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Enable verbose output')
    
    args = parser.parse_args()
    
    # Configure logging
    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=level, format='%(levelname)s: %(message)s')
    
    # Determine input files
    waveform_file = args.waveform or 'channel_sparam_waveform.csv'
    metadata_file = args.metadata or 'channel_sparam_metadata.json'
    
    # Check if files exist
    waveform_path = Path(waveform_file)
    metadata_path = Path(metadata_file)
    
    if not waveform_path.is_absolute():
        waveform_path = Path.cwd() / waveform_file
    if not metadata_path.is_absolute():
        metadata_path = Path.cwd() / metadata_file
    
    if not waveform_path.exists():
        logger.error(f"Waveform file not found: {waveform_path}")
        # Try build directory
        waveform_path = Path(__file__).parent.parent / 'build' / waveform_file
        metadata_path = Path(__file__).parent.parent / 'build' / metadata_file
        
        if not waveform_path.exists():
            logger.error(f"Waveform file not found in build directory either: {waveform_path}")
            sys.exit(1)
    
    if not metadata_path.exists():
        logger.error(f"Metadata file not found: {metadata_path}")
        sys.exit(1)
    
    # Load metadata
    metadata = load_metadata(str(metadata_path))
    
    # Determine verification type
    channel_model = metadata.get('channel_model', 'simple_lpf')
    
    try:
        if channel_model == 'simple_lpf':
            results, freq, H_meas_mag, H_meas_phase, H_ref_mag, H_ref_phase = \
                verify_simple_channel(str(waveform_path), metadata, str(Path(args.output).parent))
        else:
            results, freq, H_meas_mag, H_meas_phase, H_ref_mag, H_ref_phase = \
                verify_sparam_channel(str(waveform_path), metadata, str(Path(args.output).parent))
        
        # Save results
        save_verification_results(results, args.output)
        
        # Print summary
        print_summary(results)
        
        # Return exit code based on pass/fail
        sys.exit(0 if results['passed'] else 1)
        
    except Exception as e:
        logger.exception("Verification failed")
        sys.exit(1)


if __name__ == '__main__':
    main()
