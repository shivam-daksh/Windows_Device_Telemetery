# Windows Device Telemetry Monitor

[![Windows Native C++ CI](https://github.com/shivam/Windows_Device_Telemetry/actions/workflows/windows-ci.yml/badge.svg)](.github/workflows/windows-ci.yml)
[![Target Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%20Windows%2011%20x64-blue.svg)](https://microsoft.com)
[![Standard](https://img.shields.io/badge/Language-C%2B%2B20-darkgreen.svg)](https://en.cppreference.com/w/cpp/20)
[![Compiler](https://img.shields.io/badge/Compiler-MSVC%20%2F%20Visual%20Studio%202022-purple.svg)](https://visualstudio.microsoft.com/)
[![License](https://img.shields.io/badge/License-MIT-lightgrey.svg)](LICENSE)

A high-performance, native Windows C++20 application engineered for continuous, low-overhead device-level system telemetry. It samples CPU utilization, physical and virtual memory allocation, per-process resource consumption, and power/battery dynamics, persisting time-series telemetry into structured, RFC 4180-compliant CSV datasets.

This application is built for device telemetry engineering, performance characterization, and hardware diagnostics. It is **not** a Windows Task Manager clone or a graphical dashboard.

---

## Table of Contents

- [Key Capabilities](#key-capabilities)
- [System Requirements](#system-requirements)
- [Architectural Overview](#architectural-overview)
- [Building the Project](#building-the-project)
  - [Automated PowerShell Build](#automated-powershell-build)
  - [Manual CMake Build](#manual-cmake-build)
- [Command-Line Interface](#command-line-interface)
  - [Parameters & Defaults](#parameters--defaults)
  - [Example Commands](#example-commands)
- [Output File Formats](#output-file-formats)
  - [System Metrics (`system_metrics.csv`)](#system-metrics-system_metricscsv)
  - [Process Metrics (`process_metrics.csv`)](#process-metrics-process_metricscsv)
- [Metric Formulations](#metric-formulations)
  - [System CPU Utilization](#system-cpu-utilization)
  - [Process CPU Normalization](#process-cpu-normalization)
  - [PID Reuse Protection](#pid-reuse-protection)
  - [Power & Battery Telemetry](#power--battery-telemetry)
- [Sampling Engine & Drift Prevention](#sampling-engine--drift-prevention)
- [Security, Permissions & Privacy](#security-permissions--privacy)
- [Testing & Quality Assurance](#testing--quality-assurance)
- [Technical Limitations](#technical-limitations)
- [Documentation Index](#documentation-index)

---

## Key Capabilities

- **System CPU Utilization**: Calculates busy percentage from cumulative ring 0 (kernel) and ring 3 (user) ticks via `GetSystemTimes`.
- **Physical & Virtual Memory**: Collects physical total, available, used bytes, and utilization percentage, alongside virtual memory and commit metrics via `GlobalMemoryStatusEx`.
- **Per-Process Telemetry**: Enumerates active processes via `EnumProcesses`, measuring working set bytes, page faults (`GetProcessMemoryInfo`), and process CPU usage (`GetProcessTimes`).
- **Normalized Multi-Core CPU**: Normalizes per-process CPU consumption against logical core count so reported percentages reflect true share of total machine capacity.
- **PID Reuse Protection**: Tracks 64-bit process creation timestamps alongside PIDs to prevent delta calculation errors when Windows recycles PIDs.
- **Power & Battery State**: Collects AC line status, battery status flags, battery charge percentage, and estimated remaining run time via `GetSystemPowerStatus`.
- **Zero Hallucinated Values**: Sentinel values (`255` for battery percentage, `-1` for lifetime) and initial counter baselines are emitted as empty CSV cells (`nullopt`), never converted to misleading zeros.
- **Strict Handle Hygiene (RAII)**: Zero raw owning handles; all Windows `HANDLE` instances are managed by `common::WindowsHandle`.
- **Deterministic Scheduling**: Implements monotonic time-deadline scheduling via `std::chrono::steady_clock` to eliminate sampling drift.
- **Graceful Termination**: Responds to `Ctrl+C` immediately, ensuring all open CSV streams are flushed cleanly without corrupted or partial rows.

---

## System Requirements

- **Operating System**: Windows 10 (1809+) or Windows 11 (64-bit).
- **Architecture**: x64 (AMD64).
- **Compiler**: Visual Studio 2022 (MSVC v143 or newer) supporting C++20.
- **Build System**: CMake 3.20 or newer.
- **SDK**: Windows SDK 10.0.19041.0 or newer.
- **Privileges**: Standard user privileges (administrative elevation is **not** required).

---

## Architectural Overview

```
Windows_Device_Telemetry/
|-- CMakeLists.txt              # Primary CMake build and CTest configuration
|-- README.md                   # Project documentation and developer guide
|-- .gitignore                  # Git exclusions for build and temporary outputs
|
|-- include/
|   |-- collectors/
|   |   |-- system_cpu_collector.h # GetSystemTimes CPU utilization collector
|   |   |-- memory_collector.h     # GlobalMemoryStatusEx memory collector
|   |   |-- process_collector.h    # EnumProcesses per-process collector
|   |   |-- power_collector.h      # GetSystemPowerStatus battery collector
|   |-- telemetry/
|   |   |-- telemetry_sample.h     # Telemetry data structures and records
|   |   |-- telemetry_runner.h     # Drift-free monotonic sampling engine
|   |-- output/
|   |   |-- csv_writer.h           # RFC 4180 CSV serializer and file manager
|   |-- common/
|       |-- cli_parser.h           # Command-line parser and validator
|       |-- windows_handle.h       # Move-only RAII wrapper for Windows HANDLE
|       |-- error.h                # Win32 error formatting utilities
|
|-- src/
|   |-- main.cpp                   # Application entry point
|   |-- collectors/                # Collector implementations
|   |-- telemetry/                 # Sampling runner implementation
|   |-- output/                    # CSV formatting and persistence logic
|   |-- common/                    # CLI parsing logic
|
|-- scripts/
|   |-- build.ps1                  # Automated PowerShell build script
|   |-- run_monitor.ps1            # Convenience execution script
|
|-- tests/
|   |-- metric_tests.cpp           # Metric math, edge case, and boundary unit tests
|   |-- csv_tests.cpp              # CSV formatting, escaping, and disk write tests
|   |-- cli_tests.cpp              # CLI parser validation and boundary tests
|
|-- docs/
|   |-- architecture.md            # Detailed system design and component breakdown
|   |-- metrics.md                 # Mathematical formulations and kernel semantics
|   |-- data_dictionary.md         # Complete schema specification for CSV files
|   |-- limitations.md             # Technical limitations and boundary disclosures
|
|-- output/
|   |-- .gitkeep                   # Default telemetry output directory
|
|-- .github/
    |-- workflows/
        |-- windows-ci.yml         # GitHub Actions automated Windows CI workflow
```

---

## Building the Project

### Automated PowerShell Build

Run the included build script from PowerShell:

```powershell
# Configure, build Release binary, and run CTest test suites
.\scripts\build.ps1 -Config Release -RunTests
```

To clean previous artifacts and rebuild:

```powershell
.\scripts\build.ps1 -Config Release -Clean -RunTests
```

### Manual CMake Build

```cmd
# 1. Configure for Visual Studio 2022 (x64)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -A x64

# 2. Build Release executable and test suites
cmake --build build --config Release --parallel

# 3. Run CTest unit tests
ctest --test-dir build -C Release --output-on-failure
```

---

## Command-Line Interface

### Parameters & Defaults

| Option | Type | Default | Allowed Range | Description |
| :--- | :--- | :--- | :--- | :--- |
| `--interval-ms` | Integer | `1000` | `250` – `60000` | Sampling interval in milliseconds. Enforces minimum 250 ms to prevent high CPU observer overhead. |
| `--duration-seconds` | Integer | `60` | $> 0$ | Total monitoring duration in seconds. |
| `--output-dir` | Path | `output` | Valid path | Destination folder for CSV files (created automatically if missing). |
| `--top-processes` | Integer | `20` | `0` – `1000` | Number of top CPU-consuming processes to record. Capped at 1000 to prevent unbounded allocations. |
| `--include-processes` | Boolean | `true` | `true`, `false` | Enables or disables per-process monitoring. |
| `--help`, `-h` | Flag | — | — | Displays usage documentation and exits. |
| `--version`, `-v` | Flag | — | — | Displays version metadata and exits. |

### Example Commands

#### Standard 1-Minute Diagnostic Capture
```cmd
device_telemetry.exe --interval-ms 1000 --duration-seconds 60 --output-dir output
```

#### High-Frequency System-Only Capture (No Process Overhead)
```cmd
device_telemetry.exe --interval-ms 250 --duration-seconds 30 --include-processes false
```

#### Extended 10-Minute Capture with Top 50 Processes
```cmd
device_telemetry.exe --interval-ms 2000 --duration-seconds 600 --top-processes 50 --output-dir telemetry_data
```

#### Using the PowerShell Runner Script
```powershell
.\scripts\run_monitor.ps1 -IntervalMs 500 -DurationSeconds 120 -TopProcesses 15
```

---

## Output File Formats

All output files are written with UTF-8 encoding, consistent headers, and per-sample flushing.

### System Metrics (`system_metrics.csv`)

Tracks machine-wide hardware performance and battery dynamics.

```csv
timestamp_utc,elapsed_ms,cpu_utilization_percent,physical_memory_total_bytes,physical_memory_available_bytes,physical_memory_used_percent,virtual_memory_total_bytes,virtual_memory_available_bytes,memory_load_percent,ac_line_status,battery_flag,battery_percent,battery_remaining_seconds,skipped_process_count
2026-11-08T00:00:00.000Z,0,,17179869184,8589934592,50.00,34359738368,20000000000,50,Online,Charging,85,3600,5
2026-11-08T00:00:01.002Z,1002,14.25,17179869184,8580000000,50.06,34359738368,19950000000,50,Online,Charging,85,3600,5
```

*Note: `cpu_utilization_percent` is empty on sample #1 because no delta exists yet.*

### Process Metrics (`process_metrics.csv`)

Tracks individual processes ranked by CPU consumption.

```csv
timestamp_utc,process_id,process_name,process_cpu_percent,working_set_bytes,page_fault_count
2026-11-08T00:00:01.002Z,10452,chrome.exe,4.50,214748364,12450
2026-11-08T00:00:01.002Z,4120,code.exe,1.25,188743680,8210
2026-11-08T00:00:01.002Z,8900,svchost.exe,0.50,45088768,3120
```

*Refer to [`docs/data_dictionary.md`](docs/data_dictionary.md) for full schema definitions.*

---

## Metric Formulations

### System CPU Utilization
Windows reports cumulative ticks via `GetSystemTimes`. Crucially, **kernel time includes idle time**:

$$\Delta \text{Kernel} = \text{KernelTime}_{t} - \text{KernelTime}_{t-1}$$
$$\Delta \text{User} = \text{UserTime}_{t} - \text{UserTime}_{t-1}$$
$$\Delta \text{Idle} = \text{IdleTime}_{t} - \text{IdleTime}_{t-1}$$
$$\text{Total} = \Delta \text{Kernel} + \Delta \text{User}$$
$$\text{Busy} = \text{Total} - \Delta \text{Idle}$$
$$\text{CPU Utilization (\%)} = \left( \frac{\text{Busy}}{\text{Total}} \right) \times 100.0$$

### Process CPU Normalization
To prevent multi-threaded processes from reporting confusing percentages $> 100\%$, CPU usage is normalized across total logical processor capacity:

$$\text{Process CPU (\%)} = \left( \frac{\Delta \text{ProcessTime (ns)}}{\Delta \text{WallClockTime (ns)} \times N_{\text{logical\_cores}}} \right) \times 100.0$$

### PID Reuse Protection
To prevent calculating corrupted CPU deltas when Windows recycles PIDs:
- The monitor stores both the `PID` and 64-bit `CreationTime` (`FILETIME`).
- If a PID is re-encountered with a different creation time, it is recognized as a new process instance. The baseline is reset, and `cpu_percent` is recorded as `nullopt` for that cycle.

### Power & Battery Telemetry
- **AC Line**: `"Online"`, `"Offline"`, or `"Unknown"`.
- **Battery Flags**: Decoded bitmask (`"High"`, `"Low"`, `"Critical"`, `"Charging"`, `"NoBattery"`).
- **Unknown Sentinels**: Values of `255` for charge percentage or `-1` for lifetime are emitted as empty strings, never converted to false zeros.
- **Desktop Gracefulness**: Desktop workstations without batteries report `"NoBattery"` and continue sampling without errors.

*Refer to [`docs/metrics.md`](docs/metrics.md) for in-depth mathematical derivations.*

---

## Sampling Engine & Drift Prevention

Naive sampling engines employ `sleep(interval)` loops, causing progressive cumulative drift:

$$\text{Time Drift} = \sum (\text{CollectionTime} + \text{SleepOvershoot})$$

The Windows Device Telemetry Monitor uses **absolute deadline scheduling**:
1. Captures `T_0 = std::chrono::steady_clock::now()`.
2. Targets exact future monotonic deadlines: $T_k = T_0 + k \times \text{interval}$.
3. Sleeps until $T_k$ in small slices while evaluating the stop token, ensuring zero cumulative drift and instantaneous response to `Ctrl+C`.

---

## Security, Permissions & Privacy

- **No Privilege Escalation**: Runs as standard user. Does not request `SeDebugPrivilege` or install kernel drivers.
- **Zero Privacy Invasions**: Does **not** read window titles, process command-line arguments, user files, browser history, or keystrokes.
- **Zero Network Egress**: The application contains no networking code or telemetry upload endpoints. Data remains strictly on local disk.
- **Access-Denied Handling**: Inaccessible processes (e.g. `SYSTEM`, Protected Process Light, Anti-Cheat) are skipped cleanly without crashing, and increment `skipped_process_count`.

---

## Testing & Quality Assurance

The project includes pure unit tests decoupled from platform APIs, enabling verification on both Windows and cross-platform environments:

- **`metric_tests`**: Validates CPU utilization formulas, zero time deltas, decreasing counters, clock skew clamping, memory byte conversions, multi-core process CPU normalization, PID reuse logic, and power state decoding.
- **`csv_tests`**: Validates RFC 4180 escaping (commas, quotes, newlines), missing value handling (`nullopt`), header emission, disk persistence, and write failure recovery.
- **`cli_tests`**: Validates argument parsing, numeric bounds checking, interval boundaries (`[250, 60000]`), top processes clamping, and error diagnostic output.

Run all tests via CTest:
```cmd
ctest --test-dir build -C Release --output-on-failure
```

---

## Technical Limitations

1. **Inaccessible Processes**: Processes protected by Windows Protected Process Light (PPL), kernel anti-cheat drivers, or other user sessions cannot be opened without elevation and are counted in `skipped_process_count`.
2. **Processor Groups**: Systems with more than 64 logical processors partition cores into processor groups. `GetSystemTimes` reports time for the primary processor group.
3. **Sampling Overhead**: Sampling at intervals faster than 250 ms induces measurable CPU overhead due to repeated process enumeration.
4. **Battery Hardware Validation**: Validating physical battery discharge curves requires physical laptop hardware; cloud CI runners (GitHub Actions) do not possess physical batteries.

*Refer to [`docs/limitations.md`](docs/limitations.md) for detailed architectural limitations.*

---

## Documentation Index

- [Architecture & System Design](docs/architecture.md)
- [Metric Formulations & Kernel Semantics](docs/metrics.md)
- [Data Dictionary & Schema Specification](docs/data_dictionary.md)
- [Technical Limitations & Operating Boundaries](docs/limitations.md)
