# M-Finlogs Desktop App - Complete Setup Guide

## For New Laptop Installation

### Prerequisites (Required Before Installing M-Finlogs)

#### 1. **SQL Server** ✓ Required
- **Express** (free): https://www.microsoft.com/en-us/sql-server/sql-server-express
- **Developer** (free): https://www.microsoft.com/en-us/sql-server/sql-server-downloads
- **Standard/Enterprise** (paid): Available from your organization

**Choose Mixed Mode Authentication during installation** (allows SQL logins)

#### 2. **ODBC Driver 17 for SQL Server** ✓ CRITICAL
- Download from Microsoft: https://learn.microsoft.com/en-us/sql/connect/odbc/download-odbc-driver-for-sql-server
- **Windows 10/11**: Download "ODBC Driver 17 for SQL Server (msodbcsql.msi)"
- Run the installer and follow the default steps
- **This is required for the app to connect to the database**

#### 3. **SQL Server Management Studio (SSMS)** ✓ Recommended
- Download: https://learn.microsoft.com/en-us/sql/ssms/download-sql-server-management-studio-ssms
- Optional but highly recommended for database management and troubleshooting
- Allows you to create/manage databases without command line

---

### Step-by-Step Installation

#### Step 1: Install Prerequisites (if not already installed)
1. Install **SQL Server** (see links above)
2. Install **ODBC Driver 17 for SQL Server** (see links above)
3. Optionally install **SSMS** (see links above)

#### Step 2: Install M-Finlogs Desktop App
1. Run `M-Finlogs Setup <version>.exe`
2. Follow the installer wizard
3. Choose installation directory (default is fine)
4. Wait for installation to complete

#### Step 3: Configure Database Connection
1. Launch M-Finlogs from Start Menu or Desktop
2. Click **"Configure Database"** button
3. Enter your SQL Server details:
   - **Server Name**: `localhost` or `localhost\SQLEXPRESS` (depending on SQL Server instance)
   - **Database Name**: `m_finlogs` (or create a new one)
   - **Username**: `sa` (default SQL Server admin) or your SQL login
   - **Password**: Your SQL Server password
   - **API Server URL**: `http://127.0.0.1:8000` (for local use)
4. Click **"Save"** and restart the app

#### Step 4: Create Database (if needed)
If the database `m_finlogs` doesn't exist, you can create it:

**Option A: Using SSMS (Recommended)**
1. Open SQL Server Management Studio
2. Connect to your SQL Server
3. Right-click "Databases" → "New Database"
4. Name: `m_finlogs`
5. Click "OK"

**Option B: Using Command Line**
```cmd
sqlcmd -S localhost -U sa -P YourPassword
> CREATE DATABASE m_finlogs;
> GO
```

#### Step 5: Start Using the App
- The app should now be fully functional
- You can begin importing and processing financial logs

---

### Troubleshooting

#### ❌ "Cannot connect to SQL Server"
- Verify SQL Server is installed and running
- Check SQL Server name (usually `localhost` or `localhost\SQLEXPRESS`)
- Ensure ODBC Driver 17 is installed
- Check firewall settings (SQL Server might be blocked)

#### ❌ "ODBC Driver not found"
- **Solution**: Install ODBC Driver 17 for SQL Server from Microsoft's website
- https://learn.microsoft.com/en-us/sql/connect/odbc/download-odbc-driver-for-sql-server

#### ❌ "Login failed for user 'sa'"
- Verify username and password
- Ensure you're using Mixed Mode authentication for SQL Server
- Check that the user has appropriate database permissions

#### ❌ "Database doesn't exist"
- Create the database using SSMS or the command line (see Step 4)
- Or the app will auto-create tables on first connection if the database exists

#### ❌ "Cannot find SSMS or Management Tools"
- Optional: Install SQL Server Management Studio from Microsoft
- Or create the database using command line tools

---

### System Requirements

| Component | Requirement |
|-----------|-------------|
| OS | Windows 10 or Windows 11 |
| RAM | 4 GB minimum (8 GB recommended) |
| Disk Space | 500 MB minimum |
| SQL Server | Express (free) or higher |
| ODBC Driver | Version 17 for SQL Server |
| Network | Local network connectivity to SQL Server |

---

### For Server Installation (Running Backend 24/7)

See [README.md](README.md) for Server Mode setup instructions.

**Required:**
1. SQL Server running on server machine
2. ODBC Driver 17 installed
3. Python (if running from source) or the packaged backend.exe
4. Open Windows Firewall port 8000 for clients to connect

---

### For Network Deployment (Multiple Clients)

1. **Server Machine**:
   - Install SQL Server
   - Install ODBC Driver 17
   - Run: `npm run build:server` to create backend binary
   - Run backend.exe or Python backend

2. **Client Machines**:
   - Install M-Finlogs desktop app
   - When configuring database, use: `http://SERVER_IP:8000` for API Server URL
   - Enter your SQL Server details
   - Save and restart

---

### Getting Help

If you encounter issues:
1. Check the Troubleshooting section above
2. Verify all prerequisites are installed
3. Check Windows Event Viewer for SQL Server errors
4. Check firewall settings if networking is involved

---

**Last Updated**: May 2, 2026
