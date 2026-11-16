# Builds winhide.exe with MinGW-w64 (static, single file, no DLLs).
# Usage: .\build.ps1            normal build
#        .\build.ps1 -Clean     remove build/ first

param([switch]$Clean)

$ErrorActionPreference = 'Stop'

$gxx = 'C:\msys64\ucrt64\bin\g++.exe'
if (-not (Test-Path $gxx)) {
    $cmd = Get-Command g++ -ErrorAction SilentlyContinue
    if (-not $cmd) { throw 'g++ not found. Install MSYS2 (ucrt64) or add g++ to PATH.' }
    $gxx = $cmd.Source
}

$root = $PSScriptRoot
$sources = Get-ChildItem -Path (Join-Path $root 'src') -Filter *.cpp | ForEach-Object { $_.FullName }
if ($sources.Count -eq 0) { throw 'no sources found in src\' }

if ($Clean) {
    $build = Join-Path $root 'build'
    if (Test-Path $build) { Remove-Item $build -Recurse -Force }
}

$out = Join-Path $root 'winhide.exe'

& $gxx -std=c++20 -O2 -Wall -Wextra -DUNICODE -D_UNICODE -municode `
    -static -static-libgcc -static-libstdc++ -s `
    -o $out @sources -luser32

if ($LASTEXITCODE -ne 0) { throw "build failed (exit $LASTEXITCODE)" }

$size = [math]::Round((Get-Item $out).Length / 1KB, 1)
Write-Host "built $out ($size KB)" -ForegroundColor Green
