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

    # This script only builds/packages the app; it never publishes a GitHub
    # release itself (release creation is handled separately by CI, e.g. via
    # the "Electron App Build" workflow's tag-triggered release job). Without
    # --publish never, electron-builder detects CI and tries to construct a
    # GitHubPublisher for cleanup/upload, which fails hard if GH_TOKEN isn't
    # set - even though we never asked it to publish anything.
    & $electronBuilder --publish never
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