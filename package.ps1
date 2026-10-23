$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root 'build'
$Dist = Join-Path $Root 'dist'
New-Item -ItemType Directory -Force -Path $Dist | Out-Null

$Vst3 = Get-ChildItem $Build -Recurse -Directory -Filter 'GaelRack Fx.vst3' -ErrorAction SilentlyContinue | Select-Object -First 1
$Exe = Get-ChildItem $Build -Recurse -File -Filter '*.exe' -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match '[\\/]Standalone[\\/]' } | Select-Object -First 1
if (-not $Vst3) { throw 'VST3 bundle not found. Run .\build.ps1 first.' }
if (-not $Exe) { Write-Warning 'Standalone executable not found; packaging the VST3 only.' }

$Package = Join-Path $Dist 'GaelRackFx-Windows-x64'
if (Test-Path $Package) { Remove-Item $Package -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Package | Out-Null
Copy-Item $Vst3.FullName -Destination $Package -Recurse
if ($Exe) { Copy-Item $Exe.FullName -Destination $Package }
Copy-Item (Join-Path $Root 'README_FR.md') -Destination $Package
$Zip = Join-Path $Dist 'GaelRackFx-Windows-x64.zip'
if (Test-Path $Zip) { Remove-Item $Zip -Force }
Compress-Archive -Path (Join-Path $Package '*') -DestinationPath $Zip
Write-Host "Distribution package created: $Zip"
