import json
import numpy as np
import pandas as pd
from scipy import signal

# Load C++ output
df = pd.read_csv('/mnt/d/systemCProjects/SerDesSystemCProject/.worktrees/channel-p1-batch3-scikit-rf/build/channel_pr_waveform.csv')
t_cpp = df['time_s'].values
input_cpp = df['input_V'].values
output_cpp = df['output_V'].values

fs = 1.0 / (t_cpp[1] - t_cpp[0])
print(f"C++ simulation: fs={fs/1e9:.2f} GHz, samples={len(t_cpp)}")

# Load pole-residue data
with open('/mnt/d/systemCProjects/SerDesSystemCProject/.worktrees/channel-p1-batch3-scikit-rf/build/channel_pole_residue.json') as f:
    data = json.load(f)

pr = data['pole_residue']
poles = np.array(pr['poles_real']) + 1j * np.array(pr['poles_imag'])
residues = np.array(pr['residues_real']) + 1j * np.array(pr['residues_imag'])
constant = pr['constant']
proportional = pr['proportional']

print(f"Pole-residue model: {len(poles)} poles, constant={constant:.6f}")

# Build transfer function from poles and residues
# H(s) = sum(r/(s-p)) + constant + proportional*s

# Create state-space model from pole-residue
# For each pole, add a state
A_blocks = []
B_blocks = []
C_blocks = []

for p, r in zip(poles, residues):
    if np.abs(p.imag) > 1e-12:
        # Complex pole - convert to 2x2 real form for conjugate pair
        # H(s) = r/(s-p) + r*/(s-p*)
        # Use second-order section: (b1*s + b0) / (s^2 + a1*s + a0)
        pr_val = p.real
        pi_val = p.imag
        rr_val = r.real
        ri_val = r.imag
        
        # Coefficients for conjugate pair
        b1 = 2 * rr_val
        b0 = -2 * (rr_val * pr_val + ri_val * pi_val)
        a1 = -2 * pr_val
        a0 = pr_val**2 + pi_val**2
        
        # Controllable canonical form
        A = np.array([[0, 1], [-a0, -a1]])
        B = np.array([[0], [1]])
        C = np.array([[b0, b1]])
    else:
        # Real pole: H(s) = r/(s-p)
        A = np.array([[p.real]])
        B = np.array([[r.real]])
        C = np.array([[1.0]])
    
    A_blocks.append(A)
    B_blocks.append(B)
    C_blocks.append(C)

# For simplicity, let's just simulate using scipy's lsim with the transfer function
# Compute frequency response and then use FFT method

# Build transfer function numerator and denominator
from scipy.signal import residue, zpk2tf, tf2ss, lti, lsim

# Start with constant term
num = [constant]
den = [1]

# For each pole-residue pair
for p, r in zip(poles, residues):
    if np.abs(p.imag) > 1e-12:
        # Complex pole pair: combine to second-order section
        pr_val = p.real
        pi_val = p.imag
        rr_val = r.real
        ri_val = r.imag
        
        # H_section(s) = (b1*s + b0) / (s^2 + a1*s + a0)
        b1 = 2 * rr_val
        b0 = -2 * (rr_val * pr_val + ri_val * pi_val)
        a1 = -2 * pr_val
        a0 = pr_val**2 + pi_val**2
        
        # Cascade by convolving polynomials
        num = np.convolve(num, [b1, b0])
        den = np.convolve(den, [1, a1, a0])
    else:
        # Real pole
        # H_section(s) = r / (s - p)
        num = np.convolve(num, [r.real])
        den = np.convolve(den, [1, -p.real])

# Add proportional term (differentiator)
# This is tricky to add as a polynomial - skip for now or add as s term in numerator

# Create LTI system
sys = lti(num, den)

# Simulate with same input
t_py = t_cpp
output_py, _, _ = lsim(sys, U=input_cpp, T=t_py)

# Add constant and proportional terms manually
output_py += constant * input_cpp
# proportional term: derivative
if abs(proportional) > 1e-20:
    deriv = np.gradient(input_cpp, t_py)
    output_py += proportional * deriv

# Compare
print(f"\nC++ output: mean={np.mean(output_cpp):.6f}, std={np.std(output_cpp):.6f}")
print(f"Python output: mean={np.mean(output_py):.6f}, std={np.std(output_py):.6f}")

# Correlation
corr = np.corrcoef(output_cpp, output_py)[0, 1]
print(f"\nCorrelation C++ vs Python: {corr:.6f}")

# Plot first 100 samples
print(f"\nFirst 10 samples:")
for i in range(10):
    print(f"  {i}: C++={output_cpp[i]:.6f}, Python={output_py[i]:.6f}")
