#!/usr/bin/env python3
"""
Performance benchmark script for channel processing methods.
Compares computational complexity and execution time of:
1. Vector Fitting (various orders)
2. Impulse Response (various lengths)
3. Direct convolution vs FFT convolution
"""

import argparse
import json
import logging
import time
import sys
from pathlib import Path

import numpy as np

# Add scripts directory to path
sys.path.insert(0, str(Path(__file__).parent))

from vector_fitting import VectorFitting
from process_sparam import ImpulseResponseGenerator

logger = logging.getLogger(__name__)


class Benchmark:
    """Benchmark utility class."""
    
    def __init__(self, name):
        self.name = name
        self.results = []
        
    def run(self, func, *args, n_runs=5, **kwargs):
        """Run benchmark multiple times and collect statistics."""
        times = []
        for _ in range(n_runs):
            start = time.perf_counter()
            result = func(*args, **kwargs)
            end = time.perf_counter()
            times.append(end - start)
        
        stats = {
            'mean': np.mean(times),
            'std': np.std(times),
            'min': np.min(times),
            'max': np.max(times)
        }
        
        self.results.append({
            'name': self.name,
            **stats
        })
        
        return result, stats


def generate_test_data(n_freq, f_max):
    """Generate synthetic test data."""
    freq = np.linspace(1e6, f_max, n_freq)
    
    # Simple channel model
    loss_db = 15.0 * np.sqrt(freq / f_max)
    loss_linear = 10 ** (-loss_db / 20)
    delay_s = 100e-12
    phase = -2 * np.pi * freq * delay_s
    
    S21 = loss_linear * np.exp(1j * phase)
    
    return freq, S21


def benchmark_vector_fitting(freq, S_data, orders=[4, 6, 8, 10, 12, 14, 16]):
    """Benchmark vector fitting with different orders."""
    print("\n" + "=" * 60)
    print("Vector Fitting Benchmark")
    print("=" * 60)
    
    results = []
    
    for order in orders:
        bench = Benchmark(f"VF_order_{order}")
        
        def fit_func():
            vf = VectorFitting(order=order, max_iterations=10)
            return vf.fit(freq, S_data, enforce_stability=True)
        
        result, stats = bench.run(fit_func, n_runs=3)
        
        results.append({
            'order': order,
            'time_ms': stats['mean'] * 1000,
            'time_std_ms': stats['std'] * 1000,
            'mse': result['mse'],
            'max_error': result['max_error']
        })
        
        print(f"Order {order:2d}: {stats['mean']*1000:8.2f} ms "
              f"(±{stats['std']*1000:.2f}), MSE={result['mse']:.2e}")
    
    return results


def benchmark_impulse_response(freq, S_data, fs=100e9, 
                                sample_counts=[512, 1024, 2048, 4096, 8192]):
    """Benchmark impulse response generation with different sample counts."""
    print("\n" + "=" * 60)
    print("Impulse Response Generation Benchmark")
    print("=" * 60)
    
    results = []
    
    for n_samples in sample_counts:
        bench = Benchmark(f"IR_samples_{n_samples}")
        
        def gen_func():
            ir_gen = ImpulseResponseGenerator(fs, n_samples)
            return ir_gen.generate(freq, S_data)
        
        result, stats = bench.run(gen_func, n_runs=3)
        
        results.append({
            'n_samples': n_samples,
            'time_ms': stats['mean'] * 1000,
            'time_std_ms': stats['std'] * 1000,
            'truncated_length': result['length'],
            'energy': result['energy']
        })
        
        print(f"N={n_samples:5d}: {stats['mean']*1000:8.2f} ms "
              f"(±{stats['std']*1000:.2f}), length={result['length']}")
    
    return results


def benchmark_convolution(impulse_lengths=[64, 128, 256, 512, 1024, 2048],
                          n_samples=10000):
    """Benchmark direct convolution vs FFT convolution."""
    print("\n" + "=" * 60)
    print("Convolution Method Benchmark")
    print("=" * 60)
    
    results = []
    
    for L in impulse_lengths:
        # Generate random impulse response and input
        h = np.random.randn(L) * 0.1
        x = np.random.randn(n_samples)
        
        # Direct convolution benchmark
        def direct_conv():
            y = np.zeros(n_samples)
            for n in range(n_samples):
                for k in range(min(L, n + 1)):
                    y[n] += h[k] * x[n - k]
            return y
        
        # FFT convolution benchmark
        def fft_conv():
            return np.convolve(x, h, mode='same')
        
        # Time direct convolution
        if L <= 512:  # Only run direct for reasonable sizes
            bench_direct = Benchmark(f"Direct_L_{L}")
            _, stats_direct = bench_direct.run(direct_conv, n_runs=1)
            time_direct = stats_direct['mean']
        else:
            time_direct = float('inf')
        
        # Time FFT convolution
        bench_fft = Benchmark(f"FFT_L_{L}")
        _, stats_fft = bench_fft.run(fft_conv, n_runs=3)
        time_fft = stats_fft['mean']
        
        speedup = time_direct / time_fft if time_direct != float('inf') else float('nan')
        
        results.append({
            'impulse_length': L,
            'direct_time_ms': time_direct * 1000 if time_direct != float('inf') else None,
            'fft_time_ms': time_fft * 1000,
            'speedup': speedup
        })
        
        direct_str = f"{time_direct*1000:8.2f}" if time_direct != float('inf') else "    N/A "
        print(f"L={L:5d}: Direct={direct_str} ms, FFT={time_fft*1000:8.2f} ms, "
              f"Speedup={speedup:.1f}x" if not np.isnan(speedup) else "")
    
    return results


def benchmark_online_processing(impulse_lengths=[256, 512, 1024, 2048],
                                 n_samples=100000):
    """Benchmark online (sample-by-sample) processing."""
    print("\n" + "=" * 60)
    print("Online Processing Benchmark (sample-by-sample)")
    print("=" * 60)
    
    results = []
    
    for L in impulse_lengths:
        # Generate impulse response
        h = np.random.randn(L) * 0.1
        
        # Circular buffer implementation
        def online_conv():
            buffer = np.zeros(L)
            buf_idx = 0
            y_sum = 0.0
            
            for n in range(n_samples):
                x_new = np.random.randn()
                buffer[buf_idx] = x_new
                
                y = 0.0
                for k in range(L):
                    buf_pos = (buf_idx - k + L) % L
                    y += h[k] * buffer[buf_pos]
                
                buf_idx = (buf_idx + 1) % L
                y_sum += y
            
            return y_sum
        
        bench = Benchmark(f"Online_L_{L}")
        _, stats = bench.run(online_conv, n_runs=1)
        
        samples_per_sec = n_samples / stats['mean']
        
        results.append({
            'impulse_length': L,
            'time_s': stats['mean'],
            'samples_per_sec': samples_per_sec,
            'complexity': 'O(L)'
        })
        
        print(f"L={L:5d}: {stats['mean']:.3f} s, "
              f"{samples_per_sec/1e6:.2f} MSamples/s")
    
    return results


def run_all_benchmarks(output_file=None):
    """Run all benchmarks and optionally save results."""
    print("=" * 60)
    print("Channel Processing Performance Benchmarks")
    print("=" * 60)
    
    # Generate test data
    print("\nGenerating test data...")
    freq, S_data = generate_test_data(n_freq=1001, f_max=50e9)
    print(f"Test data: {len(freq)} frequency points, 1 MHz - 50 GHz")
    
    # Run benchmarks
    all_results = {
        'vector_fitting': benchmark_vector_fitting(freq, S_data),
        'impulse_response': benchmark_impulse_response(freq, S_data),
        'convolution_comparison': benchmark_convolution(),
        'online_processing': benchmark_online_processing()
    }
    
    # Summary
    print("\n" + "=" * 60)
    print("Summary")
    print("=" * 60)
    
    # Best VF configuration
    vf_results = all_results['vector_fitting']
    best_vf = min(vf_results, key=lambda x: x['mse'])
    print(f"Best VF: order={best_vf['order']}, "
          f"MSE={best_vf['mse']:.2e}, time={best_vf['time_ms']:.2f} ms")
    
    # Recommended IR configuration
    ir_results = all_results['impulse_response']
    recommended_ir = ir_results[2]  # 2048 samples typically good balance
    print(f"Recommended IR: N={recommended_ir['n_samples']}, "
          f"length={recommended_ir['truncated_length']}, "
          f"time={recommended_ir['time_ms']:.2f} ms")
    
    # FFT crossover point
    conv_results = all_results['convolution_comparison']
    for r in conv_results:
        if r['speedup'] and r['speedup'] > 1.0:
            print(f"FFT faster than direct for L > {r['impulse_length']//2}")
            break
    
    # Save results if requested
    if output_file:
        with open(output_file, 'w') as f:
            json.dump(all_results, f, indent=2, default=float)
        print(f"\nResults saved to: {output_file}")
    
    return all_results


def main():
    """Command-line interface."""
    parser = argparse.ArgumentParser(
        description='Performance benchmarks for channel processing'
    )
    parser.add_argument('-o', '--output', help='Output JSON file for results')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Enable verbose output')
    
    args = parser.parse_args()
    
    # Configure logging
    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=level, format='%(levelname)s: %(message)s')
    
    try:
        results = run_all_benchmarks(args.output)
        return 0
    except Exception as e:
        logger.error(f"Benchmark failed: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
