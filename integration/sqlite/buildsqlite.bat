@echo off
REM ========================================
REM SQLite Build Script (MinGW Compatible)
REM Cache-Adaptive B-Trees SIGMOD Project
REM ========================================

echo.
echo ========================================
echo Building SQLite (MinGW Compatible)
echo ========================================
echo.

REM Configuration
set SQLITE_DIR=sqlite-amalgamation-3510200
set OUTPUT=sqlite3_adaptive.exe

REM Check if SQLite source exists
if not exist %SQLITE_DIR% (
    echo ERROR: Directory %SQLITE_DIR% not found!
    goto error
)

if not exist %SQLITE_DIR%\sqlite3.c (
    echo ERROR: sqlite3.c not found!
    goto error
)

if not exist %SQLITE_DIR%\sqlite3.h (
    echo ERROR: sqlite3.h not found!
    goto error
)

if not exist %SQLITE_DIR%\shell.c (
    echo ERROR: shell.c not found!
    goto error
)

echo [1/2] SQLite source files: FOUND
echo      Location: %SQLITE_DIR%
echo.

REM Compile with MinGW compatibility flags
echo [2/2] Compiling SQLite...
echo      Using MinGW compatibility mode...
echo.

gcc -O2 ^
    -DSQLITE_ENABLE_STAT4 ^
    -DHAVE_LOCALTIME_R=0 ^
    -DHAVE_LOCALTIME_S=0 ^
    -DHAVE_USLEEP=0 ^
    -D_UCRT ^
    -I%SQLITE_DIR% ^
    %SQLITE_DIR%\sqlite3.c ^
    %SQLITE_DIR%\shell.c ^
    -o %OUTPUT%

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ========================================
    echo ERROR: Compilation failed!
    echo ========================================
    echo.
    echo Trying alternative build without shell...
    echo.
    
    REM Try building just the library without shell
    gcc -O2 ^
        -DSQLITE_ENABLE_STAT4 ^
        -DHAVE_LOCALTIME_R=0 ^
        -D_UCRT ^
        -I%SQLITE_DIR% ^
        -c %SQLITE_DIR%\sqlite3.c ^
        -o sqlite3.o
    
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: Even core compilation failed!
        goto error
    )
    
    echo.
    echo Core SQLite compiled successfully!
    echo Shell compilation had issues (known MinGW problem)
    echo.
    echo Next step: Download official SQLite pre-built Windows binary
    echo From: https://www.sqlite.org/download.html
    echo Look for: "Precompiled Binaries for Windows"
    echo.
    goto end
)

REM Success!
echo.
echo ========================================
echo SUCCESS!
echo ========================================
echo.
echo Created: %OUTPUT%
echo Size: 
dir %OUTPUT% | findstr /C:"%OUTPUT%"
echo.
echo Test with:
echo   %OUTPUT% test.db
echo.
echo Then in SQLite shell:
echo   CREATE TABLE users (id INTEGER, name TEXT);
echo   INSERT INTO users VALUES (1, 'Alice');
echo   SELECT * FROM users;
echo   .exit
echo.
goto end

:error
echo.
echo ========================================
echo Build failed!
echo ========================================
echo.
echo Known Issue: MinGW GCC 6.3.0 has compatibility issues with newer SQLite shell.c
echo.
echo SOLUTION: Use pre-compiled SQLite binary for testing
echo   1. Go to: https://www.sqlite.org/download.html
echo   2. Download: "sqlite-tools-win-x64-*.zip"
echo   3. Extract sqlite3.exe to this folder
echo   4. Use that for testing
echo.
echo OR: Upgrade to newer MinGW-w64 for compilation
echo.
exit /b 1

:end