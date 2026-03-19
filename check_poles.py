import json
import numpy as np

with open('/mnt/d/systemCProjects/SerDesSystemCProject/.worktrees/channel-p1-batch3-scikit-rf/build/channel_pole_residue.json') as f:
    data = json.load(f)

pr = data['pole_residue']
poles = np.array(pr['poles_real']) + 1j * np.array(pr['poles_imag'])
residues = np.array(pr['residues_real']) + 1j * np.array(pr['residues_imag'])

print(f"Total poles: {len(poles)}")
print(f"Complex poles (|imag| > 1e-12): {np.sum(np.abs(poles.imag) > 1e-12)}")
print(f"Real poles: {np.sum(np.abs(poles.imag) <= 1e-12)}")

print("\nFirst 10 poles:")
for i in range(min(10, len(poles))):
    p = poles[i]
    r = residues[i]
    print(f"  {i}: p={p.real:.4e}{p.imag:+.4e}j, r={r.real:.4e}{r.imag:+.4e}j")

# Check for conjugate pairs
print("\nChecking for conjugate pairs...")
used = set()
pairs = []
for i in range(len(poles)):
    if i in used:
        continue
    p1 = poles[i]
    found_pair = False
    for j in range(i+1, len(poles)):
        if j in used:
            continue
        p2 = poles[j]
        # Check if conjugate
        if np.abs(p1 - np.conj(p2)) < 1e-6 * np.abs(p1):
            pairs.append((i, j))
            used.add(i)
            used.add(j)
            found_pair = True
            break
    if not found_pair and np.abs(p1.imag) > 1e-12:
        print(f"  WARNING: Pole {i} has no conjugate pair!")

print(f"Found {len(pairs)} conjugate pairs")
print(f"Unpaired real poles: {len(poles) - 2*len(pairs)}")
