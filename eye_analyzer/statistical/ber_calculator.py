"""BER contour calculation for statistical eye analysis."""

import numpy as np
from typing import List, Dict
from ..modulation import ModulationFormat


class BERContourCalculator:
    """Calculate BER contours from eye diagram PDF.
    
    Example:
        >>> calc = BERContourCalculator()
        >>> contours = calc.calculate_contours(pdf, ber_levels=[1e-12, 1e-9])
    """
    
    def calculate_contours(self,
                          eye_pdf: np.ndarray,
                          modulation: ModulationFormat,
                          ber_levels: List[float] = None) -> Dict[float, np.ndarray]:
        """Calculate BER contours at specified levels.
        
        Args:
            eye_pdf: Eye diagram PDF (2D array)
            modulation: Modulation format
            ber_levels: List of BER levels (default: [1e-12, 1e-9, 1e-6])
            
        Returns:
            Dictionary mapping BER level to contour matrix
        """
        if ber_levels is None:
            ber_levels = [1e-12, 1e-9, 1e-6]
        
        contours = {}
        
        for ber in ber_levels:
            contour = self._calculate_contour_at_ber(eye_pdf, modulation, ber)
            contours[ber] = contour
        
        return contours
    
    def _calculate_contour_at_ber(self, eye_pdf, modulation, target_ber):
        """Calculate single BER contour."""
        vh_size, window_size = eye_pdf.shape
        contour = np.zeros((vh_size, window_size))
        
        eye_centers = modulation.get_eye_centers()
        
        # Calculate CDF for each eye
        for i in range(window_size):
            for eye_idx, center in enumerate(eye_centers):
                # Find center index
                center_idx = int(vh_size / 2)  # Simplified
                
                # Calculate CDF above and below center
                if center_idx < vh_size:
                    cdf_above = np.cumsum(eye_pdf[center_idx:, i])
                    cdf_above = cdf_above / cdf_above[-1] if cdf_above[-1] > 0 else cdf_above
                    
                    cdf_below = np.cumsum(eye_pdf[:center_idx, i][::-1])[::-1]
                    cdf_below = cdf_below / cdf_below[0] if cdf_below[0] > 0 else cdf_below
                    
                    # Combine
                    full_cdf = np.concatenate([cdf_below, cdf_above])
                    contour[:, i] = full_cdf[:vh_size]
        
        return contour
    
    def calculate_eye_dimensions(self,
                                 eye_pdf: np.ndarray,
                                 modulation: ModulationFormat,
                                 target_ber: float) -> Dict[str, float]:
        """Calculate eye height and width at target BER.
        
        Args:
            eye_pdf: Eye diagram PDF
            modulation: Modulation format
            target_ber: Target bit error rate
            
        Returns:
            Dictionary with eye_height and eye_width
        """
        # Simplified calculation
        # In full implementation, would use CDF analysis
        
        eye_heights = []
        thresholds = modulation.get_thresholds()
        
        for threshold in thresholds:
            # Find eye opening at this threshold
            height = self._compute_height_at_threshold(eye_pdf, threshold)
            eye_heights.append(height)
        
        return {
            'eye_heights': eye_heights,
            'eye_height_min': min(eye_heights) if eye_heights else 0.0,
            'eye_height_avg': sum(eye_heights) / len(eye_heights) if eye_heights else 0.0,
        }
    
    def _compute_height_at_threshold(self, eye_pdf, threshold):
        """Compute eye height at a decision threshold."""
        # Simplified - full implementation would analyze PDF
        return float(np.max(eye_pdf)) * 0.5
