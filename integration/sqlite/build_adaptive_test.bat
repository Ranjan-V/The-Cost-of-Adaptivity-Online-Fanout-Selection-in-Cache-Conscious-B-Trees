@echo off
REM Build Adaptive SQLite Integration Test
REM Tests all 3 algorithms with SQLite

echo.
echo ========================================
echo Building Adaptive SQLite Test
echo ========================================
echo.

REM Check if adaptive files exist
if not exist adaptive_complete_sqlite.h (
    echo ERROR: adaptive_complete_sqlite.h not found!
    echo Please copy from outputs folder
    goto error
)

if not exist adaptive_wrapper.h (
    echo ERROR: adaptive_wrapper.h not found!
    echo Please copy from outputs folder
    goto error
)

if not exist test_sqlite_adaptive.cpp (
    echo ERROR: test_sqlite_adaptive.cpp not found!
    echo Please copy from outputs folder
    goto error
)

echo [1/2] Compiling adaptive integration test...
echo.

g++ -O2 ^
    -std=c++11 ^
    -I. ^
    test_sqlite_adaptive.cpp ^
    -o test_sqlite_adaptive.exe

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Compilation failed!
    goto error
)

echo.
echo ========================================
echo SUCCESS!
echo ========================================
echo.
echo Created: test_sqlite_adaptive.exe
echo.
echo Run the test:
echo   .\test_sqlite_adaptive.exe
echo.
goto end

:error
echo.
echo Build failed!
echo.
exit /b 1

:end