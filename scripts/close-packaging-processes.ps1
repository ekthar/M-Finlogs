Write-Host '[build] Closing running Electron/backend processes before packaging...'

$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$repoRootPattern = [Regex]::Escape($repoRoot)
$targetNames = @('electron.exe', 'backend.exe', 'M-Finlogs.exe', 'M-Finlogs Server.exe')

try {
    $matches = Get-CimInstance Win32_Process | Where-Object {
        $name = $_.Name
        $cmd = $_.CommandLine
        ($targetNames -contains $name) -or (
            $cmd -and ($cmd -match $repoRootPattern) -and (
                $cmd -match 'electron' -or
                $cmd -match 'backend\.exe' -or
                $cmd -match 'M-Finlogs'
            )
        )
    }

    foreach ($proc in $matches) {
        try {
            Stop-Process -Id $proc.ProcessId -Force -ErrorAction SilentlyContinue
            Write-Host "[build] Stopped process $($proc.Name) ($($proc.ProcessId))"
        } catch {
            Write-Host "[build] Could not stop process $($proc.Name) ($($proc.ProcessId)): $($_.Exception.Message)"
        }
    }
} catch {
    Write-Host "[build] Process scan failed: $($_.Exception.Message)"
}

Start-Sleep -Seconds 2
Write-Host '[build] Packaging processes cleared.'