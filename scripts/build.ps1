param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Preset = "windows-$($Configuration.ToLowerInvariant())"
$Output = Join-Path $Root "build/windows/bin/$Configuration/invoice_helper.exe"

if (-not (Test-Path (Join-Path $Root 'tools/vcpkg/scripts/buildsystems/vcpkg.cmake'))) {
    throw 'vcpkg is not installed. Run bootstrap-vcpkg.ps1 or bash scripts/bootstrap-vcpkg.sh first.'
}

$LocalCMake = Join-Path $Root 'tools/cmake/bin/cmake.exe'
$CMake = if (Test-Path $LocalCMake) { $LocalCMake } else { (Get-Command cmake -ErrorAction Stop).Source }

& $CMake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $CMake --build --preset $Preset --parallel 4
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Build complete: $Output"
