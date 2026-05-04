# M-Finlogs System Dependency Checker (PowerShell)
# Run this script to verify all required components are installed

param([switch]$Silent = $false)

$ErrorActionPreference = "Continue"

function Write-CheckResult {
    param([string]$Status, [string]$Message, [string]$Action = "")
    
    $symbol = switch($Status) {
        "PASS"    { "[✓ PASS] " }
        "FAIL"    { "[✗ FAIL] " }
        "WARN"    { "[! WARN] " }
        default   { "[INFO] " }
    }
    
    Write-Host "$symbol$Message" -ForegroundColor $(if($Status -eq "PASS") { "Green" } elseif($Status -eq "FAIL") { "Red" } else { "Yellow" })
    
    if ($Action) {
        Write-Host "         ACTION: $Action" -ForegroundColor Cyan
    }
}

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "          M-Finlogs: System Dependency Checker" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

$checksPasssed = 0
$checksFailed = 0
$checksWarning = 0

# ===== Check for ODBC Driver 17 =====
Write-Host "[1/5] Checking for ODBC Driver 17 for SQL Server..." -ForegroundColor White

try {
    $odbcDriver = Get-ItemProperty "HKLM:\SOFTWARE\ODBC\ODBCINST.INI\ODBC Driver 17 for SQL Server" -ErrorAction Stop
    Write-CheckResult "PASS" "ODBC Driver 17 is installed"
    $checksPasssed++
} catch {
    Write-CheckResult "FAIL" "ODBC Driver 17 NOT FOUND" `
        "Download from: https://learn.microsoft.com/en-us/sql/connect/odbc/download-odbc-driver-for-sql-server"
    $checksFailed++
}
Write-Host ""

# ===== Check for SQL Server =====
Write-Host "[2/5] Checking for SQL Server installation..." -ForegroundColor White

try {
    $sqlServer = Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Microsoft SQL Server" -ErrorAction Stop
    Write-CheckResult "PASS" "SQL Server is installed"
    $checksPasssed++
} catch {
    Write-CheckResult "WARN" "SQL Server NOT FOUND" `
        "Download from: https://www.microsoft.com/en-us/sql-server/sql-server-express"
    $checksWarning++
}
Write-Host ""

# ===== Check for SQL Command-Line Tools =====
Write-Host "[3/5] Checking for sqlcmd (SQL Server tools)..." -ForegroundColor White

$sqlcmdPath = Get-Command sqlcmd -ErrorAction SilentlyContinue
if ($sqlcmdPath) {
    Write-CheckResult "PASS" "SQL command-line tools found at: $($sqlcmdPath.Source)"
    $checksPasssed++
} else {
    Write-CheckResult "WARN" "sqlcmd not found in PATH" `
        "Optional: Add SQL Server tools to your PATH or install SSMS"
    $checksWarning++
}
Write-Host ""

# ===== Check for .NET Framework =====
Write-Host "[4/5] Checking for .NET Framework..." -ForegroundColor White

try {
    $dotnet = Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full" -ErrorAction Stop
    $version = $dotnet.Release
    Write-CheckResult "PASS" ".NET Framework is installed (Release: $version)"
    $checksPasssed++
} catch {
    Write-CheckResult "WARN" ".NET Framework not found" `
        "Optional: Install .NET Framework if needed"
    $checksWarning++
}
Write-Host ""

# ===== Check for Python (optional, for dev/source mode) =====
Write-Host "[5/5] Checking for Python (optional, for development)..." -ForegroundColor White

$pythonPath = Get-Command python -ErrorAction SilentlyContinue
if ($pythonPath) {
    $pythonVersion = python --version 2>&1
    Write-CheckResult "PASS" "Python found: $pythonVersion"
    $checksPasssed++
} else {
    Write-CheckResult "WARN" "Python not found (OK for packaged app)" `
        "Only needed if running from source code"
    $checksWarning++
}
Write-Host ""

# ===== Summary =====
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "                        SUMMARY" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "Passed:   $checksPasssed ✓" -ForegroundColor Green
Write-Host "Failed:   $checksFailed ✗" -ForegroundColor Red
Write-Host "Warnings: $checksWarning !" -ForegroundColor Yellow
Write-Host ""

if ($checksFailed -gt 0) {
    Write-Host "[✗] INSTALLATION INCOMPLETE" -ForegroundColor Red
    Write-Host ""
    Write-Host "You must install the missing components before M-Finlogs can work." -ForegroundColor White
    Write-Host "See SETUP_GUIDE.md for detailed instructions." -ForegroundColor White
    Write-Host ""
    
    if (-not $Silent) {
        Read-Host "Press Enter to exit"
    }
    exit 1
}

if ($checksWarning -gt 0) {
    Write-Host "[! ] READY WITH WARNINGS" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Your system may need minor adjustments. See above for details." -ForegroundColor White
    Write-Host "You can proceed with M-Finlogs installation." -ForegroundColor White
    Write-Host ""
    
    if (-not $Silent) {
        Read-Host "Press Enter to exit"
    }
    exit 0
}

Write-Host "[✓] ALL CHECKS PASSED" -ForegroundColor Green
Write-Host ""
Write-Host "Your system is ready for M-Finlogs installation!" -ForegroundColor Green
Write-Host ""

if (-not $Silent) {
    Read-Host "Press Enter to exit"
}
exit 0
