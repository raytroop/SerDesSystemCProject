"""Gaussian noise injection for statistical eye analysis."""

import numpy as np
from scipy.stats import norm


class NoiseInjector:
    """Inject Gaussian noise into ISI PDF.
    
    Example:
        >>> injector = NoiseInjector()
        >>> noisy_pdf = injector.inject(isi_pdf, mu=0, sigma=1e-4)
    """
    
    def inject(self,
               pdf: np.ndarray,
               sigma: float,
               mu: float = 0.0,
               vh_edges: np.ndarray = None) -> np.ndarray:
        """Inject Gaussian noise into PDF.
        
        Args:
            pdf: Input PDF (2D array: voltage_bins x time_bins)
            sigma: Noise standard deviation (V)
            mu: Noise mean (V), default 0
            vh_edges: Voltage bin edges (optional)
            
        Returns:
            PDF with noise injected
        """
        vh_size = pdf.shape[0]
        
        if vh_edges is None:
            vh_edges = np.linspace(-1, 1, vh_size + 1)
        
        vh_centers = (vh_edges[:-1] + vh_edges[1:]) / 2
        
        # Create Gaussian noise PDF
        noise_pdf = norm.pdf(vh_centers, mu, sigma)
        noise_pdf = noise_pdf / np.sum(noise_pdf) if np.sum(noise_pdf) > 0 else noise_pdf
        
        # Convolve each time slice
        result = np.zeros_like(pdf)
        for i in range(pdf.shape[1]):
            convolved = np.convolve(noise_pdf, pdf[:, i], mode='same')
            result[:, i] = convolved / np.sum(convolved) if np.sum(convolved) > 0 else convolved
        
        return result
