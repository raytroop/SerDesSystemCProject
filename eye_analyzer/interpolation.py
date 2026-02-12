"""
Interpolation Module for Eye Diagram Analysis

This module provides interpolation utilities for the Sampler-Centric scheme,
which requires mapping variable-length windows to a fixed-size grid.
"""

from typing import Tuple, Optional
import numpy as np
from scipy.interpolate import CubicSpline, interp1d, Akima1DInterpolator


def interpolate_window(normalized_time: np.ndarray, 
                       window_voltage: np.ndarray,
                       target_bins: int = 128,
                       method: str = 'cubic') -> np.ndarray:
    """
    Interpolate window data to a uniform grid.
    
    Maps non-uniformly distributed sample points to a uniform grid
    for eye diagram accumulation.
    
    Args:
        normalized_time: Normalized time array in [-1, +1] relative to sampling moment
        window_voltage: Voltage values corresponding to normalized_time
        target_bins: Number of output bins (default: 128)
        method: Interpolation method ('cubic', 'linear', 'akima')
                - 'cubic': CubicSpline (smooth, accurate) - default
                - 'linear': Linear interpolation (fast, stable)
                - 'akima': Akima interpolation (avoids overshoot)
                
    Returns:
        Interpolated voltage array of shape (target_bins,)
        
    Raises:
        ValueError: If input arrays are invalid or method is unknown
    """
    if len(normalized_time) < 2:
        return np.zeros(target_bins)
    
    if len(normalized_time) != len(window_voltage):
        raise ValueError(
            f"Array length mismatch: time={len(normalized_time)}, "
            f"voltage={len(window_voltage)}"
        )
    
    # Target grid: uniform points in [-1, +1]
    target_time = np.linspace(-1, 1, target_bins)
    
    # Select interpolation method
    if method == 'cubic':
        try:
            interpolator = CubicSpline(normalized_time, window_voltage, 
                                       extrapolate=True)
        except ValueError:
            # Fall back to linear if cubic fails
            interpolator = interp1d(normalized_time, window_voltage, 
                                    kind='linear', 
                                    fill_value='extrapolate')
    elif method == 'linear':
        interpolator = interp1d(normalized_time, window_voltage, 
                                kind='linear', 
                                fill_value='extrapolate')
    elif method == 'akima':
        if len(normalized_time) < 4:
            # Akima needs at least 4 points, fall back to linear
            interpolator = interp1d(normalized_time, window_voltage, 
                                    kind='linear', 
                                    fill_value='extrapolate')
        else:
            try:
                interpolator = Akima1DInterpolator(normalized_time, window_voltage)
            except ValueError:
                interpolator = interp1d(normalized_time, window_voltage, 
                                        kind='linear', 
                                        fill_value='extrapolate')
    else:
        raise ValueError(
            f"Unknown interpolation method '{method}'. "
            f"Valid methods: 'cubic', 'linear', 'akima'"
        )
    
    # Perform interpolation
    interpolated_voltage = interpolator(target_time)
    
    return interpolated_voltage


def is_valid_window(t_center: float, t_start: float, t_end: float, 
                    ui: float) -> bool:
    """
    Check if a sampling moment is valid (not at boundary).
    
    A sampling moment is valid if its 2-UI window falls completely
    within the waveform data range.
    
    Args:
        t_center: Sampling moment (center of window)
        t_start: Start time of waveform data
        t_end: End time of waveform data
        ui: Unit interval in seconds
        
    Returns:
        True if the window is valid (not at boundary), False otherwise
    """
    window_start = t_center - ui
    window_end = t_center + ui
    
    return window_start >= t_start and window_end <= t_end


def extract_window(time_array: np.ndarray, voltage_array: np.ndarray,
                   t_center: float, ui: float) -> Tuple[np.ndarray, np.ndarray]:
    """
    Extract a 2-UI window centered at the given time.
    
    Uses binary search (np.searchsorted) for fast window extraction.
    
    Args:
        time_array: Full time array (assumed sorted)
        voltage_array: Full voltage array
        t_center: Center time of the window (sampling moment)
        ui: Unit interval in seconds
        
    Returns:
        Tuple of (window_time, window_voltage) arrays
    """
    t_start = t_center - ui
    t_end = t_center + ui
    
    # Binary search for window boundaries
    idx_start = np.searchsorted(time_array, t_start, side='left')
    idx_end = np.searchsorted(time_array, t_end, side='right')
    
    # Ensure valid indices
    idx_start = max(0, idx_start)
    idx_end = min(len(time_array), idx_end)
    
    return time_array[idx_start:idx_end], voltage_array[idx_start:idx_end]


def normalize_time_to_window(window_time: np.ndarray, 
                             t_center: float, 
                             ui: float) -> np.ndarray:
    """
    Normalize window time to [-1, +1] range.
    
    The sampling moment (t_center) maps to 0.
    
    Args:
        window_time: Time array within the window
        t_center: Center time (sampling moment)
        ui: Unit interval in seconds
        
    Returns:
        Normalized time array in [-1, +1]
    """
    return (window_time - t_center) / ui


def get_valid_sampler_indices(sampler_timestamps: np.ndarray,
                              t_start: float, t_end: float,
                              ui: float) -> np.ndarray:
    """
    Get indices of valid (non-boundary) sampling moments.
    
    Filters out sampling moments whose 2-UI windows would extend
    beyond the waveform data boundaries.
    
    Args:
        sampler_timestamps: Array of CDR sampling timestamps
        t_start: Start time of waveform data
        t_end: End time of waveform data
        ui: Unit interval in seconds
        
    Returns:
        Array of indices into sampler_timestamps for valid samples
    """
    valid_start = t_start + ui
    valid_end = t_end - ui
    
    valid_mask = (sampler_timestamps >= valid_start) & (sampler_timestamps <= valid_end)
    return np.where(valid_mask)[0]
