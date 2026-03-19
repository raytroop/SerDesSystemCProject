import json
import numpy as np
import skrf as rf
from scipy import signal

def verify_pole_residue_response():
    # Load S-parameter file
    ntwk = rf.Network('/mnt/d/systemCProjects/SerDesSystemCProject/peters_01_0605_B12_thru.s4p')
    s21 = ntwk.s[:, 1, 0]  # S21
    freqs = ntwk.f  # Hz
    
    # Load pole-residue data
    with open('/mnt/d/systemCProjects/SerDesSystemCProject/.worktrees/channel-p1-batch3-scikit-rf/build/channel_pole_residue.json') as f:
        pr_data = json.load(f)
    
    poles = np.array(pr_data['poles_real']) + 1j * np.array(pr_data['poles_imag'])
    residues = np.array(pr_data['residues_real']) + 1j * np.array(pr_data['residues_imag'])
    
    # Compute frequency response from poles and residues
    # H(s) = sum(r / (s - p)) + constant
    omega = 2 * np.pi * freqs  # rad/s
    s = 1j * omega
    
    H_fit = np.zeros(len(s), dtype=complex)
    for p, r in zip(poles, residues):
        H_fit += r / (s - p)
    H_fit += pr_data['constant']
    
    # Compute correlation with original S21
    corr = np.corrcoef(np.abs(s21), np.abs(H_fit))[0, 1]
    
    print(f"Pole-Residue Model Correlation: {corr:.6f}")
    print(f"Number of poles: {len(poles)}")
    
    # Also compute at a few key frequencies
    print("\nFrequency Response Comparison:")
    for i in [0, len(freqs)//4, len(freqs)//2, 3*len(freqs)//4, len(freqs)-1]:
        print(f"  {freqs[i]/1e9:.2f} GHz: |S21|={np.abs(s21[i]):.6f}, |H_fit|={np.abs(H_fit[i]):.6f}")

if __name__ == "__main__":
    verify_pole_residue_response()
