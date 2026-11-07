<#
.SYNOPSIS
    Builds Windows Device Telemetry Monitor using CMake and MSVC.

.DESCRIPTION
    Automates the CMake configuration, compilation, and optional test execution
    for the Windows Device Telemetry Monitor project.

.PARAMETER Config
    Build configuration (e.g., Release, Debug, RelWithDebInfo). Default: Release.

.PARAMETER RunTests
    Switch indicating whether to execute CTest test suites after a successful build.

.PARAMETER Clean
    Switch indicating whether to remove the build directory before building.

.EXAMPLE
    .\scripts\build.ps1 -Config Release -RunTests
#>

[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Config = "Release",

    [switch]$RunTests,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RootDir "build"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Windows Device Telemetry Monitor - Build Script" -ForegroundColor Cyan
Write-Host "  Configuration: $Config" -ForegroundColor Cyan
Write-Host "  Directory:     $RootDir" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "[*] Cleaning existing build directory: $BuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Ensure build directory exists
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Write-Host "`n[*] Configuring CMake..." -ForegroundColor Green
$cmakeConfigArgs = @(
    "-B", $BuildDir,
    "-S", $RootDir,
    "-DCMAKE_BUILD_TYPE=$Config"
)

# If Visual Studio is present, target x64 architecture
if ($env:VisualStudioVersion -or (Get-Command "vswhere.exe" -ErrorAction SilentlyContinue)) {
    $cmakeConfigArgs += @("-A", "x64")
}

& cmake @cmakeConfigArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "[-] CMake configuration failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}

Write-Host "`n[*] Compiling ($Config)..." -ForegroundColor Green
& cmake --build $BuildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Error "[-] Build failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}

Write-Host "[+] Build completed successfully!" -ForegroundColor Green

if ($RunTests) {
    Write-Host "`n[*] Running CTest Unit Tests..." -ForegroundColor Green
    & ctest --test-dir $BuildDir -C $Config --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[-] CTest execution reported test failures."
        exit $LASTEXITCODE
    }
    Write-Host "[+] All CTest test suites passed successfully!" -ForegroundColor Green
}
