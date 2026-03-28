$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$distBackend = Join-Path $repoRoot 'dist\backend.exe'

if (-not (Test-Path $distBackend)) {
    Write-Error 'dist\backend.exe not found. Run npm run build:server first.'
}

$packageJsonPath = Join-Path $repoRoot 'package.json'
$pkg = Get-Content $packageJsonPath -Raw | ConvertFrom-Json
$version = $pkg.version

$releaseRoot = Join-Path $repoRoot 'release'
$serverDir = Join-Path $releaseRoot ("server-" + $version)

if (Test-Path $serverDir) {
    Remove-Item -Path $serverDir -Recurse -Force
}
New-Item -ItemType Directory -Path $serverDir | Out-Null

Copy-Item $distBackend (Join-Path $serverDir 'backend.exe') -Force

$dbConfigSrc = Join-Path $repoRoot 'db_config.json'
if (Test-Path $dbConfigSrc) {
    Copy-Item $dbConfigSrc (Join-Path $serverDir 'db_config.json') -Force
} else {
    '{}' | Out-File -FilePath (Join-Path $serverDir 'db_config.json') -Encoding utf8 -NoNewline
}

$readme = @"
M-Finlogs Server Package
========================

Files:
- backend.exe
- db_config.json

Run on server:
1. Install Microsoft ODBC Driver for SQL Server.
2. Edit db_config.json for your SQL server settings if needed.
3. Run backend.exe
4. Keep port 8000 open in Windows Firewall.

Note:
- backend.exe uses JWT secret from environment variable JWT_SECRET_KEY when provided.
- If JWT secret is not provided, ensure your deployment process sets one.
"@

$readme | Out-File -FilePath (Join-Path $serverDir 'README-SERVER.txt') -Encoding utf8

$zipPath = Join-Path $releaseRoot ("M-Finlogs-Server-" + $version + '.zip')
if (Test-Path $zipPath) {
    Remove-Item -Path $zipPath -Force
}
Compress-Archive -Path (Join-Path $serverDir '*') -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Server package ready: $zipPath"
