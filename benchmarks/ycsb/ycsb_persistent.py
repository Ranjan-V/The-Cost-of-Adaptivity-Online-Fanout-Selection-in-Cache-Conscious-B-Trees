#!/usr/bin/env python3
"""
YCSB Benchmark - FIXED VERSION
Uses persistent database so algorithms can learn properly
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
    """Setup database ONCE"""
    print(f"\n  [Setup] Creating persistent database ({record_count} records)...")
    
    sql = f"""
    DROP TABLE IF EXISTS usertable;
    CREATE TABLE usertable (ycsb_key INTEGER PRIMARY KEY, field0 TEXT, field1 TEXT, field2 TEXT);
    BEGIN TRANSACTION;
    """
    
    for i in range(record_count):
        sql += f"INSERT INTO usertable VALUES ({i}, 'f0_{i}', 'f1_{i}', 'f2_{i}');\n"
        if (i + 1) % 1000 == 0:
            sql += "COMMIT; BEGIN TRANSACTION;\n"
    
    sql += "COMMIT;"
    
    run_sql_command(sqlite_exe, db_path, sql)
    print(f"  [Setup] ✓ Database ready (will be reused for all tests)")

def generate_zipfian(n, alpha=0.99):
    """Zipfian distribution"""
    ranks = np.arange(1, n + 1)
    weights = 1.0 / np.power(ranks, alpha)
    weights /= weights.sum()
    return weights

def generate_workload(workload_type, record_count, op_count):
    """Generate workload operations"""
    zipf = generate_zipfian(record_count)
    ops = []
    
    if workload_type == 'A':
        # 50% read, 50% update, Zipfian
        for _ in range(op_count):
            key = int(np.random.choice(record_count, p=zipf))
            if random.random() < 0.5:
                ops.append(('read', key))
            else:
                ops.append(('update', key))
    elif workload_type == 'B':
        # 95% read, 5% update, Zipfian
        for _ in range(op_count):
            key = int(np.random.choice(record_count, p=zipf))
            if random.random() < 0.95:
                ops.append(('read', key))
            else:
                ops.append(('update', key))
    else:  # C
        # 100% read, Zipfian
        for _ in range(op_count):
            key = int(np.random.choice(record_count, p=zipf))
            ops.append(('read', key))
    
    return ops

def run_benchmark(sqlite_exe, db_path, algorithm, workload_name, operations):
    """Run benchmark on PERSISTENT database"""
    print(f"\n  [Run] {workload_name} with {algorithm}...")
    
    # Build SQL script
    sql = ""
    
    # Load extension and set algorithm
    sql += ".load ./adaptive_btree\n"
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
    extension_loaded = ('"cache_hit_rate"' in stdout or algorithm in stdout)
    
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
    """Main benchmark - PERSISTENT DATABASE"""
    print("="*80)
    print("  YCSB BENCHMARK - ADAPTIVE B-TREES")
    print("  FIXED: Persistent Database + More Operations")
    print("="*80)
    
    SQLITE_EXE = '../../integration/sqlite/sqlite3.exe'
    DB_PATH = 'benchmark_persistent.db'  # SAME database for all!
    RECORD_COUNT = 10000
    OPERATION_COUNT = 20000  # Increased for better learning
    
    if not os.path.exists(SQLITE_EXE):
        print(f"\n❌ ERROR: {SQLITE_EXE} not found!")
        return
    
    # Setup database ONCE
    setup_database(SQLITE_EXE, DB_PATH, RECORD_COUNT)
    
    algorithms = [
        ('baseline', 'Baseline (No Adaptive)'),
        ('regression', 'Linear Regression'),
        ('qlearning', 'Q-Learning'),
        ('aco', 'Ant Colony Optimization'),
    ]
    
    workloads = [
        ('Workload A', 'A'),
        ('Workload B', 'B'),
        ('Workload C', 'C'),
    ]
    
    all_results = []
    
    for algo_key, algo_name in algorithms:
        print(f"\n{'='*80}")
        print(f"  TESTING: {algo_name}")
        print(f"{'='*80}")
        
        # All tests use SAME database (persistent learning!)
        for workload_name, workload_type in workloads:
            ops = generate_workload(workload_type, RECORD_COUNT, OPERATION_COUNT)
            result = run_benchmark(SQLITE_EXE, DB_PATH, algo_key, workload_name, ops)
            all_results.append(result)
    
    # Save results
    with open('ycsb_persistent_results.json', 'w') as f:
        json.dump([asdict(r) for r in all_results], f, indent=2)
    
    # Cleanup
    try:
        os.remove(DB_PATH)
    except:
        pass
    
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
    print("\n✅ Results saved to: ycsb_persistent_results.json")
    
    # Analysis
    print("\n📊 Performance Analysis:")
    for workload_name, _ in workloads:
        workload_results = [r for r in all_results if r.workload == workload_name]
        baseline = [r for r in workload_results if r.algorithm == 'baseline'][0]
        
        print(f"\n  {workload_name}:")
        for r in workload_results:
            if r.algorithm != 'baseline':
                diff = ((r.throughput - baseline.throughput) / baseline.throughput) * 100
                symbol = "✅" if diff > 0 else "⚠️"
                print(f"    {symbol} {r.algorithm:20} {diff:+6.1f}% (fanout: {r.final_fanout})")
    
    print("\n" + "="*80)

if __name__ == "__main__":
    main()