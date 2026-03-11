@echo off
REM ============================================================================
REM generate_placeholder_logos.bat
REM Creates minimal placeholder PNG files for the UWP package manifest.
REM These are 1x1 green pixel PNGs at the required sizes.
REM Replace them with actual Minecraft logos before final packaging.
REM ============================================================================
REM
REM Required sizes for Package.appxmanifest:
REM   Square44x44Logo.png     - 44x44
REM   Square150x150Logo.png   - 150x150
REM   Wide310x150Logo.png     - 310x150
REM   Square310x310Logo.png   - 310x310
REM   SplashScreen.png        - 620x300
REM   StoreLogo.png           - 50x50
REM
REM This script creates minimal valid PNG files using PowerShell.
REM ============================================================================

echo Creating placeholder UWP logo assets...

powershell -Command ^
  "Add-Type -AssemblyName System.Drawing; ^
   $sizes = @{ ^
     'Square44x44Logo'=@(44,44); ^
     'Square150x150Logo'=@(150,150); ^
     'Wide310x150Logo'=@(310,150); ^
     'Square310x310Logo'=@(310,310); ^
     'SplashScreen'=@(620,300); ^
     'StoreLogo'=@(50,50) ^
   }; ^
   foreach($name in $sizes.Keys) { ^
     $w=$sizes[$name][0]; $h=$sizes[$name][1]; ^
     $bmp = New-Object System.Drawing.Bitmap($w,$h); ^
     $g = [System.Drawing.Graphics]::FromImage($bmp); ^
     $g.Clear([System.Drawing.Color]::FromArgb(255,34,139,34)); ^
     $font = New-Object System.Drawing.Font('Arial',([math]::Min($w,$h)/4)); ^
     $g.DrawString('MC', $font, [System.Drawing.Brushes]::White, 4, 4); ^
     $g.Dispose(); ^
     $bmp.Save('%~dp0Assets\' + $name + '.png', [System.Drawing.Imaging.ImageFormat]::Png); ^
     $bmp.Dispose(); ^
     Write-Host ('  Created ' + $name + '.png (' + $w + 'x' + $h + ')') ^
   }"

echo Done! Placeholder logos created in Assets\ folder.
echo Replace these with actual game logos before distributing.
pause
