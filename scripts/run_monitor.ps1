<#
.SYNOPSIS
    Runs the Windows Device Telemetry Monitor executable.

.DESCRIPTION
    Locates the compiled device_telemetry.exe binary and invokes it with
    user-specified or default parameters.

.PARAMETER IntervalMs
    Sampling interval in milliseconds [250 - 60000]. Default: 1000.

.PARAMETER DurationSeconds
    Monitoring duration in seconds. Default: 60.

.PARAMETER OutputDir
    Directory path for CSV output. Default: output.

.PARAMETER TopProcesses
    Number of top CPU processes to monitor. Default: 20.

.PARAMETER IncludeProcesses
    Boolean switch to include or exclude process monitoring. Default: $true.

.EXAMPLE
    .\scripts\run_monitor.ps1 -IntervalMs 500 -DurationSeconds 30 -TopProcesses 10
#>

[CmdletBinding()]
param(
    [ValidateRange(250, 60000)]
    [int]$IntervalMs = 1000,

    [ValidateRange(1, [int]::MaxValue)]
    [int]$DurationSeconds = 60,

    [string]$OutputDir = "output",

    [ValidateRange(0, 1000)]
    [int]$TopProcesses = 20,

    [bool]$IncludeProcesses = $true
)

$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $PSScriptRoot
$Candidates = @(
    (Join-Path $RootDir "build\Release\device_telemetry.exe"),
    (Join-Path $RootDir "build\device_telemetry.exe"),
    (Join-Path $RootDir "build\Debug\device_telemetry.exe"),
    (Join-Path $RootDir "bin\device_telemetry.exe")
)

$ExePath = $null
foreach ($path in $Candidates) {
    if (Test-Path $path) {
        $ExePath = $path
        break
    }
}

if (-not $ExePath) {
    Write-Error "[-] device_telemetry.exe could not be found. Please build the project first using .\scripts\build.ps1."
    exit 1
}

Write-Host "[+] Found executable: $ExePath" -ForegroundColor Cyan
Write-Host "[+] Launching telemetry monitor..." -ForegroundColor Cyan

$arguments = @(
    "--interval-ms", $IntervalMs,
    "--duration-seconds", $DurationSeconds,
    "--output-dir", $OutputDir,
    "--top-processes", $TopProcesses,
    "--include-processes", ($IncludeProcesses.ToString().ToLower())
)

& $ExePath @arguments
exit $LASTEXITCODE
