"""
Vector Fitting algorithm implementation for S-parameter rational function fitting.
This module provides the core algorithms for converting frequency domain S-parameters
to rational functions in the Laplace domain.
"""

import numpy as np
from scipy.linalg import lstsq
import logging

logger = logging.getLogger(__name__)


class VectorFitting:
    """
    Vector Fitting algorithm implementation for rational function approximation
    of frequency domain data (e.g., S-parameters).
    
    The algorithm approximates frequency response H(s) as:
    H(s) = exp(-s*delay) * [sum(r_k / (s - p_k)) + d]
    
    where r_k are residues, p_k are poles, d is direct term, and delay is
    estimated group delay.
    
    This implementation uses:
    - Automatic delay extraction for transmission lines
    - Frequency normalization for numerical stability
    - Column scaling for better conditioning
    """
    
    def __init__(self, order=8, max_iterations=10, tolerance=1e-6):
        """
        Initialize the Vector Fitting algorithm.
        
        Args:
            order: Number of poles (fitting order), recommended 6-16
            max_iterations: Maximum number of VF iterations
            tolerance: Convergence tolerance for pole relocation
        """
        self.order = order
        self.max_iterations = max_iterations
        self.tolerance = tolerance
        
        # Results storage
        self.poles = None
        self.residues = None
        self.d = 0.0  # Direct term
        self.h = 0.0  # s-proportional term (usually 0)
        self.delay = 0.0  # Estimated delay
        self.num_coeffs = None
        self.den_coeffs = None
        self.mse = None
        
        # Normalization factors
        self._freq_scale = 1.0
        self._s_scale = 1.0
    
    def _estimate_delay(self, freq, H_data):
        """
        Estimate group delay from phase slope.
        
        Args:
            freq: Frequency array (Hz)
            H_data: Complex frequency response
            
        Returns:
            Estimated delay in seconds
        """
        phase = np.unwrap(np.angle(H_data))
        
        # Use linear regression on phase vs angular frequency
        # phase = -delay * omega + offset
        omega = 2 * np.pi * freq
        
        # Robust fit using middle portion of data to avoid edge effects
        n = len(freq)
        start_idx = n // 10
        end_idx = n - n // 10
        
        if end_idx <= start_idx:
            start_idx = 0
            end_idx = n
        
        poly = np.polyfit(omega[start_idx:end_idx], phase[start_idx:end_idx], 1)
        delay = -poly[0]
        
        # Ensure delay is positive
        if delay < 0:
            delay = 0
        
        return delay
        
    def _initialize_poles(self, freq):
        """
        Initialize starting poles distributed across the frequency range.
        Uses complex conjugate pairs with damping optimized for channel response.
        
        Args:
            freq: Frequency array (Hz)
            
        Returns:
            Initial pole array (complex), normalized
        """
        f_min = freq[freq > 0].min() if np.any(freq > 0) else 1.0
        f_max = freq.max()
        
        # Store normalization factor
        self._freq_scale = f_max
        self._s_scale = 2 * np.pi * f_max
        
        # Create poles: some below data range for DC extrapolation
        n_complex_pairs = self.order // 2
        n_real = self.order % 2
        
        poles = []
        
        # Complex conjugate pairs (normalized)
        if n_complex_pairs > 0:
            # Extend range below f_min for better low-frequency fit
            f_start = f_min / 10  # Start one decade below minimum data frequency
            pole_freqs = np.logspace(np.log10(f_start), np.log10(f_max), n_complex_pairs)
            
            for f in pole_freqs:
                omega_norm = 2 * np.pi * f / self._s_scale
                # Use moderate damping
                sigma = omega_norm * 0.3
                poles.append(-sigma + 1j * omega_norm)
                poles.append(-sigma - 1j * omega_norm)
        
        # Real poles if order is odd (normalized)
        if n_real > 0:
            f_mid = np.sqrt(f_min * f_max)
            omega_mid_norm = 2 * np.pi * f_mid / self._s_scale
            poles.append(-omega_mid_norm * 0.3)
        
        return np.array(poles, dtype=complex)
    
    def _build_system_matrix(self, s_norm, poles):
        """
        Build the system matrix for least squares fitting.
        
        Args:
            s_norm: Normalized complex frequency array (j*2*pi*f/s_scale)
            poles: Current pole estimates (normalized)
            
        Returns:
            A: System matrix
        """
        N = len(s_norm)
        M = len(poles)
        
        # Build matrix: [1/(s-p1), 1/(s-p2), ..., 1]
        # Note: We exclude the s-proportional term (h*s) for stability
        # as it causes numerical issues and is usually not needed for
        # typical S-parameter fitting
        A = np.zeros((N, M + 1), dtype=complex)
        
        for k, p in enumerate(poles):
            A[:, k] = 1.0 / (s_norm - p)
        
        A[:, M] = 1.0      # Direct term d
        
        return A
    
    def _solve_residues(self, s_norm, H, poles):
        """
        Solve for residues given fixed poles using least squares.
        
        Args:
            s_norm: Normalized complex frequency array
            H: Target frequency response (complex)
            poles: Current pole locations (normalized)
            
        Returns:
            residues: Residue array
            d: Direct term
            h: s-proportional term (always 0 in this implementation)
        """
        A = self._build_system_matrix(s_norm, poles)
        
        # Apply column scaling for better conditioning
        col_norms = np.linalg.norm(A, axis=0)
        col_norms[col_norms < 1e-10] = 1.0  # Avoid division by zero
        A_scaled = A / col_norms
        
        # Stack real and imaginary parts for real-valued least squares
        A_real = np.vstack([A_scaled.real, A_scaled.imag])
        b_real = np.hstack([H.real, H.imag])
        
        # Solve least squares with SVD for better stability
        result, _, _, _ = lstsq(A_real, b_real, lapack_driver='gelsd')
        
        # Unscale the result
        result = result / col_norms
        
        M = len(poles)
        residues = result[:M]
        d = result[M]
        h = 0.0  # We don't use s-proportional term
        
        return residues, d, h
    
    def _relocate_poles(self, s_norm, H, poles, residues):
        """
        Relocate poles using the vector fitting pole relocation formula.
        
        Args:
            s_norm: Normalized complex frequency array
            H: Target frequency response
            poles: Current poles (normalized)
            residues: Current residues
            
        Returns:
            new_poles: Relocated poles (normalized)
        """
        N = len(s_norm)
        M = len(poles)
        
        # Build augmented system for pole relocation
        # sigma(s) = sum(r_k / (s - p_k)) + 1
        # sigma(s) * H_fit(s) = H(s) * sigma(s)
        
        # Left side: [1/(s-p1), ..., 1/(s-pM), 1] for fitting
        # Right side: [-H*1/(s-p1), ..., -H*1/(s-pM)] for sigma
        
        A_left = self._build_system_matrix(s_norm, poles)
        A_right = np.zeros((N, M), dtype=complex)
        
        for k, p in enumerate(poles):
            A_right[:, k] = -H / (s_norm - p)
        
        # Combined system
        A_full = np.hstack([A_left, A_right])
        
        # Apply column scaling
        col_norms = np.linalg.norm(A_full, axis=0)
        col_norms[col_norms < 1e-10] = 1.0
        A_scaled = A_full / col_norms
        
        # Stack real and imaginary
        A_real = np.vstack([A_scaled.real, A_scaled.imag])
        b_real = np.hstack([H.real, H.imag])
        
        # Solve
        result, _, _, _ = lstsq(A_real, b_real, lapack_driver='gelsd')
        
        # Unscale
        result = result / col_norms
        
        # Extract sigma residues (last M values)
        sigma_residues = result[M + 1:]  # After residues and d term
        
        # Form sigma polynomial and find zeros (new poles)
        # sigma(s) = sum(sigma_r_k / (s - p_k)) + 1
        # The zeros of sigma become the new poles
        
        # Build companion matrix for sigma
        # This is equivalent to finding eigenvalues of a state-space matrix
        
        # Build state-space realization
        A_state = np.diag(poles)
        B_state = np.ones(M)
        C_state = sigma_residues
        
        # New poles are eigenvalues of (A - B*C)
        if np.all(np.abs(sigma_residues) < 1e-12):
            # No relocation needed
            return poles.copy()
        
        try:
            # Form the modified state matrix
            BC = np.outer(B_state, C_state)
            state_matrix = A_state - BC
            new_poles = np.linalg.eigvals(state_matrix)
        except np.linalg.LinAlgError:
            logger.warning("Eigenvalue computation failed, keeping current poles")
            return poles.copy()
        
        return new_poles
    
    def _enforce_stability(self, poles):
        """
        Enforce stability by flipping unstable poles to the left half-plane.
        
        Args:
            poles: Pole array
            
        Returns:
            Stabilized poles
        """
        stable_poles = poles.copy()
        
        for i, p in enumerate(stable_poles):
            if np.real(p) > 0:
                # Flip to left half-plane
                stable_poles[i] = -np.abs(np.real(p)) + 1j * np.imag(p)
                logger.debug(f"Flipped unstable pole {p} to {stable_poles[i]}")
        
        return stable_poles
    
    def _enforce_conjugate_pairs(self, poles):
        """
        Ensure complex poles come in conjugate pairs.
        Maintains the original number of poles.
        
        Args:
            poles: Pole array
            
        Returns:
            Poles with enforced conjugate pairs (same length as input)
        """
        M = len(poles)
        result = []
        used = set()
        
        # Sort poles by imaginary part to pair up conjugates more reliably
        indices = np.argsort(np.imag(poles))
        poles_sorted = poles[indices]
        
        i = 0
        while i < M:
            p = poles_sorted[i]
            
            if np.abs(np.imag(p)) < 1e-10 * (np.abs(p) + 1e-10):
                # Real pole - keep as is
                result.append(np.real(p) + 0j)
                i += 1
            else:
                # Complex pole - pair with next one to form conjugate pair
                if i + 1 < M:
                    q = poles_sorted[i + 1]
                    # Average to form a proper conjugate pair
                    avg_real = (np.real(p) + np.real(q)) / 2
                    avg_imag = (np.abs(np.imag(p)) + np.abs(np.imag(q))) / 2
                    
                    # Ensure positive imaginary part first
                    if avg_imag > 0:
                        result.append(avg_real + 1j * avg_imag)
                        result.append(avg_real - 1j * avg_imag)
                    else:
                        result.append(avg_real - 1j * avg_imag)
                        result.append(avg_real + 1j * avg_imag)
                    i += 2
                else:
                    # Last pole is complex without a pair - make it real
                    result.append(np.real(p) + 0j)
                    i += 1
        
        return np.array(result, dtype=complex)
    
    def fit(self, freq, H_data, enforce_stability=True, enforce_passivity=False, 
            remove_delay=True):
        """
        Perform vector fitting on frequency response data.
        
        For transmission line S-parameters (like S21), this method can automatically
        detect and remove group delay before fitting, which dramatically improves
        numerical stability and fitting accuracy.
        
        Args:
            freq: Frequency array (Hz)
            H_data: Complex frequency response data
            enforce_stability: If True, flip unstable poles to LHP
            enforce_passivity: If True, enforce passivity constraint
            remove_delay: If True, estimate and remove group delay before fitting
            
        Returns:
            dict: Fitting results containing poles, residues, coefficients, MSE
        """
        logger.info(f"Starting Vector Fitting with order={self.order}")
        
        # Estimate and optionally remove group delay
        H_fit_data = H_data
        self.delay = 0.0
        
        if remove_delay:
            self.delay = self._estimate_delay(freq, H_data)
            if self.delay > 1e-12:
                logger.info(f"Estimated group delay: {self.delay*1e12:.2f} ps")
                # Remove delay from data for fitting
                omega = 2 * np.pi * freq
                H_fit_data = H_data * np.exp(1j * omega * self.delay)
                logger.debug(f"Phase range after delay removal: "
                           f"{np.angle(H_fit_data[0])*180/np.pi:.1f} to "
                           f"{np.angle(H_fit_data[-1])*180/np.pi:.1f} deg")
        
        # Initialize poles (this also sets _s_scale)
        self.poles = self._initialize_poles(freq)
        
        # Convert to normalized complex frequency (s_norm = j*2*pi*f / s_scale)
        s_norm = 1j * 2 * np.pi * freq / self._s_scale
        
        logger.debug(f"Initial poles (normalized): {self.poles}")
        logger.debug(f"Frequency scale: {self._s_scale:.2e}")
        
        # Iterative pole relocation
        for iteration in range(self.max_iterations):
            # Solve for residues with current poles (using delay-removed data)
            self.residues, self.d, self.h = self._solve_residues(s_norm, H_fit_data, self.poles)
            
            # Relocate poles
            new_poles = self._relocate_poles(s_norm, H_fit_data, self.poles, self.residues)
            
            # Enforce stability if requested
            if enforce_stability:
                new_poles = self._enforce_stability(new_poles)
            
            # Enforce conjugate pairs
            new_poles = self._enforce_conjugate_pairs(new_poles)
            
            # Check convergence
            pole_change = np.max(np.abs(new_poles - self.poles))
            rel_change = pole_change / (np.max(np.abs(self.poles)) + 1e-10)
            
            logger.debug(f"Iteration {iteration + 1}: relative pole change = {rel_change:.2e}")
            
            self.poles = new_poles
            
            if rel_change < self.tolerance:
                logger.info(f"Converged after {iteration + 1} iterations")
                break
        
        # Final residue solve with normalized frequencies (using delay-removed data)
        self.residues, self.d, self.h = self._solve_residues(s_norm, H_fit_data, self.poles)
        
        # Store the normalized poles for internal use
        # When evaluating, we'll use normalized s
        self._poles_normalized = self.poles.copy()
        self._residues_normalized = self.residues.copy()
        
        # Denormalize poles for output (convert back to rad/s)
        # p_actual = p_normalized * s_scale
        self.poles = self._poles_normalized * self._s_scale
        # Residues scale the same way: r_actual/(s - p_actual) = r_norm/(s_norm - p_norm)
        # Since s - p = s_scale * (s_norm - p_norm), we have r_actual = r_norm * s_scale
        self.residues = self._residues_normalized * self._s_scale
        
        # Convert to polynomial coefficients
        self._compute_polynomial_coefficients()
        
        # Compute fitting error
        H_fit = self.evaluate(freq)
        self.mse = np.mean(np.abs(H_data - H_fit) ** 2)
        max_error = np.max(np.abs(H_data - H_fit))
        
        logger.info(f"Fitting complete: MSE={self.mse:.2e}, max_error={max_error:.2e}")
        
        return {
            'poles': self.poles,
            'residues': self.residues,
            'd': self.d,
            'h': self.h,
            'delay': self.delay,
            'num': self.num_coeffs,
            'den': self.den_coeffs,
            'mse': self.mse,
            'max_error': max_error,
            'order': self.order
        }
    
    def _compute_polynomial_coefficients(self):
        """
        Convert pole-residue form to polynomial ratio form.
        H(s) = (b_n*s^n + ... + b_0) / (a_m*s^m + ... + a_0)
        """
        # Denominator from poles: prod(s - p_k)
        self.den_coeffs = np.array([1.0])
        for p in self.poles:
            # Multiply by (s - p)
            self.den_coeffs = np.convolve(self.den_coeffs, [1.0, -p])
        
        # Take real part (imaginary should be negligible for real system)
        self.den_coeffs = np.real(self.den_coeffs)
        
        # Numerator from residues: sum of r_k * prod_{j!=k}(s - p_j) + d*den + h*s*den
        M = len(self.poles)
        
        # Start with zero numerator
        self.num_coeffs = np.zeros(M + 1, dtype=complex)
        
        # Add residue contributions
        for k, r in enumerate(self.residues):
            # Compute prod_{j!=k}(s - p_j)
            partial_den = np.array([1.0], dtype=complex)
            for j, p in enumerate(self.poles):
                if j != k:
                    partial_den = np.convolve(partial_den, [1.0, -p])
            
            # Pad to same length as num_coeffs
            padded = np.zeros(M + 1, dtype=complex)
            padded[-len(partial_den):] = partial_den
            
            self.num_coeffs += r * padded
        
        # Add direct term: d * den
        d_contrib = self.d * self.den_coeffs
        padded_d = np.zeros(M + 1, dtype=complex)
        padded_d[-len(d_contrib):] = d_contrib
        self.num_coeffs += padded_d
        
        # Add s-proportional term: h * s * den (increases order by 1)
        if np.abs(self.h) > 1e-12:
            h_contrib = self.h * np.convolve(self.den_coeffs, [1.0, 0.0])
            # This increases numerator order, handle carefully
            if len(h_contrib) > len(self.num_coeffs):
                new_num = np.zeros(len(h_contrib), dtype=complex)
                new_num[-len(self.num_coeffs):] = self.num_coeffs
                self.num_coeffs = new_num + h_contrib
            else:
                padded_h = np.zeros(len(self.num_coeffs), dtype=complex)
                padded_h[-len(h_contrib):] = h_contrib
                self.num_coeffs += padded_h
        
        # Take real part
        self.num_coeffs = np.real(self.num_coeffs)
        
        # Normalize denominator to have leading coefficient = 1
        if np.abs(self.den_coeffs[0]) > 1e-12:
            self.num_coeffs = self.num_coeffs / self.den_coeffs[0]
            self.den_coeffs = self.den_coeffs / self.den_coeffs[0]
    
    def evaluate(self, freq):
        """
        Evaluate the fitted rational function at given frequencies.
        Includes delay term if delay was estimated during fitting.
        
        H(s) = exp(-s*delay) * [sum(r_k/(s-p_k)) + d + h*s]
        
        Args:
            freq: Frequency array (Hz)
            
        Returns:
            Complex frequency response
        """
        s = 1j * 2 * np.pi * freq
        
        # Evaluate using pole-residue form for better numerical stability
        H = np.zeros_like(s, dtype=complex)
        
        for r, p in zip(self.residues, self.poles):
            H += r / (s - p)
        
        H += self.d
        H += self.h * s
        
        # Apply delay term if present
        if self.delay > 1e-12:
            H = H * np.exp(-s * self.delay)
        
        return H
    
    def evaluate_polynomial(self, freq):
        """
        Evaluate using polynomial coefficients (for verification).
        
        Args:
            freq: Frequency array (Hz)
            
        Returns:
            Complex frequency response
        """
        s = 1j * 2 * np.pi * freq
        
        num = np.polyval(self.num_coeffs, s)
        den = np.polyval(self.den_coeffs, s)
        
        return num / den
    
    def get_dc_gain(self):
        """
        Get the DC gain (H(0)) of the fitted transfer function.
        
        Returns:
            DC gain (real value)
        """
        # From pole-residue form: H(0) = sum(-r_k/p_k) + d
        dc_gain = self.d
        for r, p in zip(self.residues, self.poles):
            if np.abs(p) > 1e-12:
                dc_gain -= r / p
        
        return np.real(dc_gain)


def check_passivity(S_matrix, freq, tolerance=1e-3):
    """
    Check if S-parameter matrix satisfies passivity constraint.
    Passivity requires: max(eigenvalue(S'*S)) <= 1 for all frequencies.
    
    Args:
        S_matrix: S-parameter matrix [N_freq, N_ports, N_ports]
        freq: Frequency array
        tolerance: Tolerance for passivity check
        
    Returns:
        is_passive: Boolean indicating passivity
        max_eigenvalue: Maximum eigenvalue found
        violation_freqs: Frequencies where passivity is violated
    """
    N_freq = len(freq)
    max_eigenvalue = 0.0
    violation_freqs = []
    
    for i in range(N_freq):
        S = S_matrix[i]
        # Compute S'*S (Hermitian conjugate)
        SHS = np.conj(S.T) @ S
        eigenvalues = np.linalg.eigvalsh(SHS)
        max_eig = np.max(eigenvalues)
        
        if max_eig > max_eigenvalue:
            max_eigenvalue = max_eig
        
        if max_eig > 1.0 + tolerance:
            violation_freqs.append(freq[i])
    
    is_passive = len(violation_freqs) == 0
    
    return is_passive, max_eigenvalue, violation_freqs


def enforce_passivity_perturbation(poles, residues, S_matrix, freq, max_iterations=50):
    """
    Enforce passivity through iterative perturbation of residues.
    This is a simplified approach - full passivity enforcement requires
    more sophisticated optimization.
    
    Args:
        poles: Current poles
        residues: Current residues  
        S_matrix: Original S-parameter data
        freq: Frequency array
        max_iterations: Maximum iterations
        
    Returns:
        Modified residues satisfying passivity
    """
    logger.info("Enforcing passivity through residue perturbation")
    
    residues_new = residues.copy()
    
    for iteration in range(max_iterations):
        # Evaluate current fit
        s = 1j * 2 * np.pi * freq
        H = np.zeros_like(s, dtype=complex)
        for r, p in zip(residues_new, poles):
            H += r / (s - p)
        
        # Check passivity (simplified single-port case)
        max_mag = np.max(np.abs(H))
        
        if max_mag <= 1.0:
            logger.info(f"Passivity achieved after {iteration + 1} iterations")
            break
        
        # Scale residues to reduce magnitude
        scale = 0.99 / max_mag
        residues_new = residues_new * scale
    
    return residues_new
