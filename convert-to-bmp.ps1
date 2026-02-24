# Convert PNG to 24-bit BMP for NSIS installer
Add-Type -AssemblyName System.Drawing

function Convert-PngToBmp {
    param(
        [string]$PngPath,
        [string]$BmpPath
    )
    
    $pngFullPath = Resolve-Path $PngPath
    $img = [System.Drawing.Image]::FromFile($pngFullPath)
    $bmp = New-Object System.Drawing.Bitmap($img.Width, $img.Height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    
    $graphics = [System.Drawing.Graphics]::FromImage($bmp)
    $graphics.Clear([System.Drawing.Color]::White)
    $graphics.DrawImage($img, 0, 0, $img.Width, $img.Height)
    $graphics.Dispose()
    
    $bmp.Save($BmpPath, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $bmp.Dispose()
    $img.Dispose()
    
    Write-Host "Successfully converted $PngPath to $BmpPath"
}

# Convert installer graphics
Convert-PngToBmp -PngPath "assets\installerSidebar.png" -BmpPath "assets\installerSidebar.bmp"
Convert-PngToBmp -PngPath "assets\installerHeader.png" -BmpPath "assets\installerHeader.bmp"

Write-Host "`nBMP files generated successfully!"
