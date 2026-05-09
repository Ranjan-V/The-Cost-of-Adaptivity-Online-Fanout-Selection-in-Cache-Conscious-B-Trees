#!/usr/bin/env python3
"""
Direct test of adaptive extension - debug version
Tests if fanout actually changes with repeated hot accesses
"""

import subprocess
import time

def run_sql(db, sql):
    """Execute SQL and return output"""
    result = subprocess.run(
        ['../../integration/sqlite/sqlite3.exe', db],
        input=sql,
        capture_output=True,
        text=True,
        timeout=30
    )
    return result.stdout

def test_adaptive():
    """Test adaptive behavior with controlled access pattern"""
    
    db = 'test_adaptive.db'
    
    print("="*80)
    print("  ADAPTIVE BEHAVIOR DEBUG TEST")
    print("="*80)
    
    # Setup
    print("\n[1] Setting up database...")
    sql = """
    DROP TABLE IF EXISTS test;
    CREATE TABLE test (id INTEGER PRIMARY KEY, data TEXT);
    BEGIN;
    """
    for i in range(100):
        sql += f"INSERT INTO test VALUES ({i}, 'data_{i}');\n"
    sql += "COMMIT;\n"
    run_sql(db, sql)
    print("    ✓ 100 records created")
    
    # Load extension
    print("\n[2] Loading adaptive extension...")
    sql = ".load ./adaptive_btree\n"
    sql += "SELECT adaptive_set_algorithm('regression');\n"
    output = run_sql(db, sql)
    print(f"    Output: {output.strip()}")
    
    # Initial stats
    print("\n[3] Initial stats:")
    sql = ".load ./adaptive_btree\nSELECT adaptive_get_stats();\n"
    output = run_sql(db, sql)
    print(f"    {output.strip()}")
    
    # Access pattern 1: Hit keys 0-9 repeatedly (HOT DATA)
    print("\n[4] Accessing keys 0-9 repeatedly (1000 times - HOT!)...")
    sql = ".load ./adaptive_btree\n"
    sql += "SELECT adaptive_set_algorithm('regression');\n"
    for _ in range(100):
        for key in range(10):  # Keys 0-9 (10% of data, 100% of accesses)
            sql += f"SELECT adaptive_record_access({key});\n"
            sql += f"SELECT * FROM test WHERE id = {key};\n"
    sql += "SELECT adaptive_get_stats();\n"
    
    start = time.time()
    output = run_sql(db, sql)
    duration = time.time() - start
    
    # Extract stats from output
    lines = output.split('\n')
    for line in lines:
        if '"cache_hit_rate"' in line or '"current_fanout"' in line:
            print(f"    {line}")
    
    print(f"\n    Duration: {duration:.2f}s")
    print(f"    Operations: 1000 accesses to 10 hot keys")
    print(f"    Expected: Fanout should DECREASE (64 → 32 → 16 → 8)")
    
    # Access pattern 2: Random cold accesses
    print("\n[5] Accessing keys 50-99 (COLD DATA)...")
    sql = ".load ./adaptive_btree\n"
    sql += "SELECT adaptive_set_algorithm('regression');\n"
    for key in range(50, 100):
        sql += f"SELECT adaptive_record_access({key});\n"
        sql += f"SELECT * FROM test WHERE id = {key};\n"
    sql += "SELECT adaptive_get_stats();\n"
    
    output = run_sql(db, sql)
    for line in output.split('\n'):
        if '"cache_hit_rate"' in line or '"current_fanout"' in line:
            print(f"    {line}")
    
    print(f"\n    Expected: Fanout should INCREASE (back toward 64-128)")
    
    print("\n" + "="*80)
    print("  TEST COMPLETE")
    print("="*80)
    print("\nAnalysis:")
    print("  If fanout changed → Algorithm is working ✅")
    print("  If fanout stuck at 64 or 128 → Algorithm not adapting ❌")
    print("="*80)

if __name__ == "__main__":
    test_adaptive()