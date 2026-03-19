import json
import numpy as np

with open('/mnt/d/systemCProjects/SerDesSystemCProject/.worktrees/channel-p1-batch3-scikit-rf/build/channel_pole_residue.json') as f:
    data = json.load(f)

pr = data['pole_residue']
poles = np.array(pr['poles_real']) + 1j * np.array(pr['poles_imag'])

print("All poles (real, imag):")
for i, p in enumerate(poles):
    print(f"  {i}: ({p.real:.6e}, {p.imag:.6e})")

# Check for near-matches (not exact, since there might be small numerical differences)
print("\n\nLooking for near-conjugate pairs (tolerance 1e-3 * |p|):")
for i in range(len(poles)):
    p1 = poles[i]
    for j in range(len(poles)):
        if i >= j:
            continue
        p2 = poles[j]
        diff = np.abs(p1 - np.conj(p2))
        avg_mag = (np.abs(p1) + np.abs(p2)) / 2
        if diff < 1e-3 * avg_mag:
            print(f"  Pair: ({i},{j}) -> p{i}={p1}, p{j}={p2}, diff={diff:.6e}")
        elif np.abs(p1.imag) > 1e-12 and np.abs(p2.imag) > 1e-12 and np.abs(p1.imag - p2.imag) < 1e6:
            # Same magnitude imag part
            if np.abs(np.abs(p1.imag) - np.abs(p2.imag)) < 1e-3 * np.abs(p1.imag):
                print(f"  Near pair: ({i},{j}) -> imag diff = {np.abs(np.abs(p1.imag) - np.abs(p2.imag)):.6e}")
