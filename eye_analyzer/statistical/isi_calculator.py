"""ISI PDF calculation using convolution or brute force methods."""

import numpy as np
from typing import Literal
from ..modulation import ModulationFormat


class ISICalculator:
    """Calculate ISI (Inter-Symbol Interference) probability distribution.
    
    Two methods supported:
    - 'convolution': Fast O(N*L) using PDF convolution
    - 'brute_force': Exact O(M^N) using enumeration
    
    Example:
        >>> calc = ISICalculator(method='convolution')
        >>> isi_pdf = calc.calculate(pulse, PAM4(), samples_per_symbol=8)
    """
    
    def __init__(self, method: Literal['convolution', 'brute_force'] = 'convolution'):
        """Initialize ISI calculator.
        
        Args:
            method: 'convolution' (fast) or 'brute_force' (exact)
        """
        self.method = method
    
    def calculate(self,
                  pulse: np.ndarray,
                  modulation: ModulationFormat,
                  samples_per_symbol: int,
                  sample_size: int,
                  vh_size: int = 2048) -> np.ndarray:
        """Calculate ISI PDF.
        
        Args:
            pulse: Processed pulse response
            modulation: Modulation format
            samples_per_symbol: Samples per UI
            sample_size: Number of ISI symbols to consider
            vh_size: Voltage histogram bins
            
        Returns:
            2D array: ISI PDF [voltage_bins, time_bins]
        """
        idx_main = int(np.argmax(np.abs(pulse)))
        window_size = samples_per_symbol
        
        # Voltage range
        A_max = np.abs(pulse[idx_main])
        A_window = A_max * 2
        
        vh_edges = np.linspace(-A_window, A_window, vh_size + 1)
        
        # Get modulation levels
        levels = modulation.get_levels().reshape(1, -1)
        
        pdf_list = []
        
        for idx in range(-window_size // 2, window_size // 2):
            idx_sampled = idx_main + idx
            
            # Sample pulse response at symbol intervals
            sampled_points = []
            i = 0
            while idx_sampled - i * samples_per_symbol >= 0:
                sampled_points.append(idx_sampled - i * samples_per_symbol)
                i += 1
            
            j = 1
            while idx_sampled + j * samples_per_symbol < len(pulse):
                sampled_points.append(idx_sampled + j * samples_per_symbol)
                j += 1
            
            sampled_points = sampled_points[:sample_size]
            sampled_amps = np.array([pulse[i] for i in sampled_points]).reshape(-1, 1)
            sampled_amps = sampled_amps @ levels
            
            if self.method == 'convolution':
                pdf = self._calculate_by_convolution(sampled_amps, vh_edges)
            else:
                pdf = self._calculate_by_brute_force(sampled_amps, vh_edges)
            
            pdf_list.append(pdf)
        
        return np.array(pdf_list).T
    
    def _calculate_by_convolution(self, sampled_amps, vh_edges):
        """Calculate ISI PDF using convolution method."""
        pdf, _ = np.histogram(sampled_amps[0], vh_edges)
        pdf = pdf / np.sum(pdf) if np.sum(pdf) > 0 else pdf
        
        for j in range(1, len(sampled_amps)):
            pdf_cursor, _ = np.histogram(sampled_amps[j], vh_edges)
            pdf_cursor = pdf_cursor / np.sum(pdf_cursor) if np.sum(pdf_cursor) > 0 else pdf_cursor
            pdf = np.convolve(pdf, pdf_cursor, mode='same')
            pdf = pdf / np.sum(pdf) if np.sum(pdf) > 0 else pdf
        
        return pdf
    
    def _calculate_by_brute_force(self, sampled_amps, vh_edges):
        """Calculate ISI PDF by enumerating all combinations."""
        grids = np.meshgrid(*[sampled_amps[i] for i in range(len(sampled_amps))])
        all_combs = np.array([g.flatten() for g in grids]).T
        A = np.sum(all_combs, axis=1)
        
        pdf, _ = np.histogram(A, vh_edges)
        pdf = pdf / np.sum(pdf) if np.sum(pdf) > 0 else pdf
        
        return pdf
