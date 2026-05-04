# M-Finlogs: System Requirements & Dependency Guide

## Minimum System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **Operating System** | Windows 10 Build 1909+ | Windows 11 |
| **RAM** | 4 GB | 8 GB or more |
| **Disk Space** | 500 MB free | 2 GB free |
| **Display** | 1920×1080 | 1920×1080 or higher |
| **Internet** | Not required* | Local network if using server mode |

*Desktop app works offline; server mode requires network connectivity between client and server

---

## Required Components (MUST Install Before M-Finlogs)

### 1. SQL Server Database Engine
**Why needed**: Stores all financial data, users, configurations

**Installation options**:
- **SQL Server Express** (FREE) → https://www.microsoft.com/en-us/sql-server/sql-server-express
- **SQL Server Developer** (FREE) → https://www.microsoft.com/en-us/sql-server/sql-server-downloads
- **SQL Server Standard/Enterprise** (PAID) → Through your organization

**Installation notes**:
- Select **Mixed Mode** authentication during setup
- Default instance name: `MSSQLSERVER` or `SQLEXPRESS`
- Choose default data folder
- Allocate sufficient space for your financial data

**Time to install**: ~10-15 minutes

---

### 2. ODBC Driver 17 for SQL Server
**Why needed**: Allows the app to communicate with SQL Server database

**Download**: https://learn.microsoft.com/en-us/sql/connect/odbc/download-odbc-driver-for-sql-server

**Installation options**:

| OS | Download | File |
|----|----------|------|
| Windows 10/11 (64-bit) | msodbcsql.msi | ODBC Driver 17 for SQL Server |
| Windows 10/11 (32-bit) | msodbcsql.msi | ODBC Driver 17 for SQL Server (x86) |

**Installation notes**:
- Run installer with administrator privileges
- Follow default options
- No restart needed (but restart is safe)
- Verify installation by running `CHECK_SYSTEM.bat`

**Time to install**: ~5 minutes

⚠️ **WITHOUT THIS, THE APP CANNOT CONNECT TO THE DATABASE**

---

## Optional Components (Recommended)

### SQL Server Management Studio (SSMS)
**Why useful**: Visual database management, query execution, troubleshooting

**Download**: https://learn.microsoft.com/en-us/sql/ssms/download-sql-server-management-studio-ssms

**Use cases**:
- Create/manage databases without command line
- Execute custom SQL queries
- View database structure and data
- Troubleshoot connection issues

**Time to install**: ~10-20 minutes (includes SQL Server Express if not already installed)

---

### Python (For Development Only)
**Why needed**: Only if you're compiling the app from source code

**Download**: https://www.python.org/downloads/

**Version**: Python 3.9+ (3.10 or 3.11 recommended)

**What's included** in the M-Finlogs desktop installer:
- ✓ Python runtime (bundled with PyInstaller)
- ✓ All required Python packages (fastapi, uvicorn, pandas, openpyxl, pyodbc, etc.)
- ✓ Backend service runner

**Time to install**: ~10 minutes (only if needed)

---

## Component Dependency Tree

```
M-Finlogs Desktop App
├── REQUIRED:
│   ├── Windows 10/11
│   ├── SQL Server (Express minimum)
│   └── ODBC Driver 17 for SQL Server
│
├── OPTIONAL (included in installer):
│   ├── Python (runtime bundled via PyInstaller)
│   ├── FastAPI
│   ├── Uvicorn
│   ├── Pandas
│   ├── Openpyxl
│   ├── PyODBC
│   ├── ReportLab
│   └── Other Python dependencies
│
└── OPTIONAL (for development):
    ├── SQL Server Management Studio
    ├── Node.js / npm (for building)
    ├── Python 3.9+ (for source modifications)
    └── C++ compiler (for native modules)
```

---

## Installation Verification Checklist

After installing M-Finlogs, verify everything is working:

- [ ] M-Finlogs appears in Start Menu
- [ ] App launches without errors
- [ ] "Configure Database" button is accessible
- [ ] Can enter SQL Server credentials
- [ ] Connection test succeeds
- [ ] Database is created or selected
- [ ] Can see main application interface

---

## Dependency Installation Command Summary

### Batch File (Recommended)
```cmd
CHECK_SYSTEM.bat
```
Opens an interactive checker that verifies all dependencies

### PowerShell (Advanced Users)
```powershell
powershell -ExecutionPolicy Bypass -File Check-System.ps1
```
Detailed JSON output available with `-OutputFormat JSON` parameter

---

## Troubleshooting by Error

| Error | Cause | Solution |
|-------|-------|----------|
| "Cannot connect to SQL Server" | SQL Server not running or invalid credentials | Check SQL Server service; verify credentials in SQL Server Management Studio |
| "ODBC Driver 17 not found" | ODBC driver not installed | Download and install from Microsoft link above |
| "Database m_finlogs not found" | Database not created | Use SSMS or SQL command to create database |
| "Login failed for user" | Incorrect SQL credentials | Verify username/password in SQL Server |
| "Connection timeout" | Firewall blocking connection | Check Windows Firewall settings for SQL Server |

---

## For Server Deployment (Multiple Clients)

Additional requirements:
- Network connectivity between client and server (port 8000)
- Server machine should have:
  - SQL Server always running
  - ODBC Driver 17 installed
  - Sufficient disk space for database
  - Automatic backup solution (recommended)

See [README.md](README.md) for Server Mode setup.

---

## Getting Help

1. **Check**: Run `CHECK_SYSTEM.bat` to verify dependencies
2. **Read**: Review [SETUP_GUIDE.md](SETUP_GUIDE.md) for step-by-step instructions
3. **Verify**: Ensure all required components are installed
4. **Test**: Use SQL Server Management Studio to test database connection

---

**Last Updated**: May 2, 2026  
**Version**: 0.1.3+
