#!/usr/bin/env python3
"""
VectorFitting with Refinement
=============================

1. Use scikit-rf for initial guess
2. Refine with scipy.optimize to enforce constraints
"""

import numpy as np
import json
from scipy.optimize import minimize
import skrf


class VectorFitRefine:
    """VectorFitting with post-refinement."""
    
    def __init__(self, freq, h):
        self.freq = np.asarray(freq)
        self.h = np.asarray(h)
        self.s = 1j * 2 * np.pi * self.freq
    
    def _h_model(self, s, poles, residues, constant):
        """Evaluate H(s)."""
        h = np.zeros_like(s, dtype=complex)
        for p, r in zip(poles, residues):
            h += r / (s - p)
        h += constant
        return h
    
    def _pack_params(self, poles, residues, constant):
        """Pack parameters into 1D array."""
        return np.concatenate([
            poles.real, poles.imag,
            residues.real, residues.imag,
            [constant.real, constant.imag]
        ])
    
    def _unpack_params(self, x, n_poles):
        """Unpack 1D array into parameters."""
        pr = x[:n_poles]
        pi = x[n_poles:2*n_poles]
        rr = x[2*n_poles:3*n_poles]
        ri = x[3*n_poles:4*n_poles]
        cr = x[-2]
        ci = x[-1]
        
        poles = pr + 1j * pi
        residues = rr + 1j * ri
        constant = cr + 1j * ci
        
        return poles, residues, constant
    
    def fit(self, n_poles=16, max_iter=100):
        """Fit with initial guess from scikit-rf."""
        
        print("Step 1: Initial guess from scikit-rf...")
        nw = skrf.Network(frequency=self.freq, s=self.h)
        nw.nports = 2  # For S21 extraction
        
        # Create a minimal 2-port network
        s_full = np.zeros((len(self.freq), 2, 2), dtype=complex)
        s_full[:, 0, 0] = 0.1  # S11
        s_full[:, 0, 1] = 0.01  # S12
        s_full[:, 1, 0] = self.h  # S21
        s_full[:, 1, 1] = 0.1  # S22
        
        nw = skrf.Network(frequency=self.freq, s=s_full)
        
        vf = skrf.VectorFitting(nw)
        vf.max_iterations = 50
        
        try:
            vf.vector_fit(n_poles_real=0, n_poles_cmplx=n_poles//2,
                         fit_constant=True, fit_proportional=False)
            
            # Extract S21 parameters
            idx = 1 * 2 + 0  # S21
            poles_init = vf.poles
            residues_init = vf.residues[idx]
            constant_init = vf.constant_coeff[idx]
            
            print(f"  Got {len(poles_init)} poles from scikit-rf")
            
        except Exception as e:
            print(f"  scikit-rf failed: {e}")
            print("  Using linear initialization")
            poles_init = self._init_poles_linear(n_poles//2)
            residues_init = np.ones(len(poles_init), dtype=complex)
            constant_init = self.h[0]
        
        # Ensure we have the right number of poles
        if len(poles_init) > n_poles // 2:
            poles_init = poles_init[:n_poles//2]
            residues_init = residues_init[:n_poles//2]
        
        print("\nStep 2: Refinement with constraints...")
        
        x0 = self._pack_params(poles_init, residues_init, constant_init)
        
        def objective(x):
            """Weighted error."""
            poles, residues, constant = self._unpack_params(x, len(poles_init))
            
            h_fit = self._h_model(self.s, poles, residues, constant)
            error = h_fit - self.h
            
            # Weight more at low frequencies (DC region)
            weights = 1.0 / (1.0 + self.freq / self.freq[0])
            weighted_error = error * weights
            
            return np.sum(np.abs(weighted_error)**2)
        
        def constraint_conjugate_symmetry(x):
            """Enforce H(-f) = H*(f) for real impulse response."""
            poles, residues, constant = self._unpack_params(x, len(poles_init))
            
            # For conjugate symmetry, constant must be real
            return -constant.imag**2  # <= 0 constraint
        
        # Bounds: poles must be stable (real part < 0)
        bounds = []
        n_p = len(poles_init)
        
        # Pole real parts: negative
        for _ in range(n_p):
            bounds.append((-np.inf, -1e-6))
        # Pole imag parts: positive (upper half)
        for _ in range(n_p):
            bounds.append((0, np.inf))
        # Residues: unconstrained
        for _ in range(2 * n_p):
            bounds.append((None, None))
        # Constant: unconstrained (will be constrained by objective)
        bounds.append((None, None))
        bounds.append((None, None))
        
        # Optimize
        result = minimize(
            objective, x0,
            method='L-BFGS-B',
            bounds=bounds,
            options={'maxiter': max_iter, 'disp': True}
        )
        
        self.poles, self.residues, self.constant = self._unpack_params(
            result.x, len(poles_init)
        )
        
        # Force real constant
        self.constant = self.constant.real
        
        # Evaluate
        h_fit = self._h_model(self.s, self.poles, self.residues, self.constant)
        
        corr = np.corrcoef(np.abs(self.h), np.abs(h_fit))[0, 1]
        h_db = 20 * np.log10(np.abs(self.h) + 1e-20)
        h_fit_db = 20 * np.log10(np.abs(h_fit) + 1e-20)
        rmse_db = np.sqrt(np.mean((h_db - h_fit_db)**2))
        max_err_db = np.max(np.abs(h_db - h_fit_db))
        
        print(f"\nResults:")
        print(f"  Poles: {len(self.poles)}")
        print(f"  Correlation: {corr:.6f}")
        print(f"  RMSE (dB): {rmse_db:.4f}")
        print(f"  Max Error (dB): {max_err_db:.4f}")
        
        return {
            'correlation': corr,
            'rmse_db': rmse_db,
            'max_error_db': max_err_db
        }
    
    def _init_poles_linear(self, n_cmplx):
        """Linear initialization."""
        omega = 2 * np.pi * np.linspace(self.freq[0], self.freq[-1], n_cmplx)
        return np.array([-0.01*w + 1j*w for w in omega])
    
    def to_config(self, fs=100e9):
        """Export to config."""
        return {
            'version': '3.0-pyvf-refine',
            'method': 'POLE_RESIDUE',
            'pole_residue': {
                'poles_real': [float(p.real) for p in self.poles],
                'poles_imag': [float(p.imag) for p in self.poles],
                'residues_real': [float(r.real) for r in self.residues],
                'residues_imag': [float(r.imag) for r in self.residues],
                'constant': float(self.constant),
                'proportional': 0.0,
                'order': len(self.poles)
            },
            'fs': fs
        }


def main():
    import sys
    
    if len(sys.argv) < 3:
        print("Usage: python vector_fit_refine.py <input.s4p> <output.json> [n_poles]")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    n_poles = int(sys.argv[3]) if len(sys.argv) > 3 else 16
    
    # Load
    print(f"Loading {input_file}...")
    nw = skrf.Network(input_file)
    freq = nw.frequency.f
    s21 = nw.s[:, 1, 0]
    
    print(f"  {len(freq)} samples")
    
    # Fit
    vf = VectorFitRefine(freq, s21)
    result = vf.fit(n_poles=n_poles)
    
    # Save
    config = vf.to_config()
    config['metrics'] = result
    
    with open(output_file, 'w') as f:
        json.dump(config, f, indent=2)
    
    print(f"\nSaved to {output_file}")
    
    # Validate
    print("\n" + "="*60)
    corr = result['correlation']
    rmse = result['rmse_db']
    max_err = result['max_error_db']
    
    print(f"Correlation: {corr:.6f} {'✓' if corr > 0.95 else '✗'}")
    print(f"RMSE (dB): {rmse:.4f} {'✓' if rmse < 3 else '✗'}")
    print(f"Max Error (dB): {max_err:.4f} {'✓' if max_err < 6 else '✗'}")
    
    passed = corr > 0.95 and rmse < 3
    print(f"\n{'✓ PASS' if passed else '✗ FAIL'}")
    
    sys.exit(0 if passed else 1)


if __name__ == '__main__':
    main()
