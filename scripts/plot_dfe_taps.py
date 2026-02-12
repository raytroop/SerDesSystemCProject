#!/usr/bin/env python3
"""
DFE Tap Coefficient Analysis Tool
Plots DFE tap coefficient adaptation curves from SerDes simulation
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import os

def plot_dfe_taps(csv_file, output_file=None):
    """Plot DFE tap coefficient curves from CSV data"""
    
    # Read CSV data
    df = pd.read_csv(csv_file)
    
    # Convert time to nanoseconds for better readability
    time_ns = df['time_s'] * 1e9
    
    # Create figure with subplots
    fig, axes = plt.subplots(2, 3, figsize=(15, 10))
    fig.suptitle('DFE Tap Coefficient Adaptation', fontsize=16, fontweight='bold')
    
    # Color scheme for taps
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd']
    
    # Plot individual tap curves
    for i, (ax, tap_name) in enumerate(zip(axes.flat[:5], ['tap1', 'tap2', 'tap3', 'tap4', 'tap5'])):
        ax.plot(time_ns, df[tap_name], color=colors[i], linewidth=2, marker='o', markersize=4)
        ax.set_xlabel('Time (ns)', fontsize=10)
        ax.set_ylabel('Coefficient Value', fontsize=10)
        ax.set_title(f'DFE Tap {i+1}', fontsize=12, fontweight='bold')
        ax.grid(True, alpha=0.3)
        ax.axhline(y=0, color='k', linestyle='--', alpha=0.3)
        
        # Add statistics
        initial = df[tap_name].iloc[0]
        final = df[tap_name].iloc[-1]
        change = final - initial
        ax.text(0.02, 0.98, f'Initial: {initial:.6f}\nFinal: {final:.6f}\nChange: {change:.6f}',
                transform=ax.transAxes, fontsize=8, verticalalignment='top',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    # Plot all taps together in the 6th subplot
    ax_all = axes.flat[5]
    for i, tap_name in enumerate(['tap1', 'tap2', 'tap3', 'tap4', 'tap5']):
        ax_all.plot(time_ns, df[tap_name], color=colors[i], linewidth=2, 
                   marker='o', markersize=3, label=f'Tap {i+1}')
    ax_all.set_xlabel('Time (ns)', fontsize=10)
    ax_all.set_ylabel('Coefficient Value', fontsize=10)
    ax_all.set_title('All DFE Taps', fontsize=12, fontweight='bold')
    ax_all.legend(loc='best', fontsize=9)
    ax_all.grid(True, alpha=0.3)
    ax_all.axhline(y=0, color='k', linestyle='--', alpha=0.3)
    
    plt.tight_layout()
    
    # Save or show
    if output_file:
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"Saved plot to {output_file}")
    else:
        # Generate default output filename
        base_name = os.path.splitext(csv_file)[0]
        output_file = base_name + '_plot.png'
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"Saved plot to {output_file}")
    
    # Print summary statistics
    print("\n=== DFE Tap Coefficient Summary ===")
    print(f"{'Tap':<6} {'Initial':<15} {'Final':<15} {'Change':<15} {'Converged'}")
    print("-" * 70)
    for i, tap_name in enumerate(['tap1', 'tap2', 'tap3', 'tap4', 'tap5'], 1):
        initial = df[tap_name].iloc[0]
        final = df[tap_name].iloc[-1]
        change = final - initial
        # Check convergence (change less than 0.1% of final value in last quarter)
        last_quarter = df[tap_name].iloc[-len(df)//4:]
        converged = "YES" if np.std(last_quarter) < abs(final) * 0.001 else "NO"
        print(f"{i:<6} {initial:<15.6f} {final:<15.6f} {change:<15.6f} {converged}")
    
    return df

if __name__ == "__main__":
    if len(sys.argv) < 2:
        # Try to find the most recent DFE taps file
        build_dir = os.path.join(os.path.dirname(__file__), '..', 'build')
        if os.path.exists(build_dir):
            dfe_files = [f for f in os.listdir(build_dir) if f.endswith('_dfe_taps.csv')]
            if dfe_files:
                csv_file = os.path.join(build_dir, sorted(dfe_files)[-1])
                print(f"Using most recent file: {csv_file}")
            else:
                print("Usage: python plot_dfe_taps.py <dfe_taps_csv_file>")
                sys.exit(1)
        else:
            print("Usage: python plot_dfe_taps.py <dfe_taps_csv_file>")
            sys.exit(1)
    else:
        csv_file = sys.argv[1]
    
    output_file = sys.argv[2] if len(sys.argv) > 2 else None
    plot_dfe_taps(csv_file, output_file)
