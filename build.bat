@echo off
REM Build script for Cache-Adaptive B-Trees project
REM Compiler: GCC 6.3.0 (C++11)
REM Usage: build.bat [all|btree|monitor|predictor|adaptive|regression|aco|segmented|demo|bench|ycsb|overhead|shifting|wiki|threads|dynamic|simple|clean]

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
if "%TARGET%"=="segmented" goto segmented
if "%TARGET%"=="demo" goto demo
if "%TARGET%"=="bench" goto bench
if "%TARGET%"=="ycsb" goto ycsb
if "%TARGET%"=="overhead" goto overhead
if "%TARGET%"=="shifting" goto shifting
if "%TARGET%"=="wiki" goto wiki
if "%TARGET%"=="threads" goto threads
if "%TARGET%"=="dynamic" goto dynamic
if "%TARGET%"=="simple" goto simple

echo Unknown target: %TARGET%
echo Usage: build.bat [all^|btree^|monitor^|predictor^|adaptive^|regression^|aco^|segmented^|demo^|bench^|ycsb^|overhead^|shifting^|wiki^|threads^|dynamic^|simple^|clean]
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
call :segmented
call :demo
call :simple
call :bench
goto end

:btree
echo.
echo [1/16] Compiling B-Tree tests...
%COMPILER% %CXXFLAGS% tests/test_btree.cpp -o %BUILD_DIR%/test_btree.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: B-Tree compilation failed!
    goto end
)
echo SUCCESS: test_btree.exe created
goto :eof

:monitor
echo.
echo [2/16] Compiling Monitor tests...
%COMPILER% %CXXFLAGS% tests/test_monitor.cpp -o %BUILD_DIR%/test_monitor.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Monitor compilation failed!
    goto end
)
echo SUCCESS: test_monitor.exe created
goto :eof

:predictor
echo.
echo [3/16] Compiling Predictor tests...
%COMPILER% %CXXFLAGS% tests/test_predictor.cpp -o %BUILD_DIR%/test_predictor.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Predictor compilation failed!
    goto end
)
echo SUCCESS: test_predictor.exe created
goto :eof

:adaptive
echo.
echo [4/16] Compiling Adaptive B-Tree (RL) tests...
%COMPILER% %CXXFLAGS% tests/test_adaptive.cpp -o %BUILD_DIR%/test_adaptive.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Adaptive B-Tree compilation failed!
    goto end
)
echo SUCCESS: test_adaptive.exe created
goto :eof

:regression
echo.
echo [5/16] Compiling Adaptive B-Tree (Regression) tests...
%COMPILER% %CXXFLAGS% tests/test_adaptive_regression.cpp -o %BUILD_DIR%/test_adaptive_regression.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Regression B-Tree compilation failed!
    goto end
)
echo SUCCESS: test_adaptive_regression.exe created
goto :eof

:aco
echo.
echo [6/16] Compiling Adaptive B-Tree (ACO) tests...
%COMPILER% %CXXFLAGS% tests/test_adaptive_aco.cpp -o %BUILD_DIR%/test_adaptive_aco.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: ACO B-Tree compilation failed!
    goto end
)
echo SUCCESS: test_adaptive_aco.exe created
goto :eof

:segmented
echo.
echo [7/16] Compiling Segmented Adaptive B-Tree tests...
%COMPILER% %CXXFLAGS% tests/test_segmented_adaptive.cpp -o %BUILD_DIR%/test_segmented_adaptive.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Segmented Adaptive B-Tree compilation failed!
    goto end
)
echo SUCCESS: test_segmented_adaptive.exe created
goto :eof

:demo
echo.
echo [8/16] Compiling Integration Demo...
%COMPILER% %CXXFLAGS% tests/demo_integration.cpp -o %BUILD_DIR%/demo_integration.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Demo compilation failed!
    goto end
)
echo SUCCESS: demo_integration.exe created
goto :eof

:simple
echo.
echo [9/16] Compiling Simple test...
%COMPILER% %CXXFLAGS% tests/simple_test.cpp -o %BUILD_DIR%/simple_test.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Simple test compilation failed!
    goto end
)
echo SUCCESS: simple_test.exe created
goto :eof

:bench
echo.
echo [10/16] Compiling Benchmarks...
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

call :ycsb
call :overhead
call :shifting
call :wiki
call :threads
call :dynamic
goto :eof

:ycsb
echo.
echo [11/16] Compiling YCSB Benchmark...
%COMPILER% %CXXFLAGS% benchmarks/benchmark_ycsb.cpp -o %BUILD_DIR%/bench_ycsb.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: YCSB benchmark compilation failed!
    goto end
)
echo SUCCESS: bench_ycsb.exe created
goto :eof

:overhead
echo.
echo [12/16] Compiling Monitor Overhead Benchmark...
%COMPILER% %CXXFLAGS% benchmarks/benchmark_overhead.cpp -o %BUILD_DIR%/bench_overhead.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Monitor overhead benchmark compilation failed!
    goto end
)
echo SUCCESS: bench_overhead.exe created
goto :eof

:shifting
echo.
echo [13/16] Compiling Shifting Oracle Benchmark...
%COMPILER% %CXXFLAGS% benchmarks/benchmark_shifting.cpp -o %BUILD_DIR%/bench_shifting.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Shifting benchmark compilation failed!
    goto end
)
echo SUCCESS: bench_shifting.exe created
goto :eof

:wiki
echo.
echo [14/16] Compiling Wikipedia Trace Benchmark...
%COMPILER% %CXXFLAGS% benchmarks/benchmark_wiki_trace.cpp -o %BUILD_DIR%/bench_wiki_trace.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Wikipedia trace benchmark compilation failed!
    goto end
)
echo SUCCESS: bench_wiki_trace.exe created
goto :eof

:threads
echo.
echo [15/16] Compiling Thread Scalability Benchmark...
%COMPILER% %CXXFLAGS% benchmarks/benchmark_threads.cpp -o %BUILD_DIR%/bench_threads.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Thread scalability benchmark compilation failed!
    goto end
)
echo SUCCESS: bench_threads.exe created
goto :eof

:dynamic
echo.
echo [16/16] Compiling Sliding Hotspot Benchmark...
%COMPILER% %CXXFLAGS% benchmarks/benchmark_dynamic.cpp -o %BUILD_DIR%/bench_dynamic.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Sliding hotspot benchmark compilation failed!
    goto end
)
echo SUCCESS: bench_dynamic.exe created
goto :eof

:clean
echo Cleaning build directory...
if exist %BUILD_DIR% (
    del /Q %BUILD_DIR%\*.exe 2>nul
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
