# Telemetry Metrics & Mathematical Formulations

This document provides the formal mathematical definitions, Windows kernel timing semantics, and conversion algorithms implemented in the **Windows Device Telemetry Monitor**.

---

## 1. System CPU Utilization

### 1.1. Windows NT Scheduling Semantics
The Windows kernel tracks processor execution time via `GetSystemTimes`:
- **`lpIdleTime`**: Cumulative time the processor spent running the idle thread (`IdleTask`).
- **`lpKernelTime`**: Cumulative time the processor spent executing in ring 0 (kernel mode), **which includes the idle thread time**.
- **`lpUserTime`**: Cumulative time the processor spent executing in ring 3 (user mode applications).

All times are represented as 64-bit `FILETIME` structures measuring cumulative time in **100-nanosecond intervals** (ticks).

### 1.2. Utilization Formula
Because kernel time subsumes idle time, total CPU capacity is computed as:

$$\Delta \text{Kernel} = \text{KernelTime}_{t} - \text{KernelTime}_{t-1}$$

$$\Delta \text{User} = \text{UserTime}_{t} - \text{UserTime}_{t-1}$$

$$\Delta \text{Idle} = \text{IdleTime}_{t} - \text{IdleTime}_{t-1}$$

$$\text{Total} = \Delta \text{Kernel} + \Delta \text{User}$$

$$\text{Busy} = \text{Total} - \Delta \text{Idle}$$

$$\text{CPU Utilization (\%)} = \left( \frac{\text{Busy}}{\text{Total}} \right) \times 100.0$$

### 1.3. Edge Case Handling
1. **First-Sample Baseline**: During the very first collection cycle ($t=0$), no prior counter state exists. The collector records the baseline counter values and sets `cpu_utilization_percent` to `std::nullopt` (empty cell in CSV), preventing artificial 0% or 100% spikes.
2. **Zero Elapsed Time**: If $\text{Total} = 0$ (e.g. back-to-back calls without timer advancement), the formula avoids division by zero and returns `std::nullopt`.
3. **Decreasing / Rolled-Over Counters**: If $\Delta \text{Kernel} < 0$, $\Delta \text{User} < 0$, or $\Delta \text{Idle} < 0$, the measurement is flagged as anomalous and returns `std::nullopt`.
4. **Timer Skew & Clamping**: On multi-socket or multi-core machines, non-atomic sampling of per-core counters can rarely cause $\Delta \text{Idle} > \text{Total}$ by a fractional tick. The calculation clamps $\Delta \text{Idle} \le \text{Total}$ and clamps the final percentage to $[0.0, 100.0]$.

---

## 2. System Memory Telemetry

Memory metrics are sampled via `GlobalMemoryStatusEx` using the `MEMORYSTATUSEX` structure.

### 2.1. Physical Memory
- **`physical_memory_total_bytes`**: Physical RAM installed and accessible to the OS (`ullTotalPhys`).
- **`physical_memory_available_bytes`**: Physical RAM immediately available for allocation without paging (`ullAvailPhys`).
- **`physical_memory_used_bytes`**: Computed strictly as:
  $$\text{UsedBytes} = \text{ullTotalPhys} - \text{ullAvailPhys}$$
- **`physical_memory_used_percent`**: Computed utilization:
  $$\text{Memory Utilization (\%)} = \left( \frac{\text{UsedBytes}}{\text{TotalBytes}} \right) \times 100.0$$

### 2.2. Virtual Memory
- **`virtual_memory_total_bytes`**: Total user-mode virtual address space available to the calling process or system commit limit (`ullTotalVirtual`).
- **`virtual_memory_available_bytes`**: Unreserved and uncommitted virtual address space (`ullAvailVirtual`).
- **`memory_load_percent`**: Approximate percentage of physical memory in use reported directly by the Windows memory manager (`dwMemoryLoad`, bounded between 0 and 100).

---

## 3. Per-Process Telemetry

### 3.1. Process CPU Normalization
Processes execute across multiple logical cores. To make per-process CPU usage intuitive and directly comparable to total system capacity, the reported CPU percentage represents the process's **share of total system CPU capacity** (0% to 100%):

$$\Delta \text{ProcessTicks} = (\text{KernelTime}_{t} - \text{KernelTime}_{t-1}) + (\text{UserTime}_{t} - \text{UserTime}_{t-1})$$

$$\Delta \text{ProcessTime (ns)} = \Delta \text{ProcessTicks} \times 100$$

$$\text{Capacity (ns)} = \Delta \text{WallClockTime (ns)} \times N_{\text{logical\_processors}}$$

$$\text{Process CPU (\%)} = \left( \frac{\Delta \text{ProcessTime (ns)}}{\text{Capacity (ns)}} \right) \times 100.0$$

#### Example
On an 8-core machine ($N=8$):
- A single thread pegging one core at 100% capacity consumes 1 core's worth of CPU time:
  $$\text{Process CPU (\%)} = \frac{1.0}{8 \times 1.0} \times 100 = 12.5\%$$
- A multi-threaded process pegging all 8 cores consumes:
  $$\text{Process CPU (\%)} = \frac{8.0}{8 \times 1.0} \times 100 = 100.0\%$$

### 3.2. PID Reuse Protection
Windows recycles process IDs aggressively. If process A terminates and process B starts with the same PID between sampling cycles, a naive PID-indexed map would calculate a corrupted delta between process A's and process B's CPU counters.

**Solution**: The monitor records the 64-bit process creation timestamp (`FILETIME lpCreationTime` from `GetProcessTimes`). When sampling:
- If `PID` exists and `CreationTime == PreviousCreationTime`: identical process instance $\rightarrow$ compute valid delta.
- If `PID` exists but `CreationTime != PreviousCreationTime`: PID was reused $\rightarrow$ reset counter baseline, record `cpu_percent = std::nullopt`.
- If `PID` is newly seen $\rightarrow$ establish baseline, record `cpu_percent = std::nullopt`.

### 3.3. Memory & Paging Metrics
- **`working_set_bytes`**: Current physical memory assigned to the process (`WorkingSetSize` from `PROCESS_MEMORY_COUNTERS`).
- **`page_fault_count`**: Cumulative number of page faults (soft and hard) generated by the process since startup (`PageFaultCount`).

---

## 4. Power & Battery Telemetry

Obtained via native `GetSystemPowerStatus` (`SYSTEM_POWER_STATUS`).

| Metric | Source Field | Type / Values | Handling of Unknown / Unsupported |
| :--- | :--- | :--- | :--- |
| **AC Line Status** | `ACLineStatus` | `0` = Offline (Battery)<br>`1` = Online (AC Line)<br>`255` = Unknown | Decoded to `"Offline"`, `"Online"`, or `"Unknown"`. |
| **Battery Flag** | `BatteryFlag` | Bitmask:<br>`1` = High (>66%)<br>`2` = Low (<33%)<br>`4` = Critical (<5%)<br>`8` = Charging<br>`128` = No system battery<br>`255` = Unknown | Decoded to pipe-separated descriptive string (e.g., `"Charging\|High"`, `"NoBattery"`, `"Unknown"`). |
| **Battery Percentage** | `BatteryLifePercent` | `0..100` (%)<br>`255` = Unknown | Valid integer in $[0, 100]$. **If 255, emitted as empty string (nullopt)**. Never converted to 0. |
| **Battery Life Remaining** | `BatteryLifeTime` | Seconds remaining<br>`-1` (`0xFFFFFFFF`) = Unknown / on AC | Valid integer in seconds. **If -1, emitted as empty string (nullopt)**. Never converted to 0. |
