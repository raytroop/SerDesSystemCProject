#!/usr/bin/env python3
"""
Unit tests for VectorFittingPY module.

Tests follow the plan in docs/plans/2024-03-19-vectorfitting-python-plan.md
"""

import numpy as np
import sys
import os

# Add scripts directory to path for importing
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'scripts'))

from vector_fitting_py import VectorFittingPY


def test_import():
    """Test that the module can be imported and class is available."""
    assert VectorFittingPY is not None


def test_init_1d():
    """Test class initialization with 1D frequency response (SISO)."""
    freq = np.linspace(1e6, 10e9, 100)
    h = np.random.randn(100) + 1j * np.random.randn(100)
    vf = VectorFittingPY(freq, h)
    
    # Check dimensions
    assert vf.Nc == 1
    assert vf.Ns == 100
    
    # Check arrays
    assert np.array_equal(vf.freq, freq)
    assert vf.h.shape == (1, 100)
    
    # Check complex frequency
    expected_s = 1j * 2 * np.pi * freq
    assert np.allclose(vf.s, expected_s)
    
    # Check initial state
    assert vf.poles is None
    assert vf.residues is None
    assert vf.SER is None


def test_init_2d():
    """Test class initialization with 2D frequency response (MIMO)."""
    freq = np.linspace(1e6, 10e9, 50)
    h = np.random.randn(4, 50) + 1j * np.random.randn(4, 50)
    vf = VectorFittingPY(freq, h)
    
    # Check dimensions
    assert vf.Nc == 4
    assert vf.Ns == 50
    
    # Check arrays
    assert vf.h.shape == (4, 50)


def test_init_preserves_dtype():
    """Test that input arrays are properly converted to numpy arrays."""
    freq = [1e6, 1e7, 1e8, 1e9]
    h = [1+2j, 2+3j, 3+4j, 4+5j]
    
    vf = VectorFittingPY(freq, h)
    
    assert isinstance(vf.freq, np.ndarray)
    assert isinstance(vf.h, np.ndarray)
    assert vf.freq.dtype == float
    assert vf.h.dtype == complex


# =============================================================================
# Tests for _init_poles
# =============================================================================

def test_init_poles_log_spacing():
    """Test pole initialization with logarithmic spacing."""
    freq = np.geomspace(1e6, 10e9, 100)
    h = np.random.randn(100) + 1j * np.random.randn(100)
    vf = VectorFittingPY(freq, h)
    
    n_cmplx = 5
    poles = vf._init_poles(n_cmplx, spacing='log')
    
    # Check shape
    assert len(poles) == n_cmplx
    
    # Check stability: all real parts should be negative
    assert np.all(poles.real < 0), "Poles must be stable (negative real part)"
    
    # Check upper half-plane: all imaginary parts should be positive
    assert np.all(poles.imag > 0), "Poles must be in upper half-plane"
    
    # Check damping ratio: real part should be -0.01 * imag part
    expected_real = -0.01 * poles.imag
    assert np.allclose(poles.real, expected_real), "Poles should have -0.01 damping"


def test_init_poles_lin_spacing():
    """Test pole initialization with linear spacing."""
    freq = np.linspace(1e6, 10e9, 100)
    h = np.random.randn(100) + 1j * np.random.randn(100)
    vf = VectorFittingPY(freq, h)
    
    n_cmplx = 10
    poles = vf._init_poles(n_cmplx, spacing='lin')
    
    # Check shape
    assert len(poles) == n_cmplx
    
    # Check stability
    assert np.all(poles.real < 0)
    
    # Check upper half-plane
    assert np.all(poles.imag > 0)
    
    # Check that poles are sorted by imaginary part (linear spacing)
    assert np.all(np.diff(poles.imag) > 0), "Poles should be sorted by frequency"


def test_init_poles_frequency_range():
    """Test that poles are initialized within expected frequency range."""
    freq = np.geomspace(1e6, 10e9, 100)
    h = np.random.randn(100) + 1j * np.random.randn(100)
    vf = VectorFittingPY(freq, h)
    
    n_cmplx = 20
    poles = vf._init_poles(n_cmplx, spacing='log')
    
    # Convert pole frequencies back to Hz (omega = 2*pi*f)
    pole_freqs = poles.imag / (2 * np.pi)
    
    # Poles should be within [fmin, fmax] of the frequency range
    fmin = max(freq[freq > 0].min(), 1e3)
    fmax = freq.max()
    
    # Allow small tolerance for floating point
    assert np.all(pole_freqs >= fmin * 0.99), f"Pole freqs should be >= {fmin}"
    assert np.all(pole_freqs <= fmax * 1.01), f"Pole freqs should be <= {fmax}"


def test_init_poles_zero_freq_handling():
    """Test that zero frequency is handled correctly."""
    freq = np.array([0, 1e6, 1e7, 1e8, 1e9])
    h = np.random.randn(5) + 1j * np.random.randn(5)
    vf = VectorFittingPY(freq, h)
    
    poles = vf._init_poles(3, spacing='lin')
    
    # Check stability and upper half-plane
    assert np.all(poles.real < 0)
    assert np.all(poles.imag > 0)


# =============================================================================
# Tests for _build_cindex
# =============================================================================

def test_build_cindex_real_only():
    """Test cindex building with only real poles."""
    freq = np.linspace(1e6, 10e9, 10)
    h = np.random.randn(10) + 1j * np.random.randn(10)
    vf = VectorFittingPY(freq, h)
    
    # Real poles only
    poles = np.array([-1e6, -2e6, -3e6])
    cindex = vf._build_cindex(poles)
    
    # All should be 0 (real)
    expected = np.array([0, 0, 0])
    assert np.array_equal(cindex, expected), f"Expected {expected}, got {cindex}"


def test_build_cindex_complex_pairs():
    """Test cindex building with complex conjugate pairs."""
    freq = np.linspace(1e6, 10e9, 10)
    h = np.random.randn(10) + 1j * np.random.randn(10)
    vf = VectorFittingPY(freq, h)
    
    # One complex pair (consecutive, positive then negative imag)
    poles = np.array([-1e6 + 1e9j, -1e6 - 1e9j])
    cindex = vf._build_cindex(poles)
    
    expected = np.array([1, 2])
    assert np.array_equal(cindex, expected), f"Expected {expected}, got {cindex}"


def test_build_cindex_mixed():
    """Test cindex building with mixed real and complex poles."""
    freq = np.linspace(1e6, 10e9, 10)
    h = np.random.randn(10) + 1j * np.random.randn(10)
    vf = VectorFittingPY(freq, h)
    
    # Mixed: real, complex pair, real
    poles = np.array([-1e6, -2e6 + 1e9j, -2e6 - 1e9j, -3e6])
    cindex = vf._build_cindex(poles)
    
    expected = np.array([0, 1, 2, 0])
    assert np.array_equal(cindex, expected), f"Expected {expected}, got {cindex}"


def test_build_cindex_multiple_pairs():
    """Test cindex building with multiple complex pairs."""
    freq = np.linspace(1e6, 10e9, 10)
    h = np.random.randn(10) + 1j * np.random.randn(10)
    vf = VectorFittingPY(freq, h)
    
    # Multiple complex pairs
    poles = np.array([
        -1e6 + 1e9j, -1e6 - 1e9j,  # First pair
        -2e6 + 2e9j, -2e6 - 2e9j,  # Second pair
        -3e6 + 3e9j, -3e6 - 3e9j   # Third pair
    ])
    cindex = vf._build_cindex(poles)
    
    expected = np.array([1, 2, 1, 2, 1, 2])
    assert np.array_equal(cindex, expected), f"Expected {expected}, got {cindex}"


def test_build_cindex_length():
    """Test that cindex has same length as poles."""
    freq = np.linspace(1e6, 10e9, 10)
    h = np.random.randn(10) + 1j * np.random.randn(10)
    vf = VectorFittingPY(freq, h)
    
    for n_poles in [1, 2, 5, 10]:
        # Generate complex poles using _init_poles
        poles = vf._init_poles(n_poles, spacing='log')
        cindex = vf._build_cindex(poles)
        assert len(cindex) == len(poles), f"cindex length mismatch for {n_poles} poles"


# =============================================================================
# Tests for _build_Dk
# =============================================================================

def test_build_Dk_real_pole():
    """Test Dk matrix building with a single real pole."""
    freq = np.array([1e6, 1e7, 1e8])
    h = np.random.randn(3) + 1j * np.random.randn(3)
    vf = VectorFittingPY(freq, h)
    
    s = vf.s
    poles = np.array([-1e6])
    cindex = np.array([0])
    
    Dk = vf._build_Dk(s, poles, cindex)
    
    # Expected: real part of 1/(s - p)
    expected = (1.0 / (s - poles[0])).real
    
    assert Dk.shape == (3, 1)
    assert Dk.dtype == float, "Dk should be real-valued"
    assert np.allclose(Dk[:, 0], expected)


def test_build_Dk_complex_pair():
    """Test Dk matrix building with a complex conjugate pair."""
    freq = np.array([1e6, 1e7, 1e8])
    h = np.random.randn(3) + 1j * np.random.randn(3)
    vf = VectorFittingPY(freq, h)
    
    s = vf.s
    p = -1e6 + 1e9j
    p_conj = np.conj(p)
    poles = np.array([p, p_conj])
    cindex = np.array([1, 2])
    
    Dk = vf._build_Dk(s, poles, cindex)
    
    # Expected for first column: real part of (1/(s-p) + 1/(s-p*))
    expected_0 = (1.0 / (s - p) + 1.0 / (s - p_conj)).real
    # Expected for second column: real part of (1j/(s-p) - 1j/(s-p*))
    expected_1 = (1.0j / (s - p) - 1.0j / (s - p_conj)).real
    
    assert Dk.shape == (3, 2)
    assert Dk.dtype == float, "Dk should be real-valued"
    assert np.allclose(Dk[:, 0], expected_0)
    assert np.allclose(Dk[:, 1], expected_1)


def test_build_Dk_shape():
    """Test that Dk has correct shape for different numbers of poles."""
    freq = np.linspace(1e6, 10e9, 50)
    h = np.random.randn(50) + 1j * np.random.randn(50)
    vf = VectorFittingPY(freq, h)
    
    s = vf.s
    
    for n_cmplx in [2, 4, 8]:
        poles = vf._init_poles(n_cmplx, spacing='log')
        cindex = vf._build_cindex(poles)
        Dk = vf._build_Dk(s, poles, cindex)
        
        # Dk shape should be (Ns, N) where N is number of poles
        assert Dk.shape == (len(s), len(poles)), \
            f"Shape mismatch for n_cmplx={n_cmplx}"


def test_build_Dk_real_result():
    """Test that Dk columns produce real-valued results."""
    freq = np.linspace(1e6, 10e9, 10)
    h = np.random.randn(10) + 1j * np.random.randn(10)
    vf = VectorFittingPY(freq, h)
    
    # Test with positive-imaginary poles only (from _init_poles)
    poles = vf._init_poles(3, spacing='log')
    cindex = vf._build_cindex(poles)
    Dk = vf._build_Dk(vf.s, poles, cindex)
    
    # Dk should be real-valued (float dtype)
    assert Dk.dtype == float, "Dk should be float dtype"
    
    # All columns should be real-valued
    for i in range(Dk.shape[1]):
        assert np.all(np.isreal(Dk[:, i])), f"Column {i} should be real-valued"
    
    # Test with conjugate pairs
    p1 = -1e6 + 1e9j
    p2 = -2e6 + 2e9j
    poles_conj = np.array([p1, np.conj(p1), p2, np.conj(p2)])
    cindex_conj = vf._build_cindex(poles_conj)
    Dk_conj = vf._build_Dk(vf.s, poles_conj, cindex_conj)
    
    assert Dk_conj.dtype == float, "Dk with conjugates should be float dtype"
    for i in range(Dk_conj.shape[1]):
        assert np.all(np.isreal(Dk_conj[:, i])), f"Column {i} should be real-valued"


if __name__ == '__main__':
    # Run tests manually if pytest is not available
    print("Running tests...")
    
    test_import()
    print("✓ test_import passed")
    
    test_init_1d()
    print("✓ test_init_1d passed")
    
    test_init_2d()
    print("✓ test_init_2d passed")
    
    test_init_preserves_dtype()
    print("✓ test_init_preserves_dtype passed")
    
    # Pole initialization tests
    test_init_poles_log_spacing()
    print("✓ test_init_poles_log_spacing passed")
    
    test_init_poles_lin_spacing()
    print("✓ test_init_poles_lin_spacing passed")
    
    test_init_poles_frequency_range()
    print("✓ test_init_poles_frequency_range passed")
    
    test_init_poles_zero_freq_handling()
    print("✓ test_init_poles_zero_freq_handling passed")
    
    # cindex tests
    test_build_cindex_real_only()
    print("✓ test_build_cindex_real_only passed")
    
    test_build_cindex_complex_pairs()
    print("✓ test_build_cindex_complex_pairs passed")
    
    test_build_cindex_mixed()
    print("✓ test_build_cindex_mixed passed")
    
    test_build_cindex_multiple_pairs()
    print("✓ test_build_cindex_multiple_pairs passed")
    
    test_build_cindex_length()
    print("✓ test_build_cindex_length passed")
    
    # Dk tests
    test_build_Dk_real_pole()
    print("✓ test_build_Dk_real_pole passed")
    
    test_build_Dk_complex_pair()
    print("✓ test_build_Dk_complex_pair passed")
    
    test_build_Dk_shape()
    print("✓ test_build_Dk_shape passed")
    
    test_build_Dk_real_result()
    print("✓ test_build_Dk_real_result passed")
    
    print("\nAll tests passed!")
