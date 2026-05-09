#!/usr/bin/env python3
"""
YCSB Benchmark with Full SQLite Adaptive Integration
Tests all 3 algorithms: RL, Regression, ACO
Uses SQLite extension for true integration
"""

import sqlite3
import random
import time
import numpy as np
import json
from dataclasses import dataclass, asdict
from typing import List

@dataclass
class BenchmarkResult:
    workload: str
    algorithm: str
    throughput: float
    avg_latency: float
    p50_latency: float
    p95_latency: float
    p99_latency: float
    cache_hit_rate: float
    final_fanout: int
    total_ops: int
    duration: float

class AdaptiveYCSBBenchmark:
    """YCSB benchmark with adaptive extension"""
    
    def __init__(self, db_path='benchmark.db'):
        self.db_path = db_path
        self.extension_loaded = False
    
    def load_extension(self, conn):
        """Load adaptive extension"""
        try:
            conn.enable_load_extension(True)
            conn.load_extension('./adaptive_btree')
            self.extension_loaded = True
            print("  ✅ Adaptive extension loaded")
            return True
        except Exception as e:
            print(f"  ⚠️  Extension not available: {e}")
            print("  ℹ️  Running without adaptive integration")
            return False
    
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
                field2 TEXT
            )
        ''')
        
        for i in range(record_count):
            cursor.execute('''
                INSERT INTO usertable VALUES (?, ?, ?, ?)
            ''', (i, f'field0_{i}', f'field1_{i}', f'field2_{i}'))
        
        conn.commit()
        conn.close()
    
    def run_workload(self, algorithm_name, workload_name, operations):
        """Run workload with specific algorithm"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        # Load extension and set algorithm
        extension_available = self.load_extension(conn)
        
        if extension_available:
            cursor.execute(f"SELECT adaptive_set_algorithm('{algorithm_name}')")
            print(f"  ✅ Algorithm set to: {algorithm_name}")
        
        latencies = []
        start_time = time.time()
        
        for op in operations:
            op_start = time.time()
            
            try:
                if op[0] == 'read':
                    # Record access for adaptive
                    if extension_available:
                        cursor.execute('SELECT adaptive_record_access(?)', (op[1],))
                    
                    cursor.execute('SELECT * FROM usertable WHERE ycsb_key = ?', (op[1],))
                    _ = cursor.fetchone()
                
                elif op[0] == 'update':
                    cursor.execute('''
                        UPDATE usertable SET field0 = ? WHERE ycsb_key = ?
                    ''', (op[2], op[1]))
            
            except Exception as e:
                continue
            
            latencies.append((time.time() - op_start) * 1000)
        
        conn.commit()
        
        # Get adaptive stats
        cache_rate = 0.0
        final_fanout = 64
        
        if extension_available:
            try:
                cursor.execute('SELECT adaptive_get_stats()')
                stats_json = cursor.fetchone()[0]
                stats = json.loads(stats_json)
                cache_rate = stats.get('cache_hit_rate', 0.0)
                final_fanout = stats.get('current_fanout', 64)
            except:
                pass
        
        conn.close()
        
        duration = time.time() - start_time
        throughput = len(operations) / duration
        
        return BenchmarkResult(
            workload=workload_name,
            algorithm=algorithm_name,
            throughput=throughput,
            avg_latency=np.mean(latencies),
            p50_latency=np.percentile(latencies, 50),
            p95_latency=np.percentile(latencies, 95),
            p99_latency=np.percentile(latencies, 99),
            cache_hit_rate=cache_rate,
            final_fanout=final_fanout,
            total_ops=len(operations),
            duration=duration
        )

def generate_zipfian(n, alpha=0.99):
    """Generate Zipfian distribution"""
    ranks = np.arange(1, n + 1)
    weights = 1.0 / np.power(ranks, alpha)
    weights /= weights.sum()
    return weights

def workload_a(record_count, op_count):
    """50% reads, 50% updates"""
    ops = []
    zipf = generate_zipfian(record_count)
    for _ in range(op_count):
        key = np.random.choice(record_count, p=zipf)
        if random.random() < 0.5:
            ops.append(('read', key))
        else:
            ops.append(('update', key, f'updated_{time.time()}'))
    return ops

def workload_b(record_count, op_count):
    """95% reads, 5% updates"""
    ops = []
    zipf = generate_zipfian(record_count)
    for _ in range(op_count):
        key = np.random.choice(record_count, p=zipf)
        if random.random() < 0.95:
            ops.append(('read', key))
        else:
            ops.append(('update', key, f'updated_{time.time()}'))
    return ops

def workload_c(record_count, op_count):
    """100% reads"""
    ops = []
    zipf = generate_zipfian(record_count)
    for _ in range(op_count):
        key = np.random.choice(record_count, p=zipf)
        ops.append(('read', key))
    return ops

def run_full_comparison():
    """Run complete comparison benchmark"""
    print("="*80)
    print("  YCSB BENCHMARK - FULL ADAPTIVE INTEGRATION")
    print("  Testing: Baseline vs Q-Learning vs Regression vs ACO")
    print("="*80)
    
    RECORD_COUNT = 10000
    OPERATION_COUNT = 10000
    
    algorithms = [
        ('baseline', 'Baseline (No Adaptive)'),
        ('qlearning', 'Q-Learning (RL)'),
        ('regression', 'Linear Regression'),
        ('aco', 'Ant Colony Optimization'),
    ]
    
    workloads = [
        ('Workload A', workload_a),
        ('Workload B', workload_b),
        ('Workload C', workload_c),
    ]
    
    all_results = []
    
    for algo_key, algo_name in algorithms:
        print(f"\n{'='*80}")
        print(f"  Testing: {algo_name}")
        print(f"{'='*80}")
        
        benchmark = AdaptiveYCSBBenchmark(f'benchmark_{algo_key}.db')
        print(f"\n  Setting up database ({RECORD_COUNT} records)...")
        benchmark.setup_database(RECORD_COUNT)
        
        for workload_name, workload_func in workloads:
            print(f"\n  Running {workload_name}...")
            ops = workload_func(RECORD_COUNT, OPERATION_COUNT)
            result = benchmark.run_workload(algo_key, workload_name, ops)
            all_results.append(result)
            
            print(f"    Throughput:     {result.throughput:>15,.2f} ops/sec")
            print(f"    Avg Latency:    {result.avg_latency:>15.3f} ms")
            print(f"    P95 Latency:    {result.p95_latency:>15.3f} ms")
            print(f"    Cache Hit Rate: {result.cache_hit_rate*100:>15.2f}%")
            print(f"    Final Fanout:   {result.final_fanout:>15}")
    
    # Save results
    with open('ycsb_full_comparison.json', 'w') as f:
        json.dump([asdict(r) for r in all_results], f, indent=2)
    
    # Print comparison table
    print("\n" + "="*80)
    print("  COMPLETE COMPARISON SUMMARY")
    print("="*80)
    print(f"{'Workload':<15} {'Algorithm':<25} {'Throughput':>15} {'Cache %':>10} {'Fanout':>8}")
    print("-"*80)
    
    for r in all_results:
        print(f"{r.workload:<15} {r.algorithm:<25} {r.throughput:>12,.0f} ops/s "
              f"{r.cache_hit_rate*100:>9.1f}% {r.final_fanout:>8}")
    
    print("="*80)
    print("\nResults saved to: ycsb_full_comparison.json")
    print("="*80)

if __name__ == "__main__":
    run_full_comparison()