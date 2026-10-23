$ErrorActionPreference = 'Stop'

# Build script for Windows 10/11 with Visual Studio 2022 and CMake.
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root 'build'

cmake -S $Root -B $Build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }

cmake --build $Build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }

Write-Host ''
Write-Host 'Build completed. Looking for the VST3 and Standalone outputs...'
Get-ChildItem $Build -Recurse -Directory -Filter '*.vst3' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
Get-ChildItem $Build -Recurse -File -Filter '*.exe' -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match '[\\/]Standalone[\\/]' } | Select-Object -ExpandProperty FullName
