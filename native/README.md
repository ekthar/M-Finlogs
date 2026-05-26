# M-Finlogs Native Rewrite

This tree is the C++/Qt 6 rewrite target. It is intentionally separate from the current Electron/Python application while parity is built.

## Build Prerequisites

- Windows
- MSVC Build Tools
- CMake 3.25+
- Qt 6.5+ with Widgets, Sql, Network, Charts, PrintSupport, and HttpServer
- vcpkg dependencies from `vcpkg.json`
- SQL Server ODBC Driver 17 or newer

## Runtime Modes

```powershell
mfinlogs-native.exe
mfinlogs-native.exe --server
mfinlogs-native.exe --server-install
mfinlogs-native.exe --server-uninstall
mfinlogs-native.exe --server-start
mfinlogs-native.exe --server-stop
```

The GUI mode calls C++ services directly. Server mode exposes a LAN REST surface backed by the same services.

GitHub Actions packages the executable with Qt, vcpkg runtime DLLs, fonts, and `db_config.json`.
