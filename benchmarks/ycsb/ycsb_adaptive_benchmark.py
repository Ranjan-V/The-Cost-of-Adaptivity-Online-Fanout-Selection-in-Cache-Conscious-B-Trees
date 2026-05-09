#!/usr/bin/env python3
"""
YCSB Benchmark with Adaptive B-Trees
Compares: Baseline vs RL vs Regression vs ACO
"""

import sqlite3
import random
import time
import numpy as np
import json
import os
import sys
from dataclasses import dataclass, asdict
from typing import List

# Add path to SQLite adaptive integration
sys.path.insert(0, '../../integration/sqlite')

@dataclass
class BenchmarkResult:
    workload: str
    algorithm: str
    throughput: float
    avg_latency: float
    p50_latency: float
    p95_latency: float
    p99_latency: float
    total_ops: int
    duration: float
    cache_hit_rate: float = 0.0
    final_fanout: int = 64

class YCSBWorkloadGenerator:
    """Generates YCSB-style workloads"""
    
    def __init__(self, record_count=10000, operation_count=10000):
        self.record_count = record_count
        self.operation_count = operation_count
    
    def generate_zipfian(self, n, alpha=0.99):
        """Generate Zipfian distribution (80/20 rule)"""
        ranks = np.arange(1, n + 1)
        weights = 1.0 / np.power(ranks, alpha)
        weights /= weights.sum()
        return weights
    
    def workload_a(self):
        """50% reads, 50% updates"""
        ops = []
        zipf = self.generate_zipfian(self.record_count)
        for _ in range(self.operation_count):
            key = np.random.choice(self.record_count, p=zipf)
            if random.random() < 0.5:
                ops.append(('read', key))
            else:
                ops.append(('update', key, f'updated_{time.time()}'))
        return ops
    
    def workload_b(self):
        """95% reads, 5% updates"""
        ops = []
        zipf = self.generate_zipfian(self.record_count)
        for _ in range(self.operation_count):
            key = np.random.choice(self.record_count, p=zipf)
            if random.random() < 0.95:
                ops.append(('read', key))
            else:
                ops.append(('update', key, f'updated_{time.time()}'))
        return ops
    
    def workload_c(self):
        """100% reads"""
        ops = []
        zipf = self.generate_zipfian(self.record_count)
        for _ in range(self.operation_count):
            key = np.random.choice(self.record_count, p=zipf)
            ops.append(('read', key))
        return ops

class AdaptiveSQLiteBenchmark:
    """Benchmark with adaptive integration"""
    
    def __init__(self, db_path='benchmark.db', algorithm='baseline'):
        self.db_path = db_path
        self.algorithm = algorithm
        self.latencies = []
        self.adaptive_controller = None
        
        # Try to import adaptive controller (may not exist yet)
        if algorithm != 'baseline':
            print(f"  Note: Adaptive integration not yet available")
            print(f"  Running as baseline for now")
            self.algorithm = 'baseline'
    
    def setup_database(self, record_count):
        """Initialize database"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('DROP TABLE IF EXISTS usertable')
        cursor.execute('''
            CREATE TABLE usertable (
                ycsb_key INTEGER PRIMARY KEY,
                field0 TEXT,
                field1 TEXT,
                field2 TEXT,
                field3 TEXT,
                field4 TEXT
            )
        ''')
        
        print(f"  Loading {record_count} records...")
        for i in range(record_count):
            cursor.execute('''
                INSERT INTO usertable VALUES (?, ?, ?, ?, ?, ?)
            ''', (i, f'field0_{i}', f'field1_{i}', f'field2_{i}', 
                  f'field3_{i}', f'field4_{i}'))
        
        conn.commit()
        conn.close()
    
    def run_workload(self, workload_name, operations):
        """Run workload and measure performance"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        self.latencies = []
        start_time = time.time()
        
        for i, op in enumerate(operations):
            op_start = time.time()
            
            try:
                if op[0] == 'read':
                    cursor.execute('SELECT * FROM usertable WHERE ycsb_key = ?', (op[1],))
                    _ = cursor.fetchone()
                    # Record access for adaptive controller
                    if self.adaptive_controller:
                        self.adaptive_controller.record_access(op[1])
                
                elif op[0] == 'update':
                    cursor.execute('''
                        UPDATE usertable SET field0 = ? WHERE ycsb_key = ?
                    ''', (op[2], op[1]))
                
                elif op[0] == 'scan':
                    cursor.execute('''
                        SELECT * FROM usertable WHERE ycsb_key BETWEEN ? AND ?
                    ''', (op[1], op[2]))
                    _ = cursor.fetchall()
            
            except Exception as e:
                print(f"  Error: {e}")
                continue
            
            op_latency = (time.time() - op_start) * 1000
            self.latencies.append(op_latency)
        
        conn.commit()
        conn.close()
        
        duration = time.time() - start_time
        throughput = len(operations) / duration
        
        return BenchmarkResult(
            workload=workload_name,
            algorithm=self.algorithm,
            throughput=throughput,
            avg_latency=np.mean(self.latencies),
            p50_latency=np.percentile(self.latencies, 50),
            p95_latency=np.percentile(self.latencies, 95),
            p99_latency=np.percentile(self.latencies, 99),
            total_ops=len(operations),
            duration=duration,
            cache_hit_rate=0.0,  # Will be calculated from adaptive controller
            final_fanout=64
        )

def run_comparison_benchmark():
    """Run all algorithms and compare"""
    print("="*70)
    print("  YCSB Benchmark - Algorithm Comparison")
    print("  Baseline vs RL vs Regression vs ACO")
    print("="*70)
    
    RECORD_COUNT = 10000
    OPERATION_COUNT = 10000
    
    algorithms = [
        ('Baseline', 'baseline'),
        # These will be enabled when adaptive integration is ready:
        # ('Q-Learning', 'qlearning'),
        # ('Linear Regression', 'regression'),
        # ('Ant Colony', 'aco'),
    ]
    
    workloads = [
        ('Workload A (50% R, 50% U)', 'workload_a'),
        ('Workload B (95% R, 5% U)', 'workload_b'),
        ('Workload C (100% R)', 'workload_c'),
    ]
    
    all_results = []
    generator = YCSBWorkloadGenerator(RECORD_COUNT, OPERATION_COUNT)
    
    for algo_name, algo_type in algorithms:
        print(f"\n{'='*70}")
        print(f"  Testing: {algo_name}")
        print(f"{'='*70}")
        
        benchmark = AdaptiveSQLiteBenchmark(f'benchmark_{algo_type}.db', algo_type)
        benchmark.setup_database(RECORD_COUNT)
        
        for workload_name, workload_func in workloads:
            print(f"\n  Running {workload_name}...")
            ops = getattr(generator, workload_func)()
            result = benchmark.run_workload(workload_name, ops)
            all_results.append(result)
            
            print(f"    Throughput:  {result.throughput:>12,.2f} ops/sec")
            print(f"    Avg Latency: {result.avg_latency:>12.3f} ms")
            print(f"    P95 Latency: {result.p95_latency:>12.3f} ms")
        
        # Cleanup
        if os.path.exists(benchmark.db_path):
            os.remove(benchmark.db_path)
    
    # Save results
    results_dict = [asdict(r) for r in all_results]
    with open('ycsb_comparison_results.json', 'w') as f:
        json.dump(results_dict, f, indent=2)
    
    # Print comparison table
    print("\n" + "="*70)
    print("  COMPARISON SUMMARY")
    print("="*70)
    print(f"{'Workload':<25} {'Algorithm':<15} {'Throughput':>15} {'Avg Latency':>12}")
    print("-"*70)
    
    for result in all_results:
        print(f"{result.workload:<25} {result.algorithm:<15} "
              f"{result.throughput:>12,.0f} ops/s {result.avg_latency:>9.3f} ms")
    
    print("="*70)
    print(f"\nResults saved to: ycsb_comparison_results.json")
    print("\nNext step: Integrate adaptive algorithms and re-run!")
    print("="*70)

if __name__ == "__main__":
    run_comparison_benchmark()