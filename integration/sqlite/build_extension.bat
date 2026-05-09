@echo off
REM Build SQLite Adaptive Extension - COMPLETE FIX
REM Copies all required SQLite headers

echo.
echo ========================================
echo Building SQLite Adaptive Extension
echo ========================================
echo.

set SQLITE_DIR=sqlite-amalgamation-3510200

REM Copy SQLite headers to current directory
echo [Setup] Copying SQLite headers...

if exist %SQLITE_DIR%\sqlite3ext.h (
    copy %SQLITE_DIR%\sqlite3ext.h . >nul 2>&1
    echo   ✓ sqlite3ext.h
) else (
    echo   ✗ sqlite3ext.h not found in %SQLITE_DIR%
    goto error
)

if exist %SQLITE_DIR%\sqlite3.h (
    copy %SQLITE_DIR%\sqlite3.h . >nul 2>&1
    echo   ✓ sqlite3.h
) else (
    echo   ✗ sqlite3.h not found in %SQLITE_DIR%
    goto error
)

echo.
echo [Check] Verifying source files...

if not exist adaptive_complete_sqlite.h (
    echo   ✗ adaptive_complete_sqlite.h MISSING
    goto error
)
echo   ✓ adaptive_complete_sqlite.h

if not exist adaptive_wrapper.h (
    echo   ✗ adaptive_wrapper.h MISSING
    goto error
)
echo   ✓ adaptive_wrapper.h

if not exist adaptive_wrapper_impl.cpp (
    echo   ✗ adaptive_wrapper_impl.cpp MISSING
    goto error
)
echo   ✓ adaptive_wrapper_impl.cpp

if not exist adaptive_btree_extension.c (
    echo   ✗ adaptive_btree_extension.c MISSING
    goto error
)
echo   ✓ adaptive_btree_extension.c

echo.
echo [1/3] Compiling C++ wrapper...

g++ -c -O2 -std=c++11 ^
    -I. ^
    adaptive_wrapper_impl.cpp ^
    -o adaptive_wrapper.o

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo   ✗ C++ compilation failed
    goto error
)
echo   ✓ adaptive_wrapper.o

echo.
echo [2/3] Compiling C extension...

gcc -c -O2 ^
    -I. ^
    adaptive_btree_extension.c ^
    -o adaptive_btree_extension.o

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo   ✗ C compilation failed
    goto error
)
echo   ✓ adaptive_btree_extension.o

echo.
echo [3/3] Linking DLL...

g++ -shared ^
    adaptive_wrapper.o ^
    adaptive_btree_extension.o ^
    -o adaptive_btree.dll

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo   ✗ Linking failed
    goto error
)
echo   ✓ adaptive_btree.dll

REM Clean up object files
del adaptive_wrapper.o adaptive_btree_extension.o 2>nul

echo.
echo ========================================
echo SUCCESS!
echo ========================================
echo.
dir adaptive_btree.dll | findstr "adaptive_btree"
echo.
echo Quick test:
echo   sqlite3.exe test.db
echo.
echo In SQLite shell:
echo   .load adaptive_btree
echo   SELECT adaptive_set_algorithm('regression');
echo   SELECT adaptive_get_stats();
echo.
goto end

:error
echo.
echo ========================================
echo Build Failed
echo ========================================
echo.
echo Current directory:
cd
echo.
echo Files needed:
echo   - adaptive_complete_sqlite.h
echo   - adaptive_wrapper.h
echo   - adaptive_wrapper_impl.cpp
echo   - adaptive_btree_extension.c
echo   - %SQLITE_DIR%\sqlite3.h
echo   - %SQLITE_DIR%\sqlite3ext.h
echo.
exit /b 1

:end