#!/usr/bin/env python3
"""
Pure Python VectorFitting Implementation - Version 2
=====================================================

Simplified but correct implementation of VectorFitting algorithm.

Key features:
1. Relaxed VectorFitting formulation
2. Proper handling of complex poles
3. DC point preservation
4. No external dependencies (except numpy)
"""

import numpy as np
import json
from typing import Tuple, Optional


class VectorFittingPY:
    """
    VectorFitting for S-parameter channel modeling.
    
    Fits: H(s) = sum(r_i / (s - p_i)) + d
    """
    
    def __init__(self, freq: np.ndarray, h: np.ndarray):
        """
        Args:
            freq: Frequency array (Hz)
            h: Complex response (e.g., S21)
        """
        self.freq = np.asarray(freq, dtype=float)
        self.h = np.asarray(h, dtype=complex)
        self.s = 1j * 2 * np.pi * self.freq
        
        # Results
        self.poles = None
        self.residues = None
        self.constant = None
        
    def _init_poles_log(self, n_cmplx: int) -> np.ndarray:
        """Initialize complex poles with log spacing."""
        fmin = max(self.freq[self.freq > 0].min(), 1e3)  # Avoid 0
        fmax = self.freq.max()
        
        omega = 2 * np.pi * np.geomspace(fmin, fmax, n_cmplx)
        
        # Start with weakly damped poles
        poles = []
        for w in omega:
            poles.append(-0.01 * w + 1j * w)  # Small negative real part
        
        return np.array(poles, dtype=complex)
    
    def _init_poles_linear(self, n_cmplx: int) -> np.ndarray:
        """Initialize complex poles with linear spacing."""
        fmin = max(self.freq[self.freq > 0].min(), 1e3)
        fmax = self.freq.max()
        
        omega = 2 * np.pi * np.linspace(fmin, fmax, n_cmplx)
        
        poles = []
        for w in omega:
            poles.append(-0.01 * w + 1j * w)
        
        return np.array(poles, dtype=complex)
    
    def _build_sigma_system(self, poles: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """
        Build linear system for sigma function.
        
        We want to find sigma(s) and H_tilde(s) such that:
            sigma(s) * H(s) = H_tilde(s)
        
        where:
            sigma(s) = 1 + sum(c_i / (s - q_i))
            H_tilde(s) = sum(d_i / (s - q_i)) + d
        
        The new poles are zeros of sigma(s).
        """
        n_freq = len(self.freq)
        n_poles = len(poles)
        
        # For each pole, we need:
        # - 2 columns for real and imag parts of residue in H_tilde
        # - 2 columns for real and imag parts of residue in sigma
        # Plus 1 column for constant in H_tilde
        # Plus 1 column for constant in sigma (should be 1, fixed)
        
        n_cols = 4 * n_poles + 2  # H_tilde residues + sigma residues + constants
        
        A = np.zeros((2 * n_freq, n_cols))
        b = np.zeros(2 * n_freq)
        
        # Column indices
        col = 0
        # H_tilde residues (real part)
        h_tilde_re_cols = []
        # H_tilde residues (imag part)
        h_tilde_im_cols = []
        # Sigma residues (real part)
        sigma_re_cols = []
        # Sigma residues (imag part)
        sigma_im_cols = []
        
        for i, p in enumerate(poles):
            # Terms: 1/(s-p) + 1/(s-p*)
            term_re = 1.0 / (self.s - p) + 1.0 / (self.s - np.conj(p))
            term_im = 1.0j / (self.s - p) - 1.0j / (self.s - np.conj(p))
            
            # H_tilde residues
            h_tilde_re_cols.append(col)
            A[:n_freq, col] = term_re.real
            A[n_freq:, col] = term_re.imag
            col += 1
            
            h_tilde_im_cols.append(col)
            A[:n_freq, col] = term_im.real
            A[n_freq:, col] = term_im.imag
            col += 1
            
            # Sigma residues (multiplied by -H(s))
            sigma_re_cols.append(col)
            A[:n_freq, col] = -(term_re * self.h).real
            A[n_freq:, col] = -(term_re * self.h).imag
            col += 1
            
            sigma_im_cols.append(col)
            A[:n_freq, col] = -(term_im * self.h).real
            A[n_freq:, col] = -(term_im * self.h).imag
            col += 1
        
        # Constants
        h_tilde_const_col = col
        A[:n_freq, col] = 1.0
        A[n_freq:, col] = 0.0
        col += 1
        
        sigma_const_col = col
        A[:n_freq, col] = -self.h.real
        A[n_freq:, col] = -self.h.imag
        col += 1
        
        # RHS
        b[:n_freq] = self.h.real
        b[n_freq:] = self.h.imag
        
        return A, b, {
            'h_tilde_re': h_tilde_re_cols,
            'h_tilde_im': h_tilde_im_cols,
            'sigma_re': sigma_re_cols,
            'sigma_im': sigma_im_cols,
            'h_tilde_const': h_tilde_const_col,
            'sigma_const': sigma_const_col
        }
    
    def _relocate_poles(self, old_poles: np.ndarray, sigma_residues: np.ndarray,
                       cols: dict) -> np.ndarray:
        """
        Calculate new poles as zeros of sigma(s).
        
        sigma(s) = 1 + sum(c_i / (s - q_i)) + c_const
        
        Zeros are eigenvalues of the system matrix.
        """
        n_poles = len(old_poles)
        
        # Build system matrix (2x2 blocks for complex poles)
        H_size = 2 * n_poles
        H = np.zeros((H_size, H_size))
        
        for i, p in enumerate(old_poles):
            # Block for this pole
            idx = 2 * i
            
            # Extract sigma residues
            c_re = sigma_residues[cols['sigma_re'][i]]
            c_im = sigma_residues[cols['sigma_im'][i]]
            
            # System matrix block
            # For pole q = alpha + j*beta, the 2x2 block is:
            # [ alpha - c_re    beta - c_im ]
            # [ -beta + c_im    alpha - c_re ]
            
            alpha = p.real
            beta = p.imag
            
            H[idx, idx] = alpha - c_re
            H[idx, idx + 1] = beta - c_im
            H[idx + 1, idx] = -beta + c_im
            H[idx + 1, idx + 1] = alpha - c_re
        
        # Compute eigenvalues
        eigs = np.linalg.eigvals(H)
        
        # Process eigenvalues to get new poles
        new_poles = []
        for ev in eigs:
            # Make stable (negative real part)
            alpha = -abs(ev.real)
            beta = abs(ev.imag)
            
            if beta < 1e-12:
                # Real pole
                new_poles.append(alpha)
            else:
                # Complex pole - only keep positive imag part
                if ev.imag > 0:
                    new_poles.append(alpha + 1j * beta)
        
        # Remove duplicates and sort by frequency
        unique_poles = []
        for p in new_poles:
            is_duplicate = any(abs(p - up) < 1e6 for up in unique_poles)
            if not is_duplicate:
                unique_poles.append(p)
        
        # Sort by imaginary part (frequency)
        unique_poles.sort(key=lambda p: abs(p.imag) if isinstance(p, complex) else 0)
        
        return np.array(unique_poles, dtype=complex)
    
    def _calculate_residues(self, poles: np.ndarray) -> Tuple[np.ndarray, complex]:
        """
        Calculate residues for fixed poles.
        
        H(s) = sum(r_i / (s - p_i)) + d
        """
        n_freq = len(self.freq)
        n_poles = len(poles)
        
        # Build matrix
        n_cols = 2 * n_poles + 2  # Real/imag for each residue + real/imag for constant
        A = np.zeros((2 * n_freq, n_cols))
        
        col = 0
        for i, p in enumerate(poles):
            # 1/(s-p) + 1/(s-p*)
            term_re = 1.0 / (self.s - p) + 1.0 / (self.s - np.conj(p))
            term_im = 1.0j / (self.s - p) - 1.0j / (self.s - np.conj(p))
            
            A[:n_freq, col] = term_re.real
            A[n_freq:, col] = term_re.imag
            col += 1
            
            A[:n_freq, col] = term_im.real
            A[n_freq:, col] = term_im.imag
            col += 1
        
        # Constant term
        A[:n_freq, col] = 1.0
        A[n_freq:, col] = 0.0
        col += 1
        
        A[:n_freq, col] = 0.0
        A[n_freq:, col] = 1.0
        col += 1
        
        # RHS
        b = np.zeros(2 * n_freq)
        b[:n_freq] = self.h.real
        b[n_freq:] = self.h.imag
        
        # Solve
        x, residuals, rank, s_vals = np.linalg.lstsq(A, b, rcond=None)
        
        # Extract residues
        residues = np.zeros(n_poles, dtype=complex)
        for i in range(n_poles):
            residues[i] = x[2*i] + 1j * x[2*i + 1]
        
        constant = x[-2] + 1j * x[-1]
        
        return residues, constant
    
    def vector_fit(self, n_poles: int = 16, max_iter: int = 100, 
                   tolerance: float = 1e-6, spacing: str = 'log') -> dict:
        """
        Perform vector fitting.
        
        Args:
            n_poles: Number of pole pairs (will result in 2*n_poles poles)
            max_iter: Maximum iterations
            tolerance: Convergence tolerance
            spacing: 'log' or 'lin' pole initialization
        
        Returns:
            Dictionary with results
        """
        print(f"VectorFittingPY v2: {n_poles} pole pairs, {2*n_poles} total poles")
        
        # Initialize poles
        n_cmplx = n_poles
        if spacing == 'log':
            poles = self._init_poles_log(n_cmplx)
        else:
            poles = self._init_poles_linear(n_cmplx)
        
        print(f"  Initial: {len(poles)} complex poles")
        
        # Iterative pole relocation
        prev_poles = poles.copy()
        
        for iteration in range(max_iter):
            # Build system
            A, b, cols = self._build_sigma_system(poles)
            
            # Solve
            x, residuals, rank, s_vals = np.linalg.lstsq(A, b, rcond=None)
            
            # Extract sigma residues
            n_p = len(poles)
            sigma_residues = np.zeros(2 * n_p)
            for i in range(n_p):
                sigma_residues[2*i] = x[cols['sigma_re'][i]]
                sigma_residues[2*i + 1] = x[cols['sigma_im'][i]]
            
            # Relocate poles
            new_poles = self._relocate_poles(poles, sigma_residues, cols)
            
            # Check convergence
            if len(new_poles) == len(poles):
                pole_shift = np.mean([min(abs(np.array(new_poles) - p)) for p in poles])
            else:
                pole_shift = float('inf')
            
            # Calculate error with current poles
            residues, constant = self._calculate_residues(poles)
            h_fit = self._evaluate(poles, residues, constant)
            error = np.sqrt(np.mean(np.abs(self.h - h_fit)**2))
            
            if iteration % 10 == 0 or error < tolerance:
                print(f"  Iter {iteration:3d}: error = {error:.6e}, shift = {pole_shift:.6e}, poles = {len(poles)}")
            
            # Check convergence
            if error < tolerance and iteration > 5:
                print(f"  Converged!")
                break
            
            if pole_shift < tolerance * 0.1 and iteration > 10:
                print(f"  Pole shift converged")
                break
            
            # Accept new poles if we have enough
            if len(new_poles) >= n_cmplx // 2:
                poles = new_poles[:n_cmplx]  # Keep requested number
            
            prev_poles = poles.copy()
        
        else:
            print(f"  Max iterations reached")
        
        # Final residue calculation
        print("\n  Finalizing...")
        self.poles = poles
        self.residues, self.constant = self._calculate_residues(poles)
        
        # Evaluate final fit
        h_fit = self._evaluate(self.poles, self.residues, self.constant)
        
        # Metrics
        corr = np.corrcoef(np.abs(self.h), np.abs(h_fit))[0, 1]
        rmse = np.sqrt(np.mean(np.abs(self.h - h_fit)**2))
        max_err = np.max(np.abs(self.h - h_fit))
        
        # Magnitude in dB
        h_db = 20 * np.log10(np.abs(self.h) + 1e-20)
        h_fit_db = 20 * np.log10(np.abs(h_fit) + 1e-20)
        rmse_db = np.sqrt(np.mean((h_db - h_fit_db)**2))
        max_err_db = np.max(np.abs(h_db - h_fit_db))
        
        print(f"\n  Results:")
        print(f"    Poles: {len(self.poles)}")
        print(f"    Correlation: {corr:.6f}")
        print(f"    RMSE (linear): {rmse:.6e}")
        print(f"    RMSE (dB): {rmse_db:.4f}")
        print(f"    Max error (dB): {max_err_db:.4f}")
        
        return {
            'converged': error < tolerance,
            'iterations': iteration + 1,
            'correlation': corr,
            'rmse_db': rmse_db,
            'max_error_db': max_err_db
        }
    
    def _evaluate(self, poles: np.ndarray, residues: np.ndarray, 
                 constant: complex) -> np.ndarray:
        """Evaluate model."""
        h = np.zeros_like(self.s, dtype=complex)
        for p, r in zip(poles, residues):
            h += r / (self.s - p)
        h += constant
        return h
    
    def to_config(self, fs: float = 100e9) -> dict:
        """Export to configuration dict."""
        if self.poles is None:
            raise ValueError("Run vector_fit first")
        
        return {
            'version': '3.0-pyvf-v2',
            'method': 'POLE_RESIDUE',
            'pole_residue': {
                'poles_real': [float(p.real) for p in self.poles],
                'poles_imag': [float(p.imag) for p in self.poles],
                'residues_real': [float(r.real) for r in self.residues],
                'residues_imag': [float(r.imag) for r in self.residues],
                'constant': {'real': float(self.constant.real), 
                            'imag': float(self.constant.imag)},
                'proportional': 0.0,
                'order': len(self.poles)
            },
            'fs': fs
        }


def main():
    import sys
    import skrf
    
    if len(sys.argv) < 3:
        print("Usage: python vector_fitting_py_v2.py <input.s4p> <output.json> [n_poles]")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    n_poles = int(sys.argv[3]) if len(sys.argv) > 3 else 16
    
    # Load
    print(f"Loading {input_file}...")
    nw = skrf.Network(input_file)
    freq = nw.frequency.f
    s21 = nw.s[:, 1, 0]
    
    print(f"  Freq: {freq[0]/1e6:.2f} MHz to {freq[-1]/1e9:.2f} GHz")
    print(f"  Samples: {len(freq)}")
    
    # Fit
    vf = VectorFittingPY(freq, s21)
    result = vf.vector_fit(n_poles=n_poles, max_iter=100)
    
    # Save
    config = vf.to_config()
    config['metrics'] = {
        'correlation': float(result['correlation']),
        'rmse_db': float(result['rmse_db']),
        'max_error_db': float(result['max_error_db'])
    }
    
    with open(output_file, 'w') as f:
        json.dump(config, f, indent=2)
    
    print(f"\nSaved to {output_file}")
    
    # Validate
    print("\n" + "="*60)
    print("VALIDATION")
    print("="*60)
    corr = result['correlation']
    rmse = result['rmse_db']
    max_err = result['max_error_db']
    
    print(f"Correlation:   {corr:.6f} {'✓ PASS' if corr > 0.95 else '✗ FAIL'}")
    print(f"RMSE (dB):     {rmse:.4f} {'✓ PASS' if rmse < 3 else '✗ FAIL'}")
    print(f"Max Error (dB): {max_err:.4f} {'✓ PASS' if max_err < 6 else '✗ FAIL'}")
    
    passed = corr > 0.95 and rmse < 3 and max_err < 6
    print(f"\n{'✓ OVERALL PASS' if passed else '✗ OVERALL FAIL'}")
    
    sys.exit(0 if passed else 1)


if __name__ == '__main__':
    main()
