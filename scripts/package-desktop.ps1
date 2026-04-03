$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$electronBuilder = Join-Path $repoRoot 'node_modules\.bin\electron-builder.cmd'

if (-not (Test-Path $electronBuilder)) {
    Write-Error "electron-builder not found at $electronBuilder"
    exit 1
}

for ($attempt = 1; $attempt -le 3; $attempt++) {
    Write-Host "[build] Desktop package attempt $attempt/3"
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot 'scripts\close-packaging-processes.ps1')
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot 'scripts\clean-packaging-output.ps1')

    $env:ComSpec = 'C:\Windows\System32\cmd.exe'
    $env:PATH = "$env:SystemRoot\System32;$env:PATH"

    & $electronBuilder
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq 0) {
        exit 0
    }

    Write-Host "[build] Desktop package attempt $attempt failed with exit code $exitCode"
    if ($attempt -lt 3) {
        Start-Sleep -Seconds 5
    }
}

exit 1