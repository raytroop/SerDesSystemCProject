"""
Vector Fitting algorithm implementation (Relaxed VF) for S-parameter fitting.

Based on vectfit3.m by Bjorn Gustavsen (SINTEF Energy Research).
This is a clean implementation of the Fast Relaxed Vector Fitting algorithm.
"""

import numpy as np
from typing import Dict, List, Tuple, Optional
import logging

logger = logging.getLogger(__name__)


class VectorFitting:
    """
    Vector Fitting algorithm with relaxed pole identification.
    
    The algorithm approximates frequency response H(s) as:
    H(s) = sum(r_k / (s - p_k)) + d + s*e
    
    using the relaxed Vector Fitting method which ensures better
    convergence by avoiding the trivial solution.
    """
    
    def __init__(self, order: int = 8, max_iterations: int = 10, 
                 tolerance: float = 1e-6, relax: bool = True,
                 stable: bool = True, asymp: int = 2):
        """
        Initialize Vector Fitting.
        
        Args:
            order: Number of poles (fitting order)
            max_iterations: Maximum VF iterations
            tolerance: Convergence tolerance for poles
            relax: Use relaxed nontriviality constraint
            stable: Force unstable poles to left half-plane
            asymp: Asymptotic behavior (1=D=0,E=0; 2=D!=0,E=0; 3=D!=0,E!=0)
        """
        self.order = order
        self.max_iterations = max_iterations
        self.tolerance = tolerance
        self.relax = relax
        self.stable = stable
        self.asymp = asymp
        
        # Results
        self.poles = None
        self.residues = None
        self.d = 0.0
        self.e = 0.0
        
        # Normalization
        self._s_scale = 1.0
        
    def _initialize_poles(self, s: np.ndarray) -> np.ndarray:
        """
        Initialize starting poles distributed across frequency range.
        
        Based on vectfit3 ex4a.m: linearly spaced complex conjugate pairs
        with small damping (1% of frequency).
        
        Args:
            s: Complex frequency array (rad/s)
            
        Returns:
            Initial pole array (complex)
        """
        s_min = np.min(np.abs(s))
        s_max = np.max(np.abs(s))
        
        # Store normalization factor
        self._s_scale = s_max
        
        # Create complex conjugate pairs (all poles are complex pairs)
        n_pairs = self.order // 2
        
        poles = []
        
        if n_pairs > 0:
            # LINEARLY spaced frequencies (like ex4a.m), not log-spaced
            # bet = linspace(w(1), w(Ns), N/2)
            omega_min = s_min
            omega_max = s_max
            
            omegas = np.linspace(omega_min, omega_max, n_pairs)
            
            for omega in omegas:
                # alf = -bet * 1e-2  (1% damping like ex4a.m)
                sigma = -omega * 0.01
                poles.append(sigma + 1j * omega)
                poles.append(sigma - 1j * omega)
        
        # If odd order, add one real pole at the middle
        if self.order % 2 == 1:
            omega_mid = (s_min + s_max) / 2
            poles.append(-omega_mid * 0.01)
        
        return np.array(poles, dtype=complex)
    
    def _build_cindex(self, poles: np.ndarray) -> np.ndarray:
        """
        Build cindex array to identify complex conjugate pairs.
        
        Args:
            poles: Pole array
            
        Returns:
            cindex: 0=real, 1=complex first, 2=complex second
        """
        N = len(poles)
        cindex = np.zeros(N, dtype=int)
        
        for m in range(N):
            if np.imag(poles[m]) != 0:
                if m == 0:
                    cindex[m] = 1
                else:
                    if cindex[m-1] == 0 or cindex[m-1] == 2:
                        cindex[m] = 1
                        if m + 1 < N:
                            cindex[m+1] = 2
                    else:
                        cindex[m] = 2
        
        return cindex
    
    def _build_Dk(self, s: np.ndarray, poles: np.ndarray, 
                  cindex: np.ndarray) -> np.ndarray:
        """
        Build Dk matrix for pole identification.
        
        Dk contains basis functions: 1/(s - p_k) for real poles,
        and separated real/imaginary parts for complex poles.
        
        Args:
            s: Complex frequency array
            poles: Current poles
            cindex: Complex index array
            
        Returns:
            Dk matrix [Ns, N+offs] where offs depends on asymp setting
        """
        Ns = len(s)
        N = len(poles)
        
        # Determine offset for asymptotic terms
        if self.asymp == 1:
            offs = 0
        elif self.asymp == 2:
            offs = 1  # D term
        else:  # asymp == 3
            offs = 2  # D + E*s terms
        
        Dk = np.zeros((Ns, N + offs), dtype=complex)
        
        # Build pole basis functions
        for m in range(N):
            if cindex[m] == 0:  # Real pole
                Dk[:, m] = 1.0 / (s - poles[m])
            elif cindex[m] == 1:  # Complex pole, first part
                Dk[:, m] = 1.0 / (s - poles[m]) + 1.0 / (s - np.conj(poles[m]))
                Dk[:, m+1] = 1j / (s - poles[m]) - 1j / (s - np.conj(poles[m]))
            # cindex[m] == 2 is handled with m+1 above
        
        # Add asymptotic terms
        if self.asymp == 2:
            Dk[:, N] = 1.0
        elif self.asymp == 3:
            Dk[:, N] = 1.0
            Dk[:, N+1] = s
        
        return Dk
    
    def _pole_identification(self, s: np.ndarray, H: np.ndarray,
                            poles: np.ndarray, weight: np.ndarray,
                            opts: Dict) -> np.ndarray:
        """
        Pole identification using relaxed Vector Fitting.
        
        This is the core pole relocation step. It solves for sigma(s)
        such that sigma(s)*H(s) is approximated by a rational function
        with the same poles. The zeros of sigma become the new poles.
        
        Algorithm:
        1. Build Dk matrix using current poles
        2. Build linear system with relaxed nontriviality constraint
        3. QR decomposition
        4. Solve for sigma coefficients
        5. Compute zeros of sigma (new poles)
        6. Stabilize if requested
        
        Args:
            s: Complex frequency array (rad/s)
            H: Frequency response (Nc, Ns)
            poles: Current poles
            weight: Weight array
            opts: Options dict
            
        Returns:
            new_poles: Relocated poles
        """
        Ns = len(s)
        N = len(poles)
        Nc = H.shape[0] if H.ndim > 1 else 1
        
        if H.ndim == 1:
            H = H.reshape(1, -1)
        
        # Build cindex
        cindex = self._build_cindex(poles)
        
        # Build Dk matrix
        Dk = self._build_Dk(s, poles, cindex)
        
        # Determine offset
        if self.asymp == 1:
            offs = 0
        elif self.asymp == 2:
            offs = 1
        else:
            offs = 2
        
        # Common weight flag
        common_weight = weight.ndim == 1 or weight.shape[0] == 1
        
        # Scale for relaxation (from vectfit3.m line 304-312)
        scale = 0.0
        for m in range(Nc):
            if common_weight:
                scale += np.linalg.norm(weight * H[m, :])**2
            else:
                scale += np.linalg.norm(weight[m, :] * H[m, :])**2
        scale = np.sqrt(scale) / Ns
        
        # Tolerance thresholds
        TOLlow = 1e-18
        TOLhigh = 1e18
        
        # Relaxed pole identification
        if opts.get('relax', True):
            AA = np.zeros((Nc * (N + 1), N + 1))
            bb = np.zeros(Nc * (N + 1))
            
            for n in range(Nc):
                # Build system matrix A
                A = np.zeros((Ns, (N + offs) + (N + 1)), dtype=complex)
                
                # Select weight
                if common_weight:
                    weig = weight
                else:
                    weig = weight[n, :] if weight.ndim > 1 else weight
                
                # Left block: Dk for H fitting
                for m in range(N + offs):
                    A[:, m] = weig * Dk[:, m]
                
                # Right block: -H*Dk for sigma fitting
                inda = N + offs
                for m in range(N + 1):
                    A[:, inda + m] = -weig * Dk[:, m] * H[n, :]
                
                # Stack real and imaginary parts
                A_real = np.vstack([A.real, A.imag])
                
                # Add relaxation constraint (integral criterion for sigma)
                # This ensures sigma is not identically zero
                if n == Nc - 1:
                    offset_row = 2 * Ns
                    A_real = np.vstack([A_real, np.zeros(N + offs + N + 1)])
                    for mm in range(N + 1):
                        A_real[offset_row, inda + mm] = scale * np.sum(Dk[:, mm].real)
                
                # QR decomposition
                Q, R = np.linalg.qr(A_real, mode='reduced')
                
                # Extract R22 block (corresponding to sigma coefficients)
                ind1 = N + offs
                ind2 = N + offs + N + 1
                R22 = R[ind1:ind2, ind1:ind2]
                
                AA[n*(N+1):(n+1)*(N+1), :] = R22
                
                # Right-hand side from relaxation
                if n == Nc - 1:
                    bb[n*(N+1):(n+1)*(N+1)] = Q[-1, ind1:ind2] * Ns * scale
            
            # Column scaling for better conditioning
            Escale = np.zeros(AA.shape[1])
            for col in range(AA.shape[1]):
                col_norm = np.linalg.norm(AA[:, col])
                if col_norm > 1e-15:
                    Escale[col] = 1.0 / col_norm
                    AA[:, col] *= Escale[col]
            
            # Solve least squares
            x = np.linalg.lstsq(AA, bb, rcond=None)[0]
            x = x * Escale
        else:
            x = np.zeros(N + 1)
            x[-1] = 1.0  # D = 1
        
        # Check if relaxation produced bad D value
        Dnew = x[-1]
        if not opts.get('relax', True) or abs(Dnew) < TOLlow or abs(Dnew) > TOLhigh:
            # Solve without relaxation, fixing D
            if not opts.get('relax', True):
                Dnew = 1.0
            elif abs(Dnew) < TOLlow:
                Dnew = np.sign(Dnew) * TOLlow if Dnew != 0 else TOLlow
            elif abs(Dnew) > TOLhigh:
                Dnew = np.sign(Dnew) * TOLhigh
            
            AA = np.zeros((Nc * N, N))
            bb = np.zeros(Nc * N)
            
            for n in range(Nc):
                A = np.zeros((Ns, (N + offs) + N), dtype=complex)
                
                if common_weight:
                    weig = weight
                else:
                    weig = weight[n, :] if weight.ndim > 1 else weight
                
                # Left block
                for m in range(N + offs):
                    A[:, m] = weig * Dk[:, m]
                
                # Right block (only N columns now, excluding D)
                inda = N + offs
                for m in range(N):
                    A[:, inda + m] = -weig * Dk[:, m] * H[n, :]
                
                b = Dnew * weig * H[n, :]
                
                # Stack real and imaginary
                A_real = np.vstack([A.real, A.imag])
                b_real = np.hstack([b.real, b.imag])
                
                # QR decomposition
                Q, R = np.linalg.qr(A_real, mode='reduced')
                
                ind1 = N + offs
                ind2 = N + offs + N
                R22 = R[ind1:ind2, ind1:ind2]
                AA[n*N:(n+1)*N, :] = R22
                bb[n*N:(n+1)*N] = Q[:, ind1:ind2].T @ b_real
            
            # Column scaling
            Escale = np.zeros(AA.shape[1])
            for col in range(AA.shape[1]):
                col_norm = np.linalg.norm(AA[:, col])
                if col_norm > 1e-15:
                    Escale[col] = 1.0 / col_norm
                    AA[:, col] *= Escale[col]
            
            # Solve
            x = np.linalg.lstsq(AA, bb, rcond=None)[0]
            x = x * Escale
            x = np.append(x, Dnew)
        
        # Extract sigma coefficients
        C = x[:-1]  # Residues for sigma
        D = x[-1]   # Direct term for sigma
        
        # Convert C back to complex representation
        C_complex = np.zeros(N, dtype=complex)
        for m in range(N):
            if cindex[m] == 0:
                C_complex[m] = C[m]
            elif cindex[m] == 1:
                r1 = C[m]
                r2 = C[m+1]
                C_complex[m] = r1 + 1j * r2
                if m + 1 < N:
                    C_complex[m+1] = r1 - 1j * r2
            # cindex[m] == 2 is handled with m-1
        
        # Build state-space for sigma and compute zeros
        # Following vectfit3.m lines 484-498 exactly
        LAMBD = np.diag(poles).astype(complex)
        B = np.ones(N)
        C_sigma = C_complex.copy()
        
        # Convert to real state-space form for complex pairs
        m = 0
        while m < N:
            if m < N - 1 and cindex[m] == 1:
                # Complex conjugate pair - convert to 2x2 real block
                p_real = np.real(poles[m])
                p_imag = np.imag(poles[m])
                
                # LAMBD block: [p_real, p_imag; -p_imag, p_real]
                LAMBD[m, m] = p_real
                LAMBD[m, m+1] = p_imag
                LAMBD[m+1, m] = -p_imag
                LAMBD[m+1, m+1] = p_real
                
                # B: [2; 0]
                B[m] = 2
                B[m+1] = 0
                
                # C: [real(C), imag(C)]
                c_val = C_sigma[m]
                C_sigma[m] = np.real(c_val)
                C_sigma[m+1] = np.imag(c_val)
                
                m += 2
            else:
                m += 1
        
        # Compute zeros: ZER = LAMBD - B*C.T/D
        ZER = LAMBD - np.outer(B, C_sigma) / D
        
        # New poles are eigenvalues of ZER
        new_poles = np.linalg.eigvals(ZER)
        
        # Stabilize: move unstable poles to left half-plane
        if opts.get('stable', True):
            unstables = np.real(new_poles) > 0
            new_poles[unstables] -= 2 * np.real(new_poles[unstables])
        
        # Sort: real poles first, then complex pairs
        new_poles = self._sort_poles(new_poles)
        
        return new_poles
    
    def _sort_poles(self, poles: np.ndarray) -> np.ndarray:
        """
        Sort poles: real poles first, then complex by imaginary part.
        
        Args:
            poles: Unsorted poles
            
        Returns:
            Sorted poles
        """
        # Separate real and complex
        real_mask = np.abs(np.imag(poles)) < 1e-10 * (np.abs(poles) + 1e-10)
        real_poles = np.real(poles[real_mask])
        complex_poles = poles[~real_mask]
        
        # Sort real poles
        real_poles = np.sort(real_poles)
        
        # Sort complex poles by imaginary part
        if len(complex_poles) > 0:
            imag_parts = np.imag(complex_poles)
            idx = np.argsort(np.abs(imag_parts))
            complex_poles = complex_poles[idx]
        
        # Combine: real first, then complex
        result = np.concatenate([real_poles, complex_poles])
        
        return result.astype(complex)
    
    def fit(self, s: np.ndarray, H: np.ndarray, 
            weight: Optional[np.ndarray] = None) -> Dict:
        """
        Fit frequency response data using Vector Fitting.
        
        Args:
            s: Complex frequency array (rad/s)
            H: Frequency response (Ns,) or (Nc, Ns)
            weight: Optional weight array
            
        Returns:
            Dictionary with poles, residues, d, e, etc.
        """
        logger.info(f"Starting Vector Fitting with order={self.order}")
        
        # Ensure H is 2D
        if H.ndim == 1:
            H = H.reshape(1, -1)
        
        Ns = len(s)
        Nc = H.shape[0]
        
        # Default weights: 1/sqrt(|f|) like ex4a.m
        # weight=1./sqrt(abs(f)) balances accuracy across magnitude variations
        if weight is None:
            # Use inverse square root of magnitude for weighting
            abs_H = np.abs(H[0, :])
            weight = 1.0 / np.sqrt(np.maximum(abs_H, 1e-15))
        
        # Initialize poles
        self.poles = self._initialize_poles(s)
        logger.debug(f"Initial poles: {self.poles}")
        
        # Build options dict
        opts = {
            'relax': self.relax,
            'stable': self.stable,
            'asymp': self.asymp
        }
        
        # Iterative pole relocation
        for iteration in range(self.max_iterations):
            # Pole identification
            new_poles = self._pole_identification(s, H, self.poles, weight, opts)
            
            # Check convergence
            pole_change = np.max(np.abs(new_poles - self.poles))
            rel_change = pole_change / (np.max(np.abs(self.poles)) + 1e-10)
            
            logger.debug(f"Iteration {iteration + 1}: rel_change = {rel_change:.2e}")
            
            self.poles = new_poles
            
            if rel_change < self.tolerance:
                logger.info(f"Converged after {iteration + 1} iterations")
                break
        
        # Final residue identification (to be implemented)
        # For now, use simple least squares
        self.residues, self.d, self.e = self._solve_residues(s, H[0, :])
        
        return {
            'poles': self.poles,
            'residues': self.residues,
            'd': self.d,
            'e': self.e,
            'order': self.order
        }
    
    def _solve_residues(self, s: np.ndarray, H: np.ndarray) -> Tuple[np.ndarray, float, float]:
        """
        Solve for residues given fixed poles using real basis functions.
        
        Based on vectfit3.m residue identification (lines 577-712):
        - For complex pole pairs, use basis: 1/(s-p)+1/(s-p*) and j/(s-p)-j/(s-p*)
        - Apply column scaling to fix ill-conditioning
        - Convert back to complex residues
        
        Args:
            s: Complex frequency array
            H: Frequency response
            
        Returns:
            residues (complex), d, e
        """
        N = len(self.poles)
        Ns = len(s)
        
        # Build cindex to identify complex conjugate pairs
        cindex = self._build_cindex(self.poles)
        
        # Build system matrix with REAL basis functions (vectfit3.m style)
        # For real poles: 1/(s-p)
        # For complex pairs: 1/(s-p)+1/(s-p*) and j/(s-p)-j/(s-p*)
        A = np.zeros((Ns, N + 2), dtype=complex)
        
        for m in range(N):
            if cindex[m] == 0:  # Real pole
                A[:, m] = 1.0 / (s - self.poles[m])
            elif cindex[m] == 1:  # Complex pole, first part
                p = self.poles[m]
                p_conj = np.conj(p)
                A[:, m] = 1.0 / (s - p) + 1.0 / (s - p_conj)
                A[:, m + 1] = 1j / (s - p) - 1j / (s - p_conj)
            # cindex[m] == 2 is filled by m-1
        
        # Add d and e terms
        A[:, N] = 1.0      # d term
        A[:, N + 1] = s    # e term (proportional)
        
        # Stack real and imaginary parts
        A_real = np.vstack([A.real, A.imag])
        b_real = np.hstack([H.real, H.imag])
        
        # Column scaling to fix ill-conditioning (vectfit3.m lines 624-628)
        Escale = np.linalg.norm(A_real, axis=0)
        Escale[Escale < 1e-15] = 1.0
        A_scaled = A_real / Escale
        
        # Solve least squares
        x = np.linalg.lstsq(A_scaled, b_real, rcond=None)[0]
        
        # Undo scaling
        x = x / Escale
        
        # Extract and convert residues back to complex (vectfit3.m lines 705-712)
        residues = np.zeros(N, dtype=complex)
        for m in range(N):
            if cindex[m] == 0:  # Real pole -> real residue
                residues[m] = x[m]
            elif cindex[m] == 1:  # Complex pair
                r1, r2 = x[m], x[m + 1]
                residues[m] = r1 + 1j * r2
                residues[m + 1] = r1 - 1j * r2  # Conjugate
        
        d = x[N]
        e = x[N + 1]
        
        return residues, d, e


    def to_state_space(self) -> Dict:
        """
        Convert pole-residue representation to real state-space form.
        
        State-space form:
            x_dot = A*x + B*u
            y = C*x + D*u + E*u_dot
            
        Complex conjugate pole pairs are converted to real 2x2 blocks.
        
        Returns:
            Dictionary with A, B, C, D, E matrices (all real)
        """
        if self.poles is None or self.residues is None:
            raise RuntimeError("Must call fit() before to_state_space()")
        
        n = len(self.poles)
        
        # Build real state-space matrices
        # For complex conjugate pairs, use 2x2 real blocks
        A_blocks = []
        B_blocks = []
        C_blocks = []
        
        i = 0
        state_idx = 0
        while i < n:
            p = self.poles[i]
            r = self.residues[i]
            
            if np.abs(p.imag) > 1e-12:
                # Complex conjugate pair
                # Find conjugate
                if i + 1 < n and np.abs(self.poles[i+1] - p.conj()) < 1e-6:
                    p_conj = self.poles[i+1]
                    r_conj = self.residues[i+1]
                else:
                    # Find closest conjugate
                    p_conj = p.conj()
                    r_conj = r.conj()
                
                sigma = p.real
                omega = p.imag
                alpha = r.real
                beta = r.imag
                
                # 2x2 real block for A
                A_block = [[sigma, omega], [-omega, sigma]]
                B_block = [[2.0], [0.0]]
                C_block = [[alpha, -beta]]
                
                A_blocks.append(A_block)
                B_blocks.append(B_block)
                C_blocks.append(C_block)
                
                i += 2
                state_idx += 2
            else:
                # Real pole
                A_block = [[p.real]]
                B_block = [[1.0]]
                C_block = [[r.real]]
                
                A_blocks.append(A_block)
                B_blocks.append(B_block)
                C_blocks.append(C_block)
                
                i += 1
                state_idx += 1
        
        # Assemble full matrices
        n_states = sum(len(b) for b in A_blocks)
        
        A = np.zeros((n_states, n_states))
        B = np.zeros((n_states, 1))
        C = np.zeros((1, n_states))
        
        idx = 0
        for i, (Ab, Bb, Cb) in enumerate(zip(A_blocks, B_blocks, C_blocks)):
            m = len(Ab)
            A[idx:idx+m, idx:idx+m] = Ab
            B[idx:idx+m, 0] = np.array(Bb).flatten()
            C[0, idx:idx+m] = np.array(Cb).flatten()
            idx += m
        
        D = np.array([[float(self.d)]])
        E = np.array([[float(self.e)]])
        
        return {
            'A': A.tolist(),
            'B': B.tolist(),
            'C': C.tolist(),
            'D': D.tolist(),
            'E': E.tolist(),
            'n_states': n_states,
            'n_outputs': 1
        }
    
    def export_to_json(self, filename: str, fs: float = 80e9) -> None:
        """
        Export state-space representation to JSON file for C++ Channel.
        
        Args:
            filename: Output JSON file path
            fs: Sampling frequency in Hz
        """
        import json
        
        state_space = self.to_state_space()
        
        config = {
            'version': '2.1-vf',
            'method': 'state_space',
            'fs': float(fs),
            'state_space': {
                'A': state_space['A'],
                'B': state_space['B'],
                'C': state_space['C'],
                'D': state_space['D'],
                'E': state_space['E']
            },
            'metadata': {
                'order': self.order,
                'n_states': state_space['n_states'],
                'n_outputs': state_space['n_outputs']
            }
        }
        
        with open(filename, 'w') as f:
            json.dump(config, f, indent=2)
        
        logger.info(f"Exported state-space to: {filename}")


def fit_vector_fitting(s: np.ndarray, H: np.ndarray, order: int = 8,
                       max_iterations: int = 10, tolerance: float = 1e-6,
                       **kwargs) -> Dict:
    """
    Convenience function for Vector Fitting.
    
    Args:
        s: Complex frequency array (rad/s)
        H: Frequency response
        order: Fitting order
        max_iterations: Maximum iterations
        tolerance: Convergence tolerance
        **kwargs: Additional options for VectorFitting
        
    Returns:
        Fitting results dictionary
    """
    vf = VectorFitting(order=order, max_iterations=max_iterations,
                       tolerance=tolerance, **kwargs)
    return vf.fit(s, H)
