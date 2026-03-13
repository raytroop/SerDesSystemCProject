"""Jitter injection using Dual-Dirac model for statistical eye analysis."""

import numpy as np
from scipy.stats import norm


class JitterInjector:
    """Inject Dual-Dirac jitter into eye diagram PDF.
    
    Models deterministic jitter (DJ) and random jitter (RJ) using
    the Dual-Dirac model from OIF-CEI standard.
    
    Example:
        >>> injector = JitterInjector()
        >>> jittered_pdf = injector.inject(pdf, dj=0.0125, rj=0.015, 
        ...                                samples_per_symbol=8)
    """
    
    def inject(self,
               pdf: np.ndarray,
               dj: float,
               rj: float,
               samples_per_symbol: int,
               jitter_xaxis_step_size: float = 1.0) -> np.ndarray:
        """Inject Dual-Dirac jitter into PDF.
        
        Args:
            pdf: Input PDF (2D array: voltage_bins x time_bins)
            dj: Deterministic jitter amplitude (UI)
            rj: Random jitter sigma (UI)
            samples_per_symbol: Samples per UI
            jitter_xaxis_step_size: Time step size for jitter model
            
        Returns:
            PDF with jitter injected
        """
        vh_size, window_size = pdf.shape
        
        # Convert UI to samples
        mu_jitter = dj * samples_per_symbol
        sigma_jitter = rj * samples_per_symbol
        
        # Create jitter time axis
        x_axis = np.linspace(-(window_size-1), window_size-1, 
                            int(2*(window_size-1)/jitter_xaxis_step_size)+1)
        idx_middle = int((len(x_axis) - 1) / 2)
        num_steps = int(1 / jitter_xaxis_step_size)
        
        # Dual-Dirac: two Gaussians at ±mu_jitter
        jitter_pdf1 = norm.pdf(x_axis, -mu_jitter, sigma_jitter)
        jitter_pdf2 = norm.pdf(x_axis, mu_jitter, sigma_jitter)
        jitter_pdf = (jitter_pdf1 + jitter_pdf2) / np.sum(jitter_pdf1 + jitter_pdf2)
        
        # Apply jitter to each time slice
        result = np.zeros((vh_size, window_size))
        
        for i in range(window_size):
            new_pdf = np.zeros(vh_size)
            for j in range(window_size):
                idx = idx_middle + (j - i) * num_steps
                if 0 <= idx < len(jitter_pdf):
                    new_pdf += pdf[:, j] * jitter_pdf[idx]
            
            new_pdf = new_pdf / np.sum(new_pdf) if np.sum(new_pdf) > 0 else new_pdf
            result[:, i] = new_pdf
        
        return result
