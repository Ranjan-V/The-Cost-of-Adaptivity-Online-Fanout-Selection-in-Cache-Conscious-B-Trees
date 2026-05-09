#!/usr/bin/env python3
"""
YCSB-Style Benchmark Suite for SQLite Adaptive B-Trees
Generates workloads and measures performance
"""

import sqlite3
import random
import time
import numpy as np
from dataclasses import dataclass
from typing import List, Tuple
import json

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
    
    def workload_d(self):
        """95% reads (latest), 5% inserts"""
        ops = []
        latest_key = self.record_count
        
        for _ in range(self.operation_count):
            if random.random() < 0.95:
                # Read recent keys
                key = max(0, latest_key - random.randint(0, 100))
                ops.append(('read', key))
            else:
                # Insert new key
                ops.append(('insert', latest_key, f'data_{latest_key}'))
                latest_key += 1
        return ops
    
    def workload_e(self):
        """95% scans, 5% inserts"""
        ops = []
        latest_key = self.record_count
        
        for _ in range(self.operation_count):
            if random.random() < 0.95:
                start = random.randint(0, self.record_count - 100)
                ops.append(('scan', start, start + 100))
            else:
                ops.append(('insert', latest_key, f'data_{latest_key}'))
                latest_key += 1
        return ops
    
    def workload_f(self):
        """100% read-modify-write"""
        ops = []
        zipf = self.generate_zipfian(self.record_count)
        
        for _ in range(self.operation_count):
            key = np.random.choice(self.record_count, p=zipf)
            ops.append(('read', key))
            ops.append(('update', key, f'modified_{time.time()}'))
        return ops

class SQLiteBenchmark:
    """Benchmark SQLite with different workloads"""
    
    def __init__(self, db_path='benchmark.db'):
        self.db_path = db_path
        self.latencies = []
    
    def setup_database(self, record_count):
        """Initialize database with data"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        # Create table
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
        
        # Insert initial data
        print(f"Loading {record_count} records...")
        for i in range(record_count):
            cursor.execute('''
                INSERT INTO usertable VALUES (?, ?, ?, ?, ?, ?)
            ''', (i, f'field0_{i}', f'field1_{i}', f'field2_{i}', 
                  f'field3_{i}', f'field4_{i}'))
            
            if (i + 1) % 1000 == 0:
                print(f"  Loaded {i + 1}/{record_count} records")
        
        conn.commit()
        conn.close()
        print("Database setup complete!")
    
    def run_workload(self, workload_name, operations):
        """Run a workload and measure performance"""
        print(f"\nRunning {workload_name}...")
        
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
                
                elif op[0] == 'update':
                    cursor.execute('''
                        UPDATE usertable SET field0 = ? WHERE ycsb_key = ?
                    ''', (op[2], op[1]))
                
                elif op[0] == 'insert':
                    cursor.execute('''
                        INSERT OR REPLACE INTO usertable VALUES (?, ?, ?, ?, ?, ?)
                    ''', (op[1], op[2], op[2], op[2], op[2], op[2]))
                
                elif op[0] == 'scan':
                    cursor.execute('''
                        SELECT * FROM usertable WHERE ycsb_key BETWEEN ? AND ?
                    ''', (op[1], op[2]))
                    _ = cursor.fetchall()
            
            except Exception as e:
                print(f"Error on operation {i}: {e}")
                continue
            
            op_latency = (time.time() - op_start) * 1000  # ms
            self.latencies.append(op_latency)
            
            if (i + 1) % 1000 == 0:
                print(f"  Completed {i + 1}/{len(operations)} operations")
        
        conn.commit()
        conn.close()
        
        duration = time.time() - start_time
        throughput = len(operations) / duration
        
        return BenchmarkResult(
            workload=workload_name,
            algorithm="Baseline",  # Will be set by caller
            throughput=throughput,
            avg_latency=np.mean(self.latencies),
            p50_latency=np.percentile(self.latencies, 50),
            p95_latency=np.percentile(self.latencies, 95),
            p99_latency=np.percentile(self.latencies, 99),
            total_ops=len(operations),
            duration=duration
        )
    
    def cleanup(self):
        """Remove benchmark database"""
        import os
        if os.path.exists(self.db_path):
            os.remove(self.db_path)

def run_all_benchmarks():
    """Run all YCSB workloads"""
    print("="*70)
    print("  YCSB Benchmark Suite - SQLite Adaptive B-Trees")
    print("="*70)
    
    # Configuration
    RECORD_COUNT = 10000
    OPERATION_COUNT = 10000
    
    generator = YCSBWorkloadGenerator(RECORD_COUNT, OPERATION_COUNT)
    benchmark = SQLiteBenchmark('benchmark.db')
    
    # Setup database
    benchmark.setup_database(RECORD_COUNT)
    
    workloads = [
        ('Workload A (50% R, 50% U)', generator.workload_a()),
        ('Workload B (95% R, 5% U)', generator.workload_b()),
        ('Workload C (100% R)', generator.workload_c()),
        ('Workload D (95% R, 5% I)', generator.workload_d()),
        ('Workload E (95% Scan, 5% I)', generator.workload_e()),
        ('Workload F (RMW)', generator.workload_f()),
    ]
    
    results = []
    
    for name, ops in workloads:
        result = benchmark.run_workload(name, ops)
        results.append(result)
        
        print(f"\n  Results for {name}:")
        print(f"    Throughput:    {result.throughput:.2f} ops/sec")
        print(f"    Avg Latency:   {result.avg_latency:.3f} ms")
        print(f"    P50 Latency:   {result.p50_latency:.3f} ms")
        print(f"    P95 Latency:   {result.p95_latency:.3f} ms")
        print(f"    P99 Latency:   {result.p99_latency:.3f} ms")
    
    # Save results
    results_data = []
    for r in results:
        results_data.append({
            'workload': r.workload,
            'throughput': r.throughput,
            'avg_latency': r.avg_latency,
            'p50_latency': r.p50_latency,
            'p95_latency': r.p95_latency,
            'p99_latency': r.p99_latency,
        })
    
    with open('ycsb_results.json', 'w') as f:
        json.dump(results_data, f, indent=2)
    
    print("\n" + "="*70)
    print("  Benchmark Complete!")
    print("  Results saved to: ycsb_results.json")
    print("="*70)
    
    benchmark.cleanup()

if __name__ == "__main__":
    run_all_benchmarks()