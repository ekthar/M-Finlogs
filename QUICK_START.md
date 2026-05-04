# M-Finlogs Quick Reference Guide

A one-page summary for users who need to get M-Finlogs running quickly.

---

## TL;DR - Absolute Minimum Steps

1. **Install SQL Server Express** (free)
   - https://www.microsoft.com/en-us/sql-server/sql-server-express
   - Choose "Mixed Mode" authentication
   - Remember your sa password

2. **Install ODBC Driver 17 for SQL Server** (CRITICAL!)
   - https://learn.microsoft.com/en-us/sql/connect/odbc/download-odbc-driver-for-sql-server
   - Run `msodbcsql.msi` installer

3. **Run system check** to verify:
   - Double-click: `CHECK_SYSTEM.bat`
   - Should show all ✓ PASS

4. **Install M-Finlogs** desktop app
   - Run: `M-Finlogs Setup <version>.exe`
   - Follow wizard

5. **Configure database**
   - Server: `localhost\SQLEXPRESS`
   - Database: `m_finlogs`
   - Username: `sa`
   - Password: (your SQL Server password)
   - Save & restart

**Done!** ✓

---

## Documentation Files

| File | Purpose | Read Time |
|------|---------|-----------|
| **INSTALLATION_CHECKLIST.md** | Step-by-step with checkboxes | 10 min |
| **SETUP_GUIDE.md** | Complete setup with troubleshooting | 15-20 min |
| **REQUIREMENTS.md** | Detailed component information | 10 min |
| **README.md** | Build instructions and deployment | 5 min |

---

## Quick Fixes for Common Issues

### ❌ "ODBC Driver 17 not found"
→ Install from: https://learn.microsoft.com/en-us/sql/connect/odbc/download-odbc-driver-for-sql-server

### ❌ "Cannot connect to SQL Server"
→ Verify SQL Server is running: Press Windows+R → type `services.msc` → find SQL Server → should say "Running"

### ❌ "Database m_finlogs not found"
→ Create it with:
```sql
sqlcmd -S localhost -U sa -P YourPassword
CREATE DATABASE m_finlogs;
GO
```

### ❌ "Login failed for user 'sa'"
→ Check username and password match what you set during SQL Server installation

---

## System Check Tools

Two scripts to verify your system is ready:

### Option 1: Batch File (Easiest)
```cmd
CHECK_SYSTEM.bat
```
- Double-click in Windows Explorer
- Shows visual check with ✓ and ✗ symbols
- Tells you what's missing

### Option 2: PowerShell (More Detail)
```powershell
powershell -ExecutionPolicy Bypass -File Check-System.ps1
```
- More detailed output
- Use if batch file doesn't work

---

## For Multi-User Setup (Network)

1. Install M-Finlogs on **server machine**
2. Install M-Finlogs on **each client machine**
3. When configuring each client, use:
   - Server: `<SERVER_IP_ADDRESS>`
   - Database: `m_finlogs` (same as server)
   - Username: `sa` or shared SQL login
   - Password: (same as server)
   - **API Server URL: `http://<SERVER_IP>:8000`** ← Important!
4. All clients will use the same database

---

## Support

**Start here**: [INSTALLATION_CHECKLIST.md](INSTALLATION_CHECKLIST.md)

**Need more detail?**: [SETUP_GUIDE.md](SETUP_GUIDE.md)

**Component info?**: [REQUIREMENTS.md](REQUIREMENTS.md)

---

**Version**: 0.1.3+  
**Last Updated**: May 2, 2026
