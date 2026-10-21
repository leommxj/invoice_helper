$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$VcpkgDir = Join-Path $Root 'tools/vcpkg'

New-Item -ItemType Directory -Force -Path (Join-Path $Root 'tools') | Out-Null

if (-not (Test-Path (Join-Path $VcpkgDir '.git'))) {
    git clone --depth 1 https://github.com/microsoft/vcpkg.git $VcpkgDir
}

Push-Location $Root
try {
    & (Join-Path $VcpkgDir 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}

Write-Host "vcpkg is ready: $VcpkgDir"
Write-Host 'Next: scripts\build.ps1 -Configuration Release'
