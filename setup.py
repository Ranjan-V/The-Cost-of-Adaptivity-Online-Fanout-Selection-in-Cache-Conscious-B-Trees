#!/usr/bin/env python3
"""
Cache-Adaptive B-Trees - Complete SIGMOD Project Setup
Creates directory structure, build files, integration templates, and benchmark setup
"""

import os
import sys
from pathlib import Path

def create_directory_structure():
    """Create complete directory structure including database integration"""
    directories = [
        # Core implementation
        "include",
        "include/btree",
        "include/monitor",
        "include/predictor",
        "include/adaptive",
        
        # Tests
        "tests",
        
        # Benchmarks
        "benchmarks",
        "benchmarks/workloads",
        
        # Database Integration (NEW!)
        "integration",
        "integration/sqlite",
        "integration/sqlite/adaptive",
        "integration/postgresql",
        "integration/postgresql/patches",
        
        # Real Benchmarks (NEW!)
        "benchmarks/ycsb",
        "benchmarks/ycsb/workloads",
        "benchmarks/ycsb/results",
        "benchmarks/tpcc",
        "benchmarks/tpcc/results",
        "benchmarks/tpch",
        "benchmarks/tpch/queries",
        "benchmarks/tpch/results",
        "benchmarks/traces",
        "benchmarks/traces/wikipedia",
        "benchmarks/traces/twitter",
        
        # Profiling (NEW!)
        "profiling",
        "profiling/cachegrind",
        "profiling/perf",
        "profiling/results",
        
        # Analysis
        "scripts",
        "scripts/analysis",
        "scripts/plotting",
        
        # Results
        "results",
        "results/synthetic",
        "results/sqlite",
        "results/postgresql",
        "results/figures",
        
        # Paper
        "paper",
        "paper/sections",
        "paper/figures",
        "paper/tables",
        
        # Documentation
        "docs",
        "docs/integration",
        "docs/benchmarks",
        
        # Build
        "build",
        
        # VS Code
        ".vscode"
    ]
    
    print("Creating directory structure...")
    for directory in directories:
        Path(directory).mkdir(parents=True, exist_ok=True)
        print(f"  [OK] {directory}/")
    
    return directories

def create_build_script():
    """Create Windows build script (build.bat)"""
    build_bat = """@echo off
REM Build script for Cache-Adaptive B-Trees project
REM Compiler: GCC (MinGW)
REM Usage: build.bat [all|btree|monitor|predictor|adaptive|regression|aco|demo|bench|clean]

setlocal

set COMPILER=g++
set CXXFLAGS=-std=c++11 -Wall -Wextra -O2 -I./include
set BUILD_DIR=build

REM Create build directory if it doesn't exist
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

REM Parse command line argument
set TARGET=%1
if "%TARGET%"=="" set TARGET=all

if "%TARGET%"=="clean" goto clean
if "%TARGET%"=="all" goto all
if "%TARGET%"=="btree" goto btree
if "%TARGET%"=="monitor" goto monitor
if "%TARGET%"=="predictor" goto predictor
if "%TARGET%"=="adaptive" goto adaptive
if "%TARGET%"=="regression" goto regression
if "%TARGET%"=="aco" goto aco
if "%TARGET%"=="demo" goto demo
if "%TARGET%"=="bench" goto bench
if "%TARGET%"=="simple" goto simple

echo Unknown target: %TARGET%
echo Usage: build.bat [all^|btree^|monitor^|predictor^|adaptive^|regression^|aco^|demo^|bench^|simple^|clean]
goto end

:all
echo ========================================
echo Building All Targets
echo ========================================
call :btree
call :monitor
call :predictor
call :adaptive
call :regression
call :aco
call :demo
call :simple
call :bench
goto end

:btree
echo.
echo [1/9] Compiling B-Tree tests...
%COMPILER% %CXXFLAGS% tests/test_btree.cpp -o %BUILD_DIR%/test_btree.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: B-Tree compilation failed!
    goto end
)
echo SUCCESS: test_btree.exe created
goto :eof

:monitor
echo.
echo [2/9] Compiling Monitor tests...
%COMPILER% %CXXFLAGS% tests/test_monitor.cpp -o %BUILD_DIR%/test_monitor.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Monitor compilation failed!
    goto end
)
echo SUCCESS: test_monitor.exe created
goto :eof

:predictor
echo.
echo [3/9] Compiling Predictor tests...
%COMPILER% %CXXFLAGS% tests/test_predictor.cpp -o %BUILD_DIR%/test_predictor.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Predictor compilation failed!
    goto end
)
echo SUCCESS: test_predictor.exe created
goto :eof

:adaptive
echo.
echo [4/9] Compiling Adaptive B-Tree (RL) tests...
%COMPILER% %CXXFLAGS% tests/test_adaptive.cpp -o %BUILD_DIR%/test_adaptive.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Adaptive B-Tree compilation failed!
    goto end
)
echo SUCCESS: test_adaptive.exe created
goto :eof

:regression
echo.
echo [5/9] Compiling Adaptive B-Tree (Regression) tests...
%COMPILER% %CXXFLAGS% tests/test_adaptive_regression.cpp -o %BUILD_DIR%/test_adaptive_regression.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Regression B-Tree compilation failed!
    goto end
)
echo SUCCESS: test_adaptive_regression.exe created
goto :eof

:aco
echo.
echo [6/9] Compiling Adaptive B-Tree (ACO) tests...
%COMPILER% %CXXFLAGS% tests/test_adaptive_aco.cpp -o %BUILD_DIR%/test_adaptive_aco.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: ACO B-Tree compilation failed!
    goto end
)
echo SUCCESS: test_adaptive_aco.exe created
goto :eof

:demo
echo.
echo [7/9] Compiling Integration Demo...
%COMPILER% %CXXFLAGS% tests/demo_integration.cpp -o %BUILD_DIR%/demo_integration.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Demo compilation failed!
    goto end
)
echo SUCCESS: demo_integration.exe created
goto :eof

:simple
echo.
echo [8/9] Compiling Simple test...
%COMPILER% %CXXFLAGS% tests/simple_test.cpp -o %BUILD_DIR%/simple_test.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Simple test compilation failed!
    goto end
)
echo SUCCESS: simple_test.exe created
goto :eof

:bench
echo.
echo [9/9] Compiling Benchmarks...
%COMPILER% %CXXFLAGS% benchmarks/benchmark_baseline.cpp -o %BUILD_DIR%/bench_baseline.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Baseline benchmark compilation failed!
    goto end
)
echo SUCCESS: bench_baseline.exe created

echo.
echo [BONUS] Compiling Algorithm Comparison...
%COMPILER% %CXXFLAGS% benchmarks/benchmark_algorithms.cpp -o %BUILD_DIR%/bench_algorithms.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Algorithm benchmark compilation failed!
    goto end
)
echo SUCCESS: bench_algorithms.exe created
goto :eof

:clean
echo Cleaning build directory...
if exist %BUILD_DIR% (
    del /Q %BUILD_DIR%\\*.exe 2>nul
    echo Build directory cleaned
) else (
    echo Build directory does not exist
)
goto end

:end
echo.
echo ========================================
echo Build Complete
echo ========================================
endlocal
"""
    
    with open("build.bat", "w", encoding='utf-8') as f:
        f.write(build_bat)
    print("  [OK] build.bat")

def create_readme():
    """Create comprehensive README.md"""
    readme = """# Cache-Adaptive B-Trees - SIGMOD Research Project

**Dynamic Fanout Optimization for Cache-Aware Index Structures**

## 🎯 Project Overview

This project implements adaptive B-Tree data structures that dynamically adjust node fanout based on real-time access patterns and cache hierarchy characteristics. Three algorithms are compared:

1. **Q-Learning (Reinforcement Learning)** - Model-free RL with epsilon-greedy exploration
2. **Linear Regression** - Statistical prediction with gradient descent
3. **Ant Colony Optimization (ACO)** - Swarm intelligence with pheromone trails

**Research Goal:** Achieve 2-3x cache hit improvement on skewed workloads (Zipfian distribution)

## 📁 Project Structure

```
SIGMOD/
├── include/              # C++ headers
│   ├── btree/           # B+ Tree implementation
│   ├── monitor/         # Access pattern monitoring
│   ├── predictor/       # Fanout prediction
│   └── adaptive/        # Adaptive algorithms (RL, Regression, ACO)
│
├── tests/               # Unit tests and integration tests
├── benchmarks/          # Performance benchmarks
│   ├── workloads/       # Synthetic workload generators
│   ├── ycsb/           # YCSB integration
│   ├── tpcc/           # TPC-C benchmark
│   └── tpch/           # TPC-H benchmark
│
├── integration/         # Database integration
│   ├── sqlite/         # SQLite modification
│   └── postgresql/     # PostgreSQL modification
│
├── profiling/          # Hardware profiling tools
│   ├── cachegrind/     # Valgrind cache profiling
│   └── perf/           # Linux perf integration
│
├── results/            # Benchmark results
├── scripts/            # Analysis and plotting scripts
├── paper/              # LaTeX paper source
└── docs/               # Documentation

```

## 🚀 Quick Start

### Prerequisites
- **Windows:** MinGW GCC 6.3.0+ (C++11)
- **Python:** 3.11+ with venv
- **Tools:** Git, VS Code (recommended)

### Setup

```powershell
# 1. Create virtual environment
python -m venv venv
.\\venv\\Scripts\\Activate.ps1

# 2. Install Python dependencies
pip install -r requirements.txt

# 3. Build all components
.\\build.bat all

# 4. Run tests
.\\build\\test_btree.exe
.\\build\\test_monitor.exe
.\\build\\test_predictor.exe
.\\build\\test_adaptive.exe
.\\build\\test_adaptive_regression.exe
.\\build\\test_adaptive_aco.exe

# 5. Run algorithm comparison
.\\build\\bench_algorithms.exe
```

## 📊 Current Status

### ✅ Phase 1: Core Implementation (COMPLETE)
- [x] B+ Tree baseline (fanout 8-256)
- [x] Access pattern monitor (heat scores)
- [x] Fanout predictor (exponential smoothing)
- [x] Q-Learning adaptive algorithm
- [x] Linear Regression adaptive algorithm
- [x] Ant Colony Optimization adaptive algorithm
- [x] Comprehensive test suites (all passing)
- [x] Synthetic benchmarks (5 workloads)

**Results:** Linear Regression won on synthetic benchmarks (Score: 7.87)

### 🚧 Phase 2: Database Integration (IN PROGRESS)
- [ ] SQLite integration
- [ ] PostgreSQL integration
- [ ] YCSB benchmarks (6 workloads)
- [ ] TPC-C benchmarks
- [ ] TPC-H benchmarks
- [ ] Real application traces

### ⏰ Phase 3: Hardware Profiling (PLANNED)
- [ ] Valgrind/Cachegrind profiling
- [ ] perf hardware counters
- [ ] Prove actual L1/L2/L3 cache improvements

### ⏰ Phase 4: Theory & Paper (PLANNED)
- [ ] Complexity analysis
- [ ] Optimality proofs
- [ ] Regret bounds
- [ ] LaTeX paper writing

## 🔬 Algorithms

### Q-Learning (Reinforcement Learning)
- **States:** 4 levels (Cold, Lukewarm, Warm, Hot)
- **Actions:** 3 options (Decrease, Maintain, Increase fanout)
- **Learning:** TD-learning with epsilon-greedy exploration
- **Parameters:** α=0.1, γ=0.9, ε=0.2→0.01

### Linear Regression (Statistical)
- **Model:** `fanout = w₀ + w₁×heat + w₂×access_count`
- **Learning:** Gradient descent
- **Advantage:** Simple, fast, interpretable
- **Winner on synthetic benchmarks!**

### Ant Colony Optimization (Swarm)
- **Mechanism:** Pheromone trails guide decisions
- **Parameters:** 10 ants, evaporation=0.1, α=1.0, β=2.0
- **Advantage:** Explores solution space well

## 📈 Performance Results (Synthetic)

### Algorithm Comparison (5 Workloads)
| Algorithm | Avg Reward | Cache Hit Rate | Score |
|-----------|------------|----------------|-------|
| **Linear Regression** | 6.96 | 100% | **7.87** ⭐ |
| Q-Learning | 6.25 | 100% | 7.37 |
| Genetic Algorithm | 6.18 | 100% | 7.32 |

### Workload Performance
- **Hot (80/20):** All algorithms achieve small fanout (8-16)
- **Cold (Uniform):** All algorithms adapt to large fanout (128-256)
- **Zipfian:** Linear Regression most stable
- **Shifting:** Linear Regression adapts fastest

## 🗄️ Database Integration

### SQLite Integration
```bash
cd integration/sqlite
# Download SQLite source from sqlite.org
# Follow SQLITE_INTEGRATION.md
./build.bat
```

### PostgreSQL Integration
```bash
cd integration/postgresql
# Clone PostgreSQL source
# Follow POSTGRESQL_INTEGRATION.md
```

### YCSB Benchmarks
```bash
cd benchmarks/ycsb
# Install Java & Maven
# Download YCSB
# Run workloads A-F
```

## 📚 Key References

1. **Graefe (2011)** - "Modern B-Tree Techniques"
2. **Rao & Ross (2000)** - "Making B+-Trees Cache Conscious in Main Memory"
3. **Chen et al. (2001)** - "Fractal Prefetching B+-Trees"
4. **Bender et al. (2000)** - "Cache-Oblivious B-Trees"

## 🎓 Target Conferences

- **SIGMOD 2026** (Deadline: ~Nov 2025)
- **VLDB 2026** (Deadline: ~Mar 2026)
- **ICDE 2026** (Deadline: ~Sep 2025)

## 📊 Experimental Methodology

### Metrics Measured
- **Throughput:** Operations per second
- **Latency:** Average, P50, P95, P99
- **Cache Performance:** L1/L2/L3 hit rates
- **Adaptation Overhead:** % of total execution time
- **Memory Usage:** Peak and average

### Workloads
1. **Synthetic:** Hot, Cold, Mixed, Zipfian, Shifting
2. **YCSB:** Workloads A-F
3. **TPC-C:** OLTP transactions
4. **TPC-H:** Analytics queries
5. **Real Traces:** Wikipedia, Twitter, E-commerce

## 🛠️ Development

### Running Specific Tests
```powershell
# B-Tree baseline
.\\build\\test_btree.exe

# Monitor
.\\build\\test_monitor.exe

# Specific algorithm
.\\build\\test_adaptive_regression.exe
```

### Running Benchmarks
```powershell
# Synthetic benchmark
.\\build\\bench_baseline.exe

# Algorithm comparison
.\\build\\bench_algorithms.exe
```

### Adding New Algorithms
1. Create header in `include/adaptive/`
2. Create test in `tests/`
3. Add to `build.bat`
4. Add to `benchmark_algorithms.cpp`

## 📝 Contributing

This is a research project. For collaboration, please contact the author.

## 📄 License

Academic use only.

## 📧 Contact

[Your Name]  
[Your University]  
[Your Email]

---

**Last Updated:** December 2024  
**Status:** Phase 1 Complete, Phase 2 In Progress  
**Next Milestone:** SQLite Integration by January 2025
"""
    
    with open("README.md", "w", encoding='utf-8') as f:
        f.write(readme)
    print("  [OK] README.md")

def create_requirements():
    """Create requirements.txt for Python dependencies"""
    requirements = """# Python dependencies for Cache-Adaptive B-Trees project

# Core scientific computing
numpy>=1.21.0
pandas>=1.3.0
scipy>=1.7.0

# Visualization
matplotlib>=3.4.0
seaborn>=0.11.0
plotly>=5.0.0

# Analysis
jupyter>=1.0.0
notebook>=6.4.0
ipython>=7.30.0

# Testing
pytest>=6.2.0
pytest-cov>=2.12.0

# Benchmarking
memory_profiler>=0.60.0

# Documentation
sphinx>=4.0.0
sphinx-rtd-theme>=0.5.0

# Utilities
tqdm>=4.60.0
"""
    
    with open("requirements.txt", "w", encoding='utf-8') as f:
        f.write(requirements)
    print("  [OK] requirements.txt")

def create_gitignore():
    """Create .gitignore"""
    gitignore = """# Build artifacts
build/
*.o
*.a
*.so
*.exe
*.dll

# Python
__pycache__/
*.pyc
*.pyo
venv/
.env
*.egg-info/

# Results
results/**/*.csv
results/**/*.json
!results/.gitkeep

# IDE
.vscode/*
!.vscode/tasks.json
!.vscode/settings.json
!.vscode/launch.json
.idea/
*.swp
*.swo
*~

# OS
.DS_Store
Thumbs.db

# Database files
*.db
*.sqlite
*.sqlite3

# Profiling
*.cachegrind
*.perf.data
*.prof

# LaTeX
*.aux
*.log
*.out
*.toc
*.bbl
*.blg
*.synctex.gz
paper/*.pdf
!paper/paper.pdf

# Temporary
*.tmp
*.bak
*~
"""
    
    with open(".gitignore", "w", encoding='utf-8') as f:
        f.write(gitignore)
    print("  [OK] .gitignore")

def create_vscode_config():
    """Create VS Code configuration"""
    
    # tasks.json
    tasks = """{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build All",
            "type": "shell",
            "command": ".\\\\build.bat",
            "args": ["all"],
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "Build Tests",
            "type": "shell",
            "command": ".\\\\build.bat",
            "args": ["btree"]
        },
        {
            "label": "Run Tests",
            "type": "shell",
            "command": ".\\\\build\\\\test_btree.exe",
            "group": "test"
        },
        {
            "label": "Run Algorithm Comparison",
            "type": "shell",
            "command": ".\\\\build\\\\bench_algorithms.exe"
        },
        {
            "label": "Clean Build",
            "type": "shell",
            "command": ".\\\\build.bat",
            "args": ["clean"]
        }
    ]
}"""
    
    with open(".vscode/tasks.json", "w", encoding='utf-8') as f:
        f.write(tasks)
    
    # settings.json
    settings = """{
    "files.associations": {
        "*.h": "cpp",
        "*.cpp": "cpp"
    },
    "editor.formatOnSave": true,
    "editor.tabSize": 4,
    "C_Cpp.default.cppStandard": "c++11",
    "C_Cpp.default.includePath": [
        "${workspaceFolder}/include"
    ],
    "files.exclude": {
        "**/*.o": true,
        "**/*.exe": true,
        "build/": true
    }
}"""
    
    with open(".vscode/settings.json", "w", encoding='utf-8') as f:
        f.write(settings)
    
    # launch.json
    launch = """{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug B-Tree Test",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/test_btree.exe",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "miDebuggerPath": "gdb.exe",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ]
        }
    ]
}"""
    
    with open(".vscode/launch.json", "w", encoding='utf-8') as f:
        f.write(launch)
    
    print("  [OK] VS Code configuration")

def create_integration_guides():
    """Create database integration guides"""
    
    # SQLite guide
    sqlite_guide = """# SQLite Integration Guide

See docs/integration/SQLITE_INTEGRATION.md for detailed instructions.

## Quick Start

1. Download SQLite amalgamation from: https://www.sqlite.org/download.html
2. Extract to: `integration/sqlite/`
3. Copy adaptive headers to: `integration/sqlite/adaptive/`
4. Run: `cd integration/sqlite && build.bat`
5. Test: `sqlite3_adaptive.exe test.db`

## Files Needed

- sqlite3.c (from SQLite)
- sqlite3.h (from SQLite)
- shell.c (from SQLite)
- adaptive/ (our headers)

## Build

```batch
cd integration\\sqlite
build.bat
```
"""
    
    Path("integration/sqlite").mkdir(parents=True, exist_ok=True)
    with open("integration/sqlite/README.md", "w", encoding='utf-8') as f:
        f.write(sqlite_guide)
    
    # PostgreSQL guide
    postgres_guide = """# PostgreSQL Integration Guide

See docs/integration/POSTGRESQL_INTEGRATION.md for detailed instructions.

## Quick Start

1. Clone PostgreSQL: `git clone https://github.com/postgres/postgres.git`
2. Apply patches from: `integration/postgresql/patches/`
3. Build PostgreSQL with adaptive B-tree
4. Run benchmarks

## Requirements

- Visual Studio 2019+ OR MinGW-w64
- Perl (for build system)
- Python 3.8+

## Build

Follow official PostgreSQL build guide with our modifications.
"""
    
    Path("integration/postgresql").mkdir(parents=True, exist_ok=True)
    with open("integration/postgresql/README.md", "w", encoding='utf-8') as f:
        f.write(postgres_guide)
    
    print("  [OK] Integration guides")

def create_placeholder_files():
    """Create placeholder .gitkeep files"""
    placeholders = [
        "results/.gitkeep",
        "results/synthetic/.gitkeep",
        "results/sqlite/.gitkeep",
        "results/postgresql/.gitkeep",
        "results/figures/.gitkeep",
        "build/.gitkeep",
        "paper/figures/.gitkeep",
        "paper/tables/.gitkeep",
        "benchmarks/ycsb/results/.gitkeep",
        "benchmarks/tpcc/results/.gitkeep",
        "benchmarks/tpch/results/.gitkeep",
        "profiling/results/.gitkeep"
    ]
    
    for placeholder in placeholders:
        Path(placeholder).parent.mkdir(parents=True, exist_ok=True)
        Path(placeholder).touch()
    
    print("  [OK] Placeholder files")

def create_analysis_scripts():
    """Create starter analysis scripts"""
    
    plot_script = """#!/usr/bin/env python3
\"\"\"
Plot benchmark results for Cache-Adaptive B-Trees
\"\"\"

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import sys

def plot_algorithm_comparison(csv_file):
    \"\"\"Plot algorithm comparison from benchmark results\"\"\"
    df = pd.read_csv(csv_file)
    
    plt.figure(figsize=(12, 6))
    sns.barplot(data=df, x='workload', y='avg_reward', hue='algorithm')
    plt.title('Algorithm Performance Comparison')
    plt.ylabel('Average Reward')
    plt.xlabel('Workload')
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig('results/figures/algorithm_comparison.png', dpi=300)
    print("Plot saved to: results/figures/algorithm_comparison.png")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python plot_results.py <results.csv>")
        sys.exit(1)
    
    plot_algorithm_comparison(sys.argv[1])
"""
    
    with open("scripts/plotting/plot_results.py", "w", encoding='utf-8') as f:
        f.write(plot_script)
    
    print("  [OK] Analysis scripts")

def main():
    """Main setup function"""
    print("\n" + "="*70)
    print("  Cache-Adaptive B-Trees - SIGMOD Project Setup")
    print("  Complete Research Project Structure")
    print("="*70 + "\n")
    
    try:
        # Create structure
        create_directory_structure()
        
        print("\nCreating build configuration...")
        create_build_script()
        
        print("\nCreating project files...")
        create_readme()
        create_requirements()
        create_gitignore()
        create_vscode_config()
        create_integration_guides()
        create_placeholder_files()
        create_analysis_scripts()
        
        print("\n" + "="*70)
        print("  ✅ SUCCESS! Setup complete!")
        print("="*70)
        print("\n📋 Next steps:")
        print("\n1. Copy your existing code:")
        print("   - Copy include/ headers from old project")
        print("   - Copy tests/ files from old project")
        print("   - Copy benchmarks/ files from old project")
        print("   - Copy build.bat from old project")
        print("\n2. Setup Python environment:")
        print("   python -m venv venv")
        print("   .\\venv\\Scripts\\Activate.ps1")
        print("   pip install -r requirements.txt")
        print("\n3. Build and test:")
        print("   .\\build.bat all")
        print("   .\\build\\test_btree.exe")
        print("\n4. Start database integration:")
        print("   cd integration\\sqlite")
        print("   (Download SQLite source)")
        print("\n📚 Documentation:")
        print("   - README.md - Project overview")
        print("   - docs/integration/ - Database integration guides")
        print("   - integration/*/README.md - Quick start guides")
        print()
        
    except Exception as e:
        print(f"\n❌ ERROR during setup: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()