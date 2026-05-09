@echo off
REM Build SQLite Adaptive Extension - 64-bit Version
REM Matches 64-bit SQLite binary

echo.
echo ========================================
echo Building SQLite Adaptive Extension (64-bit)
echo ========================================
echo.

set SQLITE_DIR=sqlite-amalgamation-3510200

REM Copy SQLite headers
echo [Setup] Copying SQLite headers...
if exist %SQLITE_DIR%\sqlite3ext.h copy %SQLITE_DIR%\sqlite3ext.h . >nul 2>&1
if exist %SQLITE_DIR%\sqlite3.h copy %SQLITE_DIR%\sqlite3.h . >nul 2>&1
echo   ✓ Headers copied

echo.
echo [Check] Verifying source files...
if not exist adaptive_complete_sqlite.h echo   ✗ MISSING & goto error
echo   ✓ adaptive_complete_sqlite.h
if not exist adaptive_wrapper.h echo   ✗ MISSING & goto error
echo   ✓ adaptive_wrapper.h
if not exist adaptive_wrapper_impl.cpp echo   ✗ MISSING & goto error
echo   ✓ adaptive_wrapper_impl.cpp
if not exist adaptive_btree_extension.c echo   ✗ MISSING & goto error
echo   ✓ adaptive_btree_extension.c

echo.
echo [1/3] Compiling C++ wrapper (64-bit)...

g++ -m64 -c -O2 -std=c++11 ^
    -I. ^
    adaptive_wrapper_impl.cpp ^
    -o adaptive_wrapper.o

if %ERRORLEVEL% NEQ 0 goto error
echo   ✓ adaptive_wrapper.o

echo.
echo [2/3] Compiling C extension (64-bit)...

gcc -m64 -c -O2 ^
    -I. ^
    adaptive_btree_extension.c ^
    -o adaptive_btree_extension.o

if %ERRORLEVEL% NEQ 0 goto error
echo   ✓ adaptive_btree_extension.o

echo.
echo [3/3] Linking DLL (64-bit)...

g++ -m64 -shared ^
    adaptive_wrapper.o ^
    adaptive_btree_extension.o ^
    -o adaptive_btree.dll

if %ERRORLEVEL% NEQ 0 goto error
echo   ✓ adaptive_btree.dll

REM Clean up
del adaptive_wrapper.o adaptive_btree_extension.o 2>nul

echo.
echo ========================================
echo SUCCESS! 64-bit Extension Built
echo ========================================
echo.
dir adaptive_btree.dll | findstr "adaptive_btree"
echo.
echo Test:
echo   sqlite3.exe test.db
echo   .load adaptive_btree
echo   SELECT adaptive_set_algorithm('regression');
echo.
goto end

:error
echo.
echo ========================================
echo Build Failed
echo ========================================
echo.
echo If you get "unrecognized option '-m64'":
echo   Your MinGW is 32-bit only
echo   Solution: Download 64-bit MinGW or rebuild in 32-bit
echo.
exit /b 1

:end