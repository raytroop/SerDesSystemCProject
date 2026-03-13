"""Statistical eye diagram analysis modules.

This package provides tools for statistical eye diagram analysis based on
channel pulse response, supporting both NRZ and PAM4 modulation formats.
"""

from .pulse_response import PulseResponseProcessor
from .noise_injector import NoiseInjector
from .jitter_injector import JitterInjector
from .ber_calculator import BERCalculator, calculate_ber_simplified

__all__ = [
    'PulseResponseProcessor',
    'NoiseInjector',
    'JitterInjector',
    'BERCalculator',
    'calculate_ber_simplified',
]
