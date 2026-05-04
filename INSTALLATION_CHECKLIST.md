# M-Finlogs Installation Checklist

Print this page and follow along as you install M-Finlogs on your new laptop.

---

## Phase 1: Install Required Components (30-40 minutes)

### ☐ 1. Install SQL Server
- [ ] Download SQL Server Express from: https://www.microsoft.com/en-us/sql-server/sql-server-express
- [ ] Run installer
- [ ] Choose "Mixed Mode" authentication
- [ ] Set sa password (remember this!)
- [ ] Complete installation
- [ ] Verify SQL Server is running in Services (Windows + R → services.msc)

**Estimated time**: 15-20 minutes

---

### ☐ 2. Install ODBC Driver 17
- [ ] Download ODBC Driver 17 from: https://learn.microsoft.com/en-us/sql/connect/odbc/download-odbc-driver-for-sql-server
- [ ] Choose: "ODBC Driver 17 for SQL Server (msodbcsql.msi)"
- [ ] Run installer as Administrator (right-click → Run as administrator)
- [ ] Follow default installation steps
- [ ] Installer will complete in ~2-3 minutes

**Estimated time**: 5-10 minutes

**⚠️ CRITICAL**: Without this driver, the M-Finlogs app cannot connect to the database!

---

### ☐ 3. (Optional) Install SQL Server Management Studio
- [ ] Visit: https://learn.microsoft.com/en-us/sql/ssms/download-sql-server-management-studio-ssms
- [ ] Download latest SSMS (currently v20+)
- [ ] Run installer
- [ ] Follow prompts (note: this may take 10-15 minutes)
- [ ] Launch SSMS after installation
- [ ] Test connection to your SQL Server instance

**Estimated time**: 10-20 minutes (optional but recommended)

---

## Phase 2: System Verification (5 minutes)

### ☐ 4. Run System Check
- [ ] Run `CHECK_SYSTEM.bat` (double-click in Windows Explorer)
- [ ] All items should show ✓ PASS (or ! WARN is OK)
- [ ] If anything shows ✗ FAIL, fix it before continuing

**Output should look like:**
```
✓ PASS - ODBC Driver 17 is installed
✓ PASS - SQL Server is installed
✓ PASS - SQL command-line tools found
✓ PASS - .NET Framework is installed
```

---

## Phase 3: Install M-Finlogs App (10 minutes)

### ☐ 5. Install Desktop App
- [ ] Download: `M-Finlogs Setup <version>.exe`
- [ ] Run installer
- [ ] Choose installation folder (default is fine)
- [ ] Wait for installation to complete
- [ ] Check "Launch M-Finlogs" when done

**Estimated time**: 5-10 minutes

---

## Phase 4: Configure Database Connection (5 minutes)

### ☐ 6. First Launch Configuration
The app will show "Configure Database" dialog:

- [ ] **Server Name**: 
  - If using localhost: `localhost`
  - If using SQLEXPRESS: `localhost\SQLEXPRESS`
  - If using remote server: `server_IP_or_name`

- [ ] **Database Name**: 
  - `m_finlogs` (leave as is)

- [ ] **Username**: 
  - `sa` (default SQL Server admin)
  - Or your SQL Server login

- [ ] **Password**: 
  - Your SQL Server password

- [ ] **API Server URL** (for networking):
  - Local use: `http://127.0.0.1:8000`
  - Remote use: `http://SERVER_IP:8000`

- [ ] Click **"Save"**
- [ ] App will restart
- [ ] Verify main interface loads

---

## Phase 5: Create Database (if needed) - 5 minutes

### ☐ 7. Verify or Create Database

**Option A: Using SQL Server Management Studio** (Recommended)
- [ ] Open SSMS
- [ ] Connect to your SQL Server
- [ ] Right-click "Databases" → "New Database"
- [ ] Name: `m_finlogs`
- [ ] Click OK
- [ ] Close SSMS

**Option B: Using Command Line** (If SSMS not installed)
- [ ] Open Command Prompt (Win + R → cmd)
- [ ] Run: `sqlcmd -S localhost -U sa -P YourPassword`
- [ ] Then type: `CREATE DATABASE m_finlogs;`
- [ ] Then type: `GO`
- [ ] Type: `exit`

**Option C: Let the app create it**
- [ ] If database doesn't exist, app may auto-create tables
- [ ] (Check documentation if tables don't appear)

---

## Phase 6: Start Using M-Finlogs ✓

### ☐ 8. Initial Test
- [ ] Launch M-Finlogs from Start Menu
- [ ] Verify no connection errors
- [ ] Try importing a sample file
- [ ] Verify data appears in the app
- [ ] Check settings are saved

🎉 **Installation complete!**

---

## Troubleshooting Quick Reference

| Problem | Action |
|---------|--------|
| **Can't connect to SQL Server** | Check Services (Ctrl+R → services.msc) → "SQL Server" should be running |
| **ODBC Driver error** | Reinstall ODBC Driver 17 from Microsoft link |
| **Database doesn't exist** | Create it using SSMS or command line (see Phase 5) |
| **Login failed** | Double-check username/password; verify Mixed Mode enabled in SQL Server setup |
| **App won't launch** | Restart computer; verify all prerequisites installed |

---

## Estimated Total Time
- Minimum: **30-40 minutes** (with SQL Express only)
- Recommended: **50-70 minutes** (with SSMS for easier management)
- With troubleshooting: **1-2 hours**

---

## Next Steps After Installation

1. **Import your first financial data**
2. **Set up automated backups** (recommended)
3. **For multi-user setup**: Install on other machines with same database details
4. **For server deployment**: See README.md for Server Mode setup

---

**Need help?** See:
- SETUP_GUIDE.md - Detailed instructions
- REQUIREMENTS.md - Component details
- README.md - Advanced deployment

**Last Updated**: May 2, 2026
