@echo off
REM Build SQLite Adaptive Extension - 32-bit Version

echo.
echo ========================================
echo Building SQLite Adaptive Extension (32-bit)
echo ========================================
echo.

set SQLITE_DIR=sqlite-amalgamation-3510200

echo [Setup] Copying SQLite headers...
if exist %SQLITE_DIR%\sqlite3ext.h copy %SQLITE_DIR%\sqlite3ext.h . >nul 2>&1
if exist %SQLITE_DIR%\sqlite3.h copy %SQLITE_DIR%\sqlite3.h . >nul 2>&1
echo   ✓ Headers copied

echo.
echo [1/3] Compiling C++ wrapper (32-bit)...
g++ -m32 -c -O2 -std=c++11 -I. adaptive_wrapper_impl.cpp -o adaptive_wrapper.o
if %ERRORLEVEL% NEQ 0 goto error
echo   ✓ Done

echo.
echo [2/3] Compiling C extension (32-bit)...
gcc -m32 -c -O2 -I. adaptive_btree_extension.c -o adaptive_btree_extension.o
if %ERRORLEVEL% NEQ 0 goto error
echo   ✓ Done

echo.
echo [3/3] Linking DLL (32-bit)...
g++ -m32 -shared adaptive_wrapper.o adaptive_btree_extension.o -o adaptive_btree.dll
if %ERRORLEVEL% NEQ 0 goto error
echo   ✓ Done

del adaptive_wrapper.o adaptive_btree_extension.o 2>nul

echo.
echo ========================================
echo SUCCESS! 32-bit Extension Built
echo ========================================
echo.
goto end

:error
echo Build failed
exit /b 1

:end