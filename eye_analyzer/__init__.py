"""
EyeAnalyzer - SerDes Link Eye Diagram Analysis Tool

A Python tool for analyzing eye diagrams from SystemC-AMS simulation output.
Supports eye diagram construction, eye height/width calculation, jitter decomposition,
and visualization.

Two analysis schemes are supported:
- Sampler-Centric: Uses CDR sampling timestamps as time reference (2-UI window)
- Golden CDR: Uses ideal clock for standardized assessment (1-UI window)

Version: 2.0.0
Author: SerDes SystemC Project Team
"""

# Original EyeAnalyzer (Golden CDR full implementation)
from .core import EyeAnalyzer, analyze_eye

# New unified analyzer supporting both schemes
from .analyzer import UnifiedEyeAnalyzer

# Individual scheme classes
from .schemes import SamplerCentricScheme, GoldenCdrScheme, BaseScheme

# Visualization utilities
from .visualization import plot_eye_diagram, save_eye_diagram

# Data loading utilities
from .io import auto_load_waveform

# Jitter decomposition
from .jitter import JitterDecomposer

# Interpolation utilities (for advanced usage)
from .interpolation import interpolate_window, is_valid_window

__version__ = "2.0.0"
__all__ = [
    # Main analyzers
    "EyeAnalyzer",           # Original (Golden CDR full implementation)
    "UnifiedEyeAnalyzer",    # New unified entry point
    "analyze_eye",           # Convenience function
    
    # Scheme classes
    "SamplerCentricScheme",
    "GoldenCdrScheme",
    "BaseScheme",
    
    # Visualization
    "plot_eye_diagram",
    "save_eye_diagram",
    
    # Data loading
    "auto_load_waveform",
    
    # Jitter analysis
    "JitterDecomposer",
    
    # Interpolation (advanced)
    "interpolate_window",
    "is_valid_window",
]