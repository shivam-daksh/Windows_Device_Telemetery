# Technical Limitations & Operating Boundaries

This document provides transparent technical disclosures regarding operating system boundaries, hardware dependencies, permission models, and sampling constraints in the **Windows Device Telemetry Monitor**.

---

## 1. Process Access Rights & Non-Elevated Execution

The application is engineered to operate safely under **standard user privileges** without requiring administrative elevation (`UAC Run as Administrator`):

### 1.1. Inaccessible Process Categories
Under standard user privileges on Windows 10 and 11, the Windows security subsystem (`Access Control Lists` / `SeDebugPrivilege`) rejects calls to `OpenProcess` with error `5` (`ERROR_ACCESS_DENIED`) for the following process classes:
1. **Core Operating System Services**: PID 4 (`System`), `csrss.exe`, `lsass.exe`, `services.exe`, `smss.exe`.
2. **Protected Process Light (PPL)**: Windows Defender / Microsoft Defender Antivirus (`MsMpEng.exe`), credential guard services.
3. **Third-Party Anti-Cheat & DRM**: Kernel-level anti-cheat drivers (e.g., Easy Anti-Cheat, BattlEye, Vanguard) that strip process query rights.
4. **Processes Belonging to Other User Sessions**: In multi-user or terminal server environments.

### 1.2. Design Decision & Transparency
- The application **does not attempt privilege escalation** or inject DLLs.
- When a process handle cannot be acquired, the process is skipped safely and the system record increments `skipped_process_count`.
- Consequently, the top-$N$ process list represents the top accessible processes visible to the calling security principal.

---

## 2. Processor Group Architectures (>64 Logical Processors)

On Windows 64-bit architectures, systems with more than 64 logical processor cores (e.g., dual-socket AMD EPYC or Intel Xeon servers) partition logical cores into **Processor Groups** (each containing up to 64 cores):
- The native Windows API `GetSystemTimes` returns CPU time ticks aggregated across all logical processors in the **processor group assigned to the current process**.
- On consumer hardware (Intel Core i3/i5/i7/i9, AMD Ryzen) with 64 or fewer logical processors, Processor Group 0 encompasses all cores, and `GetSystemTimes` accurately measures 100% of the entire physical machine.
- For multi-socket enterprise servers exceeding 64 cores, telemetry reflects the primary processor group.

---

## 3. Sampling Overhead & Observer Effect

Periodic process enumeration incurs non-zero CPU overhead:
- Each sample requires calling `EnumProcesses`, opening up to several hundred process handles via `OpenProcess`, and issuing `GetProcessTimes` and `GetProcessMemoryInfo` for each active PID.
- To prevent the telemetry monitor from noticeably altering the system performance it measures (the "observer effect" or Heisenbug effect), the CLI enforces:
  - **Minimum Sampling Interval**: 250 milliseconds.
  - **Maximum Sampling Interval**: 60,000 milliseconds (1 minute).
  - **Maximum Top Processes**: 1,000 processes.
- For high-frequency sampling (250 ms), the process enumeration loop contributes approximately 0.5% – 2.0% CPU overhead depending on CPU speed and total process count.
- If minimal overhead is required on resource-constrained devices, disable process telemetry via `--include-processes false`.

---

## 4. Diagnostics vs Profiling Tools

Windows Device Telemetry Monitor is a **device-level health and performance telemetry tool**, designed for continuous periodic diagnostic recording:
- It is **NOT** a micro-architectural profiler (e.g., Intel VTune, AMD μProf).
- It is **NOT** an Event Tracing for Windows (ETW) kernel event capture tool (e.g., Windows Performance Recorder / Windows Performance Analyzer).
- It does not collect call stacks, instruction pointers, DPC/ISR latencies, thread scheduling quantum switches, network packets, or file I/O operations.

---

## 5. Power & Battery Hardware Constraints

Battery telemetry relies on the system ACPI / Smart Battery Controller exposed via `GetSystemPowerStatus`:
- **Desktops, Cloud VMs, and CI Runners**: Workstations, cloud virtual machines, and GitHub Actions runners do not possess batteries. On these platforms, `GetSystemPowerStatus` reports `BatteryFlag = 128` (`NoBattery`), and `battery_percent` / `battery_remaining_seconds` are null. The monitor handles this gracefully and continues capturing CPU and memory metrics.
- **Physical Laptop Validation**: Hardware-specific battery behavior (such as remaining run-time estimation algorithms, battery degradation, and AC/battery transition lag) can only be observed and validated on a physical laptop running on battery power.
- **Polling Latency**: Modern laptop battery controllers typically update battery capacity and discharge rates every 10 to 60 seconds. Sampling power telemetry more frequently than 1000 ms will yield unchanged battery percentage numbers between battery controller updates.
