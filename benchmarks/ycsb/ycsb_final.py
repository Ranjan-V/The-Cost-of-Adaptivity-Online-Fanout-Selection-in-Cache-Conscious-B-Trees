#!/usr/bin/env python3
"""
YCSB Benchmark - Final Production Version
High operation count for proper adaptive learning
"""

import subprocess
import os
import time
import json
import random
import numpy as np
from dataclasses import dataclass, asdict

@dataclass
class BenchmarkResult:
    workload: str
    algorithm: str
    throughput: float
    avg_latency: float
    cache_hit_rate: float
    final_fanout: int
    total_ops: int
    duration: float

def run_sql_command(sqlite_exe, db_path, sql_commands):
    """Execute SQL via sqlite3.exe"""
    try:
        result = subprocess.run(
            [sqlite_exe, db_path],
            input=sql_commands,
            capture_output=True,
            text=True,
            timeout=120
        )
        return result.stdout, result.stderr
    except Exception as e:
        return "", str(e)

def setup_database(sqlite_exe, db_path, record_count):
    """Setup database with records"""
    print(f"  [Setup] Creating database with {record_count} records...")
    
    sql = """
    DROP TABLE IF EXISTS usertable;
    CREATE TABLE usertable (ycsb_key INTEGER PRIMARY KEY, field0 TEXT, field1 TEXT, field2 TEXT);
    BEGIN TRANSACTION;
    """
    
    # Add inserts
    for i in range(record_count):
        sql += f"INSERT INTO usertable VALUES ({i}, 'f0_{i}', 'f1_{i}', 'f2_{i}');\n"
        if (i + 1) % 1000 == 0:
            sql += "COMMIT; BEGIN TRANSACTION;\n"
    
    sql += "COMMIT;"
    
    run_sql_command(sqlite_exe, db_path, sql)
    print(f"  [Setup] ✓ Database ready")

def generate_zipfian(n, alpha=0.99):
    """Zipfian distribution for hot data"""
    ranks = np.arange(1, n + 1)
    weights = 1.0 / np.power(ranks, alpha)
    weights /= weights.sum()
    return weights

def generate_workload_a(record_count, op_count):
    """50% reads, 50% updates, Zipfian"""
    zipf = generate_zipfian(record_count)
    ops = []
    for _ in range(op_count):
        key = int(np.random.choice(record_count, p=zipf))
        if random.random() < 0.5:
            ops.append(('read', key))
        else:
            ops.append(('update', key))
    return ops

def generate_workload_b(record_count, op_count):
    """95% reads, 5% updates, Zipfian"""
    zipf = generate_zipfian(record_count)
    ops = []
    for _ in range(op_count):
        key = int(np.random.choice(record_count, p=zipf))
        if random.random() < 0.95:
            ops.append(('read', key))
        else:
            ops.append(('update', key))
    return ops

def generate_workload_c(record_count, op_count):
    """100% reads, Zipfian"""
    zipf = generate_zipfian(record_count)
    ops = []
    for _ in range(op_count):
        key = int(np.random.choice(record_count, p=zipf))
        ops.append(('read', key))
    return ops

def run_benchmark(sqlite_exe, db_path, algorithm, workload_name, operations):
    """Run benchmark with specific algorithm"""
    print(f"\n  [Run] {workload_name} with {algorithm}...")
    
    # Build SQL script
    sql = ""
    
    # Load extension
    sql += ".load ./adaptive_btree\n"
    
    # Set algorithm
    if algorithm != 'baseline':
        sql += f"SELECT adaptive_set_algorithm('{algorithm}');\n"
    
    # Execute operations
    sql += "BEGIN TRANSACTION;\n"
    for i, op in enumerate(operations):
        if op[0] == 'read':
            if algorithm != 'baseline':
                sql += f"SELECT adaptive_record_access({op[1]});\n"
            sql += f"SELECT * FROM usertable WHERE ycsb_key = {op[1]};\n"
        elif op[0] == 'update':
            sql += f"UPDATE usertable SET field0 = 'upd_{i}' WHERE ycsb_key = {op[1]};\n"
        
        # Commit in batches
        if (i + 1) % 100 == 0:
            sql += "COMMIT; BEGIN TRANSACTION;\n"
    
    sql += "COMMIT;\n"
    
    # Get stats
    if algorithm != 'baseline':
        sql += "SELECT adaptive_get_stats();\n"
    
    # Execute and time
    start = time.time()
    stdout, stderr = run_sql_command(sqlite_exe, db_path, sql)
    duration = time.time() - start
    
    # Parse results
    extension_loaded = ('regression' in stdout or 'qlearning' in stdout or 
                       'aco' in stdout or '"cache_hit_rate"' in stdout)
    
    cache_rate = 0.0
    final_fanout = 64
    
    if extension_loaded and '"cache_hit_rate"' in stdout:
        try:
            for line in stdout.split('\n'):
                if '"cache_hit_rate"' in line:
                    stats = json.loads(line)
                    cache_rate = stats.get('cache_hit_rate', 0.0)
                    final_fanout = stats.get('current_fanout', 64)
                    break
        except:
            pass
    
    throughput = len(operations) / duration
    avg_latency = (duration / len(operations)) * 1000
    
    result = BenchmarkResult(
        workload=workload_name,
        algorithm=algorithm,
        throughput=throughput,
        avg_latency=avg_latency,
        cache_hit_rate=cache_rate,
        final_fanout=final_fanout,
        total_ops=len(operations),
        duration=duration
    )
    
    status = "✅ Extension" if extension_loaded else "⚠️  No extension"
    print(f"  [Result] {status}")
    print(f"           Throughput:   {result.throughput:>10,.1f} ops/sec")
    print(f"           Latency:      {result.avg_latency:>10.3f} ms")
    print(f"           Cache Rate:   {result.cache_hit_rate*100:>10.1f}%")
    print(f"           Fanout:       {result.final_fanout:>10}")
    
    return result

def main():
    """Main benchmark"""
    print("="*80)
    print("  YCSB BENCHMARK - ADAPTIVE B-TREES")
    print("  Final Production Run")
    print("="*80)
    
    # Configuration
    SQLITE_EXE = '../../integration/sqlite/sqlite3.exe'
    RECORD_COUNT = 10000
    OPERATION_COUNT = 10000  # Increased for better learning
    
    # Check sqlite3.exe
    if not os.path.exists(SQLITE_EXE):
        print(f"\n❌ ERROR: {SQLITE_EXE} not found!")
        print("   Make sure sqlite3.exe is in integration/sqlite/")
        return
    
    # Algorithms to test
    algorithms = [
        ('baseline', 'Baseline (No Adaptive)'),
        ('regression', 'Linear Regression'),
        ('qlearning', 'Q-Learning'),
        ('aco', 'Ant Colony Optimization'),
    ]
    
    # Workloads
    workloads = [
        ('Workload A', generate_workload_a),
        ('Workload B', generate_workload_b),
        ('Workload C', generate_workload_c),
    ]
    
    all_results = []
    
    for algo_key, algo_name in algorithms:
        print(f"\n{'='*80}")
        print(f"  TESTING: {algo_name}")
        print(f"{'='*80}")
        
        db_path = f'benchmark_{algo_key}.db'
        
        # Setup database
        setup_database(SQLITE_EXE, db_path, RECORD_COUNT)
        
        # Run workloads
        for workload_name, workload_func in workloads:
            ops = workload_func(RECORD_COUNT, OPERATION_COUNT)
            result = run_benchmark(SQLITE_EXE, db_path, algo_key, workload_name, ops)
            all_results.append(result)
        
        # Cleanup
        try:
            os.remove(db_path)
        except:
            pass
    
    # Save results
    with open('ycsb_final_results.json', 'w') as f:
        json.dump([asdict(r) for r in all_results], f, indent=2)
    
    # Print summary
    print("\n" + "="*80)
    print("  FINAL COMPARISON SUMMARY")
    print("="*80)
    print(f"{'Workload':<15} {'Algorithm':<25} {'Throughput':>15} {'Cache%':>8} {'Fanout':>8}")
    print("-"*80)
    
    for r in all_results:
        print(f"{r.workload:<15} {r.algorithm:<25} "
              f"{r.throughput:>12,.0f} ops/s {r.cache_hit_rate*100:>7.1f}% {r.final_fanout:>8}")
    
    print("="*80)
    print("\n✅ Results saved to: ycsb_final_results.json")
    print("\n📊 Key Findings:")
    
    # Find best performers
    for workload_name, _ in workloads:
        workload_results = [r for r in all_results if r.workload == workload_name]
        best = max(workload_results, key=lambda x: x.throughput)
        baseline = [r for r in workload_results if r.algorithm == 'baseline'][0]
        
        if best.algorithm != 'baseline':
            improvement = ((best.throughput - baseline.throughput) / baseline.throughput) * 100
            print(f"   {workload_name}: {best.algorithm} is {improvement:+.1f}% faster than baseline")
    
    print("\n" + "="*80)

if __name__ == "__main__":
    main()