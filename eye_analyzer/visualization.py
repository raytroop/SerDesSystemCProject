"""
Eye Diagram Visualization Module

This module provides visualization utilities for eye diagram analysis,
using MATLAB-style colormap with imshow for both Golden CDR and Sampler-Centric schemes.

Features:
- MATLAB-style colormap (Blue -> Green -> Yellow -> Red -> White)
- Unified imshow-based visualization for all schemes
- 2-UI centered display
- Multiple output formats (PNG, SVG, PDF)
"""

from typing import Dict, Any, Optional, Tuple
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap
from scipy.ndimage import gaussian_filter


def create_matlab_colormap() -> LinearSegmentedColormap:
    """
    Create MATLAB-style colormap for eye diagram.
    
    MATLAB color mapping logic:
    - PD=0: White (1,1,1) - background
    - PD>0: 
        R = max(2*PD - 1, 0)
        G = min(2*PD, 2 - 2*PD)
        B = max(1 - 2*PD, 0)
    
    Color progression:
    - Low density (PD≈0): Blue (0,0,1)
    - Medium density (PD≈0.5): Green (0,1,0)
    - High density (PD≈0.75): Yellow/Red (1,1,0)/(1,0,0)
    - Peak density (PD≈1): White (1,1,1)
    
    Returns:
        LinearSegmentedColormap object
    """
    # Create 256-level colormap based on MATLAB algorithm
    n_colors = 256
    colors = []
    
    for i in range(n_colors):
        # Normalize to [0, 1]
        pd = i / (n_colors - 1)
        
        # MATLAB color calculation (background is handled separately)
        if pd == 0:
            r, g, b = 1.0, 1.0, 1.0  # White for zero
        else:
            r = max(2 * pd - 1, 0)
            g = min(2 * pd, 2 - 2 * pd)
            b = max(1 - 2 * pd, 0)
        
        colors.append((r, g, b))
    
    return LinearSegmentedColormap.from_list('matlab_eye', colors, N=n_colors)


def plot_eye_diagram(eye_matrix: np.ndarray,
                     xedges: np.ndarray,
                     yedges: np.ndarray,
                     ui: float,
                     scheme: str,
                     metrics: Dict[str, Any],
                     output_path: str,
                     dpi: int = 300,
                     figsize: Tuple[float, float] = (12, 8),
                     smooth_sigma: float = 1.0) -> None:
    """
    Plot eye diagram using imshow with MATLAB-style colormap.
    
    Unified visualization for both Golden CDR and Sampler-Centric schemes:
    - Uses imshow for consistent rendering
    - MATLAB-style colormap (Blue -> Green -> Yellow -> Red)
    - White background
    - 2-UI display window
    
    Args:
        eye_matrix: 2D density matrix (ui_bins x amp_bins)
        xedges: Time/phase axis bin edges
        yedges: Voltage axis bin edges (in volts)
        ui: Unit interval in seconds
        scheme: 'sampler_centric' or 'golden_cdr'
        metrics: Analysis metrics dictionary
        output_path: Output file path
        dpi: Output image DPI (default: 300)
        figsize: Figure size in inches (default: (12, 8))
        smooth_sigma: Gaussian smoothing sigma (default: 1.0 for smoother edges)
    """
    fig, ax = plt.subplots(figsize=figsize)
    
    # Data preprocessing
    hist_display = eye_matrix.T.copy()
    if smooth_sigma > 0:
        hist_display = gaussian_filter(hist_display, sigma=smooth_sigma)
    
    # Normalize to [0, 1] for colormap
    hist_max = hist_display.max()
    if hist_max > 0:
        hist_normalized = hist_display / hist_max
    else:
        hist_normalized = hist_display
    
    # Coordinate conversion to picoseconds
    ui_ps = ui * 1e12
    xedges_ps = xedges * ui_ps
    
    # Determine labels based on scheme
    if scheme == 'sampler_centric':
        xlabel = 'Time relative to sampling moment [ps]'
        title_suffix = 'Sampler-Centric'
    else:
        xlabel = 'Phase [ps]'
        title_suffix = 'Golden CDR'
    
    # Create MATLAB-style colormap
    cmap = create_matlab_colormap()
    
    # Plot using imshow with MATLAB colormap
    # Set background to white for zero values
    hist_masked = np.ma.masked_where(hist_normalized <= 0, hist_normalized)
    
    im = ax.imshow(
        hist_normalized,
        origin='lower',
        aspect='auto',
        extent=[xedges_ps[0], xedges_ps[-1], yedges[0], yedges[-1]],
        cmap=cmap,
        vmin=0,
        vmax=1,
        interpolation='bilinear'
    )
    
    # Set white background for areas with no data
    ax.set_facecolor('white')
    fig.patch.set_facecolor('white')
    
    # Add UI boundary lines
    _add_eye_boundaries(ax, scheme, ui_ps, (xedges_ps[0], xedges_ps[-1]))
    
    # Zero voltage reference line
    ax.axhline(y=0, color='gray', linestyle=':', alpha=0.5, linewidth=0.8)
    
    # Add metrics text box
    _add_metrics_text(ax, metrics, ui, scheme)
    
    # Labels and title
    ax.set_xlabel(xlabel, fontsize=12)
    ax.set_ylabel('Voltage [V]', fontsize=12)
    ax.set_title(f'Statistical Eye Diagram ({title_suffix})', fontsize=14)
    
    # Set axis limits
    ax.set_xlim(xedges_ps[0], xedges_ps[-1])
    ax.set_ylim(yedges[0], yedges[-1])
    
    # Add grid
    ax.grid(True, alpha=0.3, linestyle='--')
    
    # Save figure
    plt.tight_layout()
    plt.savefig(output_path, dpi=dpi, bbox_inches='tight', facecolor='white')
    plt.close(fig)


def _add_eye_boundaries(ax, scheme: str, ui_ps: float, x_range: tuple = None) -> None:
    """
    Add UI boundary lines and center markers to the plot.
    
    Args:
        ax: Matplotlib axes object
        scheme: 'sampler_centric' or 'golden_cdr'
        ui_ps: Unit interval in picoseconds
        x_range: Optional (xmin, xmax) to determine boundaries
    """
    if scheme == 'sampler_centric':
        # Sampler-Centric: 2-UI window [-1, +1]
        boundaries = [-1, -0.5, 0, 0.5, 1]
        for b in boundaries:
            x_line = b * ui_ps
            if b == 0:
                ax.axvline(x=x_line, color='red', linestyle='-', 
                          alpha=0.8, linewidth=1.5, label='Sampling moment')
            else:
                ax.axvline(x=x_line, color='gray', linestyle='--', 
                          alpha=0.4, linewidth=0.8)
    else:
        # Golden CDR: 2-UI centered window [-0.5, 1.5]
        # Eye center at 0.5 UI (center of display)
        boundaries = [-0.5, 0, 0.5, 1.0, 1.5]
        for b in boundaries:
            x_line = b * ui_ps
            if b == 0.5:
                # Eye center - ideal sampling point (red solid)
                ax.axvline(x=x_line, color='red', linestyle='-', 
                          alpha=0.8, linewidth=1.5, label='Eye center')
            elif b == 0 or b == 1.0:
                # UI boundaries (gray solid)
                ax.axvline(x=x_line, color='gray', linestyle='-', 
                          alpha=0.6, linewidth=1.0)
            else:
                # Window edges (gray dashed)
                ax.axvline(x=x_line, color='gray', linestyle='--', 
                          alpha=0.4, linewidth=0.8)


def _add_metrics_text(ax, metrics: Dict[str, Any], ui: float, scheme: str) -> None:
    """
    Add metrics text box to the plot.
    """
    ui_ps = ui * 1e12
    data_rate = 1 / ui / 1e9
    
    lines = [
        f"Data Rate: {data_rate:.1f} Gbps",
        f"UI: {ui_ps:.1f} ps",
        f"Eye Height: {metrics.get('eye_height', 0)*1000:.2f} mV",
        f"Eye Width: {metrics.get('eye_width', 0):.3f} UI",
        f"Scheme: {scheme}",
    ]
    
    if scheme == 'golden_cdr':
        rj = metrics.get('rj_sigma', 0)
        dj = metrics.get('dj_pp', 0)
        if rj > 0 or dj > 0:
            lines.append(f"RJ: {rj*1e12:.2f} ps")
            lines.append(f"DJ: {dj*1e12:.2f} ps")
    
    textstr = '\n'.join(lines)
    
    props = dict(boxstyle='round', facecolor='white', edgecolor='gray', alpha=0.8)
    ax.text(0.02, 0.98, textstr, transform=ax.transAxes, fontsize=10,
            verticalalignment='top', bbox=props, family='monospace')


def save_eye_diagram(scheme_obj, metrics: Dict[str, Any], output_path: str,
                     **kwargs) -> None:
    """
    Convenience function to save eye diagram from a scheme object.
    
    Args:
        scheme_obj: SamplerCentricScheme or GoldenCdrScheme object
        metrics: Analysis metrics dictionary
        output_path: Output file path
        **kwargs: Additional arguments passed to plot_eye_diagram
    """
    eye_matrix = scheme_obj.get_eye_matrix()
    if eye_matrix is None:
        raise ValueError("No eye matrix available. Run analyze() first.")
    
    plot_eye_diagram(
        eye_matrix=eye_matrix,
        xedges=scheme_obj.get_xedges(),
        yedges=scheme_obj.get_yedges(),
        ui=scheme_obj.ui,
        scheme=metrics.get('scheme', 'unknown'),
        metrics=metrics,
        output_path=output_path,
        **kwargs
    )
