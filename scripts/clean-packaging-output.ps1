$outputDir = Join-Path $PSScriptRoot '..\release\desktop-app-build'
$outputDir = [System.IO.Path]::GetFullPath($outputDir)

if (Test-Path $outputDir) {
    Write-Host "[build] Removing previous desktop output: $outputDir"
    $removed = $false
    for ($attempt = 1; $attempt -le 5; $attempt++) {
        try {
            Remove-Item -LiteralPath $outputDir -Recurse -Force -ErrorAction Stop
            $removed = $true
            break
        } catch {
            if ($attempt -lt 5) {
                Write-Host "[build] Remove attempt $attempt failed, retrying in 2s: $($_.Exception.Message)"
                Start-Sleep -Seconds 2
            } else {
                Write-Host "[build] Failed to remove desktop output after retries: $($_.Exception.Message)"
                exit 1
            }
        }
    }

    if ($removed) {
        Write-Host '[build] Previous desktop output removed.'
    }
}

Write-Host '[build] Desktop output is clean.'