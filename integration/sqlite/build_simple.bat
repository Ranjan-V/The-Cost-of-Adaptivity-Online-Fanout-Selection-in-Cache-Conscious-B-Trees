@echo off
REM Simplified Build for SQLite Extension
REM Copies headers and builds in current directory

echo.
echo ========================================
echo SQLite Adaptive Extension - Simple Build
echo ========================================
echo.

set SQLITE_DIR=sqlite-amalgamation-3510200

REM Copy sqlite3ext.h to current directory (fixes IntelliSense)
if exist %SQLITE_DIR%\sqlite3ext.h (
    echo [Setup] Copying sqlite3ext.h...
    copy %SQLITE_DIR%\sqlite3ext.h . >nul
    echo   ✓ sqlite3ext.h copied
) else (
    echo ERROR: %SQLITE_DIR%\sqlite3ext.h not found!
    goto error
)

REM Check required files
echo.
echo [Check] Verifying source files...
if not exist adaptive_complete_sqlite.h echo   ✗ adaptive_complete_sqlite.h MISSING & goto error
echo   ✓ adaptive_complete_sqlite.h

if not exist adaptive_wrapper.h echo   ✗ adaptive_wrapper.h MISSING & goto error
echo   ✓ adaptive_wrapper.h

if not exist adaptive_wrapper_impl.cpp echo   ✗ adaptive_wrapper_impl.cpp MISSING & goto error
echo   ✓ adaptive_wrapper_impl.cpp

if not exist adaptive_btree_extension.c echo   ✗ adaptive_btree_extension.c MISSING & goto error
echo   ✓ adaptive_btree_extension.c

echo.
echo [1/3] Compiling C++ wrapper...
g++ -c -O2 -std=c++11 -I. adaptive_wrapper_impl.cpp -o adaptive_wrapper.o
if %ERRORLEVEL% NEQ 0 goto error
echo   ✓ Done

echo.
echo [2/3] Compiling C extension...
gcc -c -O2 -I. adaptive_btree_extension.c -o adaptive_btree_extension.o
if %ERRORLEVEL% NEQ 0 goto error
echo   ✓ Done

echo.
echo [3/3] Linking DLL...
g++ -shared adaptive_wrapper.o adaptive_btree_extension.o -o adaptive_btree.dll
if %ERRORLEVEL% NEQ 0 goto error
echo   ✓ Done

echo.
echo ========================================
echo SUCCESS! Extension built
echo ========================================
echo.
dir adaptive_btree.dll | findstr "adaptive_btree.dll"
echo.
echo Quick test:
echo   sqlite3.exe test.db ".load adaptive_btree" "SELECT adaptive_set_algorithm('regression');"
echo.
goto end

:error
echo.
echo ========================================
echo ERROR: Build failed
echo ========================================
echo.
echo Check that you're in: D:\Research\SIGMOD\integration\sqlite
echo And all source files are present.
echo.
exit /b 1

:end