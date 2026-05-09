#!/usr/bin/env python3
"""
Database Integration Checkpoint Script
Verifies SQLite and PostgreSQL setup
"""

import os
import sys
from pathlib import Path

def check_file(filepath, description):
    """Check if file exists"""
    if Path(filepath).exists():
        size = Path(filepath).stat().st_size / (1024 * 1024)  # MB
        print(f"  ✅ {description}: {filepath} ({size:.2f} MB)")
        return True
    else:
        print(f"  ❌ {description}: {filepath} NOT FOUND")
        return False

def check_dir(dirpath, description):
    """Check if directory exists"""
    if Path(dirpath).exists() and Path(dirpath).is_dir():
        print(f"  ✅ {description}: {dirpath}")
        return True
    else:
        print(f"  ❌ {description}: {dirpath} NOT FOUND")
        return False

def check_sqlite():
    """Check SQLite setup"""
    print("\n" + "="*60)
    print("SQLite Integration Checkpoint")
    print("="*60)
    
    base = Path("integration/sqlite")
    all_good = True
    
    print("\n📁 Directory Structure:")
    all_good &= check_dir(base, "SQLite base directory")
    all_good &= check_dir(base / "sqlite-amalgamation-3510200", "SQLite source")
    all_good &= check_dir(base / "adaptive", "Adaptive headers")
    
    print("\n📄 SQLite Source Files:")
    sqlite_dir = base / "sqlite-amalgamation-3510200"
    all_good &= check_file(sqlite_dir / "sqlite3.c", "SQLite core")
    all_good &= check_file(sqlite_dir / "sqlite3.h", "SQLite header")
    all_good &= check_file(sqlite_dir / "shell.c", "SQLite shell")
    
    print("\n📄 Adaptive Headers:")
    adaptive_dir = base / "adaptive"
    all_good &= check_file(adaptive_dir / "monitor.h", "Access Monitor")
    all_good &= check_file(adaptive_dir / "predictor.h", "Fanout Predictor")
    all_good &= check_file(adaptive_dir / "adaptive_btree.h", "RL Algorithm")
    all_good &= check_file(adaptive_dir / "adaptive_btree_regression.h", "Regression")
    all_good &= check_file(adaptive_dir / "adaptive_btree_aco.h", "ACO Algorithm")
    
    print("\n📄 Build Scripts:")
    all_good &= check_file(base / "buildsqlite.bat", "Build script")
    
    print("\n" + "="*60)
    if all_good:
        print("✅ SQLite: READY TO BUILD!")
        print("="*60)
        print("\nNext step:")
        print("  cd integration\\sqlite")
        print("  .\\buildsqlite.bat")
    else:
        print("❌ SQLite: INCOMPLETE SETUP")
        print("="*60)
        print("\nMissing items above need to be fixed!")
    
    return all_good

def check_postgresql():
    """Check PostgreSQL setup"""
    print("\n" + "="*60)
    print("PostgreSQL Integration Checkpoint")
    print("="*60)
    
    base = Path("integration/postgresql")
    all_good = True
    
    print("\n📁 Directory Structure:")
    all_good &= check_dir(base, "PostgreSQL base directory")
    all_good &= check_dir(base / "postgresql-18.2", "PostgreSQL source")
    
    print("\n📄 PostgreSQL B-tree Files:")
    nbtree = base / "postgresql-18.2" / "src" / "backend" / "access" / "nbtree"
    if check_dir(nbtree, "B-tree source directory"):
        btree_files = [
            "nbtree.c", "nbtsearch.c", "nbtinsert.c", 
            "nbtpage.c", "nbtutils.c", "nbtsort.c"
        ]
        for f in btree_files:
            all_good &= check_file(nbtree / f, f)
    else:
        all_good = False
    
    print("\n📄 PostgreSQL Headers:")
    include = base / "postgresql-18.2" / "src" / "include" / "access"
    all_good &= check_file(include / "nbtree.h", "B-tree header")
    
    print("\n" + "="*60)
    if all_good:
        print("✅ PostgreSQL: SOURCE READY!")
        print("="*60)
        print("\nPostgreSQL options:")
        print("  1. Use binary installation (recommended for testing)")
        print("  2. Build from source (for paper - later)")
    else:
        print("❌ PostgreSQL: INCOMPLETE SETUP")
        print("="*60)
    
    return all_good

def check_core_code():
    """Check core implementation"""
    print("\n" + "="*60)
    print("Core Implementation Checkpoint")
    print("="*60)
    
    all_good = True
    
    print("\n📄 Headers:")
    headers = [
        "include/btree/btree.h",
        "include/monitor/monitor.h",
        "include/predictor/predictor.h",
        "include/adaptive/adaptive_btree.h",
        "include/adaptive/adaptive_btree_regression.h",
        "include/adaptive/adaptive_btree_aco.h",
    ]
    
    for h in headers:
        all_good &= check_file(h, Path(h).name)
    
    print("\n📄 Tests:")
    tests = [
        "tests/test_btree.cpp",
        "tests/test_monitor.cpp",
        "tests/test_predictor.cpp",
        "tests/test_adaptive.cpp",
        "tests/test_adaptive_regression.cpp",
        "tests/test_adaptive_aco.cpp",
    ]
    
    for t in tests:
        all_good &= check_file(t, Path(t).name)
    
    print("\n📄 Build System:")
    all_good &= check_file("build.bat", "Main build script")
    
    print("\n" + "="*60)
    if all_good:
        print("✅ Core Implementation: COMPLETE")
    else:
        print("❌ Core Implementation: INCOMPLETE")
    print("="*60)
    
    return all_good

def main():
    """Run all checks"""
    print("\n" + "="*70)
    print("  DATABASE INTEGRATION - COMPLETE CHECKPOINT")
    print("  Cache-Adaptive B-Trees SIGMOD Project")
    print("="*70)
    
    # Change to project root if needed
    if not Path("integration").exists():
        print("\n❌ ERROR: Must run from project root (D:\\Research\\SIGMOD)")
        print("Current directory:", os.getcwd())
        sys.exit(1)
    
    core_ok = check_core_code()
    sqlite_ok = check_sqlite()
    postgres_ok = check_postgresql()
    
    # Summary
    print("\n" + "="*70)
    print("  OVERALL STATUS")
    print("="*70)
    print(f"  Core Implementation:  {'✅ READY' if core_ok else '❌ INCOMPLETE'}")
    print(f"  SQLite Integration:   {'✅ READY' if sqlite_ok else '❌ INCOMPLETE'}")
    print(f"  PostgreSQL Source:    {'✅ READY' if postgres_ok else '❌ INCOMPLETE'}")
    print("="*70)
    
    if sqlite_ok:
        print("\n🚀 NEXT STEPS:")
        print("  1. Build SQLite:")
        print("     cd integration\\sqlite")
        print("     .\\buildsqlite.bat")
        print("\n  2. Test SQLite:")
        print("     .\\sqlite3_adaptive.exe test.db")
        print("\n  3. Then: Setup YCSB benchmarks")
    else:
        print("\n⚠️  FIX ISSUES:")
        print("  1. Copy missing adaptive headers to integration\\sqlite\\adaptive\\")
        print("  2. Copy buildsqlite.bat to integration\\sqlite\\")
        print("  3. Run this checkpoint again")
    
    print("\n" + "="*70 + "\n")
    
    return 0 if (core_ok and sqlite_ok and postgres_ok) else 1

if __name__ == "__main__":
    sys.exit(main())