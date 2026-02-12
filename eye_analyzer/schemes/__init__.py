"""
Eye Diagram Analysis Schemes

This module provides two complementary eye diagram analysis schemes:
- SamplerCentricScheme: Streaming scheme based on actual CDR sampling timestamps
- GoldenCdrScheme: Standard scheme based on ideal clock (Golden CDR)
"""

from .base import BaseScheme
from .sampler_centric import SamplerCentricScheme
from .golden_cdr import GoldenCdrScheme

__all__ = ['BaseScheme', 'SamplerCentricScheme', 'GoldenCdrScheme']
