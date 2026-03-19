#!/usr/bin/env python3
"""
Simplified VectorFitting in Pure Python
========================================

A clean, working implementation of Relaxed VectorFitting.

Algorithm based on:
B. Gustavsen, "Improving the Pole Relocating Properties of Vector Fitting"
IEEE Trans. Power Delivery, 2006
"""

import numpy as np
import json
import warnings


class VectorFittingSimple:
    """
    Simplified VectorFitting for S-parameter modeling.
    """
    
    def __init__(self, freq, h):
        self.freq = np.asarray(freq)
        self.h = np.asarray(h)
        self.omega = 2 * np.pi * self.freq
        self.s = 1j * self.omega
        
        self.poles = None
        self.residues = None
        self.constant = None
    
    def _init_poles(self, n_cmplx):
        """Initialize poles with log spacing."""
        fmin = max(self.freq[self.freq > 0].min(), 1e6)
        fmax = self.freq.max()
        
        omega = 2 * np.pi * np.geomspace(fmin, fmax, n_cmplx)
        
        poles = []
        for w in omega:
            poles.append(-0.01 * w + 1j * w)
        
        return np.array(poles, dtype=complex)
    
    def _evaluate(self, poles, residues, constant):
        """Evaluate H(s)."""
        h = np.zeros_like(self.s, dtype=complex)
        for p, r in zip(poles, residues):
            h += r / (self.s - p)
        h += constant
        return h
    
    def fit(self, n_poles=16, max_iter=100, tol=1e-6):
        """
        Main fitting routine.
        
        Uses a simplified approach:
        1. Initialize poles
        2. For each iteration:
           a. Build and solve the linear system for sigma and H_tilde
           b. Find zeros of sigma (new poles)
           c. Calculate residues at new poles
        """
        print(f"VectorFittingSimple: {n_poles} poles")
        
        # Initialize
        poles = self._init_poles(n_poles // 2)
        print(f"  Started with {len(poles)} complex poles")
        
        N = len(self.freq)
        
        for iteration in range(max_iter):
            # Build system matrix for relaxed VF
            # [A] * x = b
            # where x = [residues of H_tilde; residues of sigma]
            
            n_p = len(poles)
            
            # For each complex pole, we have:
            # - Real and imag parts of residue for H_tilde
            # - Real and imag parts of residue for sigma
            # Plus constant terms for both
            n_cols = 4 * n_p + 4
            
            A = np.zeros((2 * N, n_cols))
            
            col = 0
            # H_tilde residues
            h_cols_re = []
            h_cols_im = []
            for i, p in enumerate(poles):
                # 1/(s-p) + 1/(s-p*)
                phi_re = 1.0 / (self.s - p) + 1.0 / (self.s - np.conj(p))
                phi_im = 1j / (self.s - p) - 1j / (self.s - np.conj(p))
                
                h_cols_re.append(col)
                A[:N, col] = phi_re.real
                A[N:, col] = phi_re.imag
                col += 1
                
                h_cols_im.append(col)
                A[:N, col] = phi_im.real
                A[N:, col] = phi_im.imag
                col += 1
            
            # Sigma residues (weighted by -H)
            s_cols_re = []
            s_cols_im = []
            for i, p in enumerate(poles):
                phi_re = 1.0 / (self.s - p) + 1.0 / (self.s - np.conj(p))
                phi_im = 1j / (self.s - p) - 1j / (self.s - np.conj(p))
                
                s_cols_re.append(col)
                A[:N, col] = -(phi_re * self.h).real
                A[N:, col] = -(phi_re * self.h).imag
                col += 1
                
                s_cols_im.append(col)
                A[:N, col] = -(phi_im * self.h).real
                A[N:, col] = -(phi_im * self.h).imag
                col += 1
            
            # Constant terms
            h_c_col = col
            A[:N, col] = 1.0
            A[N:, col] = 0.0
            col += 1
            
            A[:N, col] = 0.0
            A[N:, col] = 1.0
            col += 1
            
            s_c_col = col
            A[:N, col] = -self.h.real
            A[N:, col] = -self.h.imag
            col += 1
            
            A[:N, col] = self.h.imag
            A[N:, col] = -self.h.real
            col += 1
            
            # RHS
            b = np.zeros(2 * N)
            b[:N] = self.h.real
            b[N:] = self.h.imag
            
            # Solve
            x, _, _, _ = np.linalg.lstsq(A, b, rcond=None)
            
            # Extract sigma residues
            c_re = [x[s_cols_re[i]] for i in range(n_p)]
            c_im = [x[s_cols_im[i]] for i in range(n_p)]
            c_const = x[s_c_col] + 1j * x[s_c_col + 1]
            
            # Build companion matrix for sigma zeros
            # For each complex pole pair, we have a 2x2 block
            M_size = 2 * n_p
            M = np.zeros((M_size, M_size))
            
            for i, p in enumerate(poles):
                idx = 2 * i
                
                # Block: [alpha, beta; -beta, alpha]
                # Modified by sigma residues
                alpha = p.real - c_re[i]
                beta = p.imag - c_im[i]
                
                M[idx, idx] = alpha
                M[idx, idx + 1] = beta
                M[idx + 1, idx] = -beta
                M[idx + 1, idx + 1] = alpha
            
            # Eigenvalues are new poles
            eigs = np.linalg.eigvals(M)
            
            # Process eigenvalues
            new_poles = []
            for ev in eigs:
                # Make stable
                alpha = -abs(ev.real) if ev.real > 0 else ev.real
                beta = abs(ev.imag)
                
                if beta > 1e-12 and ev.imag > 0:
                    new_poles.append(alpha + 1j * beta)
            
            if len(new_poles) < n_p // 2:
                # Not enough poles, keep old ones
                print(f"  Warning: Only {len(new_poles)} poles found, keeping old")
                new_poles = poles
            
            # Calculate error
            residues, constant = self._calc_residues(poles)
            h_fit = self._evaluate(poles, residues, constant)
            error = np.sqrt(np.mean(np.abs(self.h - h_fit)**2))
            
            # Check convergence
            if iteration % 10 == 0:
                print(f"  Iter {iteration}: error = {error:.6e}, poles = {len(poles)}")
            
            if error < tol:
                print(f"  Converged at iteration {iteration}")
                break
            
            # Accept new poles (limit to requested number)
            poles = np.array(new_poles[:n_p // 2] if len(new_poles) > n_p // 2 else new_poles)
        
        # Final calculation
        print("\n  Finalizing...")
        self.poles = poles
        self.residues, self.constant = self._calc_residues(poles)
        
        # Evaluate
        h_fit = self._evaluate(self.poles, self.residues, self.constant)
        
        # Metrics
        corr = np.corrcoef(np.abs(self.h), np.abs(h_fit))[0, 1]
        h_db = 20 * np.log10(np.abs(self.h) + 1e-20)
        h_fit_db = 20 * np.log10(np.abs(h_fit) + 1e-20)
        rmse_db = np.sqrt(np.mean((h_db - h_fit_db)**2))
        max_err_db = np.max(np.abs(h_db - h_fit_db))
        
        print(f"\n  Results:")
        print(f"    Correlation: {corr:.6f}")
        print(f"    RMSE (dB): {rmse_db:.4f}")
        print(f"    Max Error (dB): {max_err_db:.4f}")
        
        return {
            'correlation': corr,
            'rmse_db': rmse_db,
            'max_error_db': max_err_db
        }
    
    def _calc_residues(self, poles):
        """Calculate residues for fixed poles."""
        N = len(self.freq)
        n_p = len(poles)
        
        n_cols = 2 * n_p + 2
        A = np.zeros((2 * N, n_cols))
        
        col = 0
        for i, p in enumerate(poles):
            phi_re = 1.0 / (self.s - p) + 1.0 / (self.s - np.conj(p))
            phi_im = 1j / (self.s - p) - 1j / (self.s - np.conj(p))
            
            A[:N, col] = phi_re.real
            A[N:, col] = phi_re.imag
            col += 1
            
            A[:N, col] = phi_im.real
            A[N:, col] = phi_im.imag
            col += 1
        
        # Constant
        A[:N, col] = 1.0
        A[N:, col] = 0.0
        col += 1
        
        A[:N, col] = 0.0
        A[N:, col] = 1.0
        col += 1
        
        b = np.zeros(2 * N)
        b[:N] = self.h.real
        b[N:] = self.h.imag
        
        x, _, _, _ = np.linalg.lstsq(A, b, rcond=None)
        
        residues = np.array([x[2*i] + 1j * x[2*i+1] for i in range(n_p)])
        constant = x[-2] + 1j * x[-1]
        
        return residues, constant
    
    def to_config(self, fs=100e9):
        """Export to config."""
        return {
            'version': '3.0-pyvf-simple',
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
        print("Usage: python vector_fitting_simple.py <input.s4p> <output.json> [n_poles]")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    n_poles = int(sys.argv[3]) if len(sys.argv) > 3 else 16
    
    # Load
    print(f"Loading {input_file}...")
    nw = skrf.Network(input_file)
    freq = nw.frequency.f
    s21 = nw.s[:, 1, 0]
    
    print(f"  {len(freq)} samples, {freq[0]/1e6:.2f} MHz to {freq[-1]/1e9:.2f} GHz")
    
    # Fit
    vf = VectorFittingSimple(freq, s21)
    result = vf.fit(n_poles=n_poles)
    
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
    print(f"Correlation: {result['correlation']:.6f} {'✓' if result['correlation'] > 0.95 else '✗'}")
    print(f"RMSE (dB): {result['rmse_db']:.4f} {'✓' if result['rmse_db'] < 3 else '✗'}")
    print(f"Max Error (dB): {result['max_error_db']:.4f} {'✓' if result['max_error_db'] < 6 else '✗'}")
    
    passed = result['correlation'] > 0.95 and result['rmse_db'] < 3
    print(f"\n{'✓ PASS' if passed else '✗ FAIL'}")
    
    sys.exit(0 if passed else 1)


if __name__ == '__main__':
    main()
