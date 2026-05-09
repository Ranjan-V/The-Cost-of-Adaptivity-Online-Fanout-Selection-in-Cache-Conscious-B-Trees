# SQLite Integration Guide

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
cd integration\sqlite
build.bat
```
