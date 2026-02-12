#!/usr/bin/env python3
"""
Generate eye diagram for DFE output
"""
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from eye_analyzer import analyze_eye
import pandas as pd
import numpy as np

def main():
    # DFE output file path
    dfe_file = 'build/serdes_link_basic_dfe.csv'
    output_dir = 'output_eye'
    
    # Check if file exists
    if not os.path.exists(dfe_file):
        print(f"Error: {dfe_file} not found!")
        print("Please run the simulation first: ./tb/serdes_link_tb")
        return 1
    
    print("="*60)
    print("DFE Output Eye Diagram Analysis")
    print("="*60)
    print(f"Input file: {dfe_file}")
    
    # Load waveform data
    print("\nLoading waveform data...")
    df = pd.read_csv(dfe_file)
    time_array = df['time_s'].values
    voltage_array = df['voltage_v'].values
    
    print(f"  Total samples: {len(time_array)}")
    print(f"  Time range: {time_array[0]:.3e} s to {time_array[-1]:.3e} s")
    print(f"  Duration: {time_array[-1] - time_array[0]:.3e} s")
    print(f"  Voltage range: {voltage_array.min():.3f} V to {voltage_array.max():.3f} V")
    
    # UI for 10 Gbps = 100 ps
    ui = 100e-12  # 100 ps
    
    print(f"\nAnalyzing eye diagram...")
    print(f"  UI: {ui*1e12:.1f} ps ({1/ui/1e9:.1f} Gbps)")
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Analyze eye diagram
    metrics = analyze_eye(
        waveform_array=np.column_stack((time_array, voltage_array)),
        ui=ui,
        ui_bins=128,
        amp_bins=128,
        measure_length=None,  # Use all data
        target_ber=1e-12,
        sampling='phase-lock',
        jitter_method='dual-dirac',
        output_image_format='png',
        output_image_dpi=300
    )
    
    # Save results
    from eye_analyzer import EyeAnalyzer
    analyzer = EyeAnalyzer(ui=ui, ui_bins=128, amp_bins=128)
    analyzer._dat_path = dfe_file
    analyzer.analyze(time_array, voltage_array, target_ber=1e-12)
    analyzer.save_results(metrics, output_dir=output_dir)
    
    print("\n" + "="*60)
    print("Analysis Complete!")
    print("="*60)
    print(f"\nEye Metrics:")
    print(f"  Eye Height:     {metrics['eye_height']*1000:.2f} mV")
    print(f"  Eye Width:      {metrics['eye_width']:.3f} UI ({metrics['eye_width']*ui*1e12:.2f} ps)")
    print(f"  Eye Area:       {metrics['eye_area']*1000:.2f} mV*UI")
    print(f"\nJitter Metrics:")
    print(f"  RJ (sigma):     {metrics['rj_sigma']*1e12:.2f} ps")
    print(f"  DJ (pp):        {metrics['dj_pp']*1e12:.2f} ps")
    print(f"  TJ @ 1e-12:     {metrics['tj_at_ber']*1e12:.2f} ps")
    print(f"\nSignal Quality:")
    print(f"  Signal Mean:    {metrics['signal_mean']*1000:.2f} mV")
    print(f"  Signal RMS:     {metrics['signal_rms']*1000:.2f} mV")
    print(f"  Peak-to-peak:   {metrics['signal_peak_to_peak']*1000:.2f} mV")
    
    print(f"\nOutput files saved to: {output_dir}/")
    print(f"  - eye_diagram.png")
    print(f"  - eye_metrics.json")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
