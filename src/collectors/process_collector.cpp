#include "collectors/process_collector.h"
#include "common/windows_handle.h"

#include <algorithm>
#include <thread>
#include <unordered_set>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace collectors {

namespace {

#if defined(_WIN32)
inline uint64_t FileTimeToUint64(const FILETIME& ft) noexcept {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | static_cast<uint64_t>(ft.dwLowDateTime);
}

std::string Utf16ToUtf8(std::wstring_view wstr) {
    if (wstr.empty()) {
        return "";
    }
    const int size_needed = ::WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
        nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        return "";
    }
    std::string result(static_cast<size_t>(size_needed), '\0');
    ::WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
        result.data(), size_needed, nullptr, nullptr);
    return result;
}

std::string ExtractBaseName(std::wstring_view full_path) {
    if (full_path.empty()) {
        return "[Unknown]";
    }
    const auto last_slash = full_path.find_last_of(L"\\/");
    if (last_slash != std::wstring_view::npos) {
        full_path = full_path.substr(last_slash + 1);
    }
    return Utf16ToUtf8(full_path);
}
#endif

} // namespace

std::optional<double> CalculateProcessCpuPercent(
    uint64_t prev_cpu_time_100ns,
    uint64_t curr_cpu_time_100ns,
    uint64_t elapsed_wall_nanoseconds,
    uint32_t logical_processors) {

    if (curr_cpu_time_100ns < prev_cpu_time_100ns) {
        return std::nullopt;
    }
    if (elapsed_wall_nanoseconds == 0 || logical_processors == 0) {
        return std::nullopt;
    }

    const uint64_t delta_cpu_100ns = curr_cpu_time_100ns - prev_cpu_time_100ns;
    // 1 FILETIME unit = 100 nanoseconds
    const double delta_cpu_ns = static_cast<double>(delta_cpu_100ns) * 100.0;
    const double total_capacity_ns = static_cast<double>(elapsed_wall_nanoseconds) *
                                     static_cast<double>(logical_processors);

    if (total_capacity_ns <= 0.0) {
        return std::nullopt;
    }

    const double percent = (delta_cpu_ns / total_capacity_ns) * 100.0;
    return std::clamp(percent, 0.0, 100.0);
}

bool IsSameProcessInstance(bool has_previous, uint64_t prev_creation_time, uint64_t curr_creation_time) {
    if (!has_previous) {
        return false;
    }
    return prev_creation_time == curr_creation_time;
}

ProcessCollector::ProcessCollector(uint32_t logical_processors) {
    if (logical_processors > 0) {
        logical_processors_ = logical_processors;
    } else {
#if defined(_WIN32)
        SYSTEM_INFO sys_info{};
        ::GetSystemInfo(&sys_info);
        logical_processors_ = std::max(1u, static_cast<uint32_t>(sys_info.dwNumberOfProcessors));
#else
        const unsigned int hw = std::thread::hardware_concurrency();
        logical_processors_ = (hw > 0) ? hw : 1;
#endif
    }
}

telemetry::ProcessSnapshot ProcessCollector::Collect([[maybe_unused]] size_t top_n) {
    telemetry::ProcessSnapshot snapshot;

#if defined(_WIN32)
    std::vector<DWORD> pids(1024);
    DWORD bytes_returned = 0;

    while (true) {
        const DWORD buffer_size = static_cast<DWORD>(pids.size() * sizeof(DWORD));
        if (!::EnumProcesses(pids.data(), buffer_size, &bytes_returned)) {
            return snapshot;
        }
        if (bytes_returned < buffer_size) {
            pids.resize(bytes_returned / sizeof(DWORD));
            break;
        }
        // Buffer was completely filled, enlarge and query again
        pids.resize(pids.size() * 2);
    }

    std::unordered_set<uint32_t> seen_pids;
    seen_pids.reserve(pids.size());

    const auto now = std::chrono::steady_clock::now();
    std::vector<telemetry::ProcessMetric> collected_processes;
    collected_processes.reserve(pids.size());

    for (const DWORD pid : pids) {
        if (pid == 0) {
            // PID 0 is System Idle Process; skip per standard practice
            continue;
        }

        seen_pids.insert(pid);

        // First try QUERY_LIMITED_INFORMATION | VM_READ
        HANDLE raw_handle = ::OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
            FALSE, pid);

        if (raw_handle == nullptr) {
            // Fall back to QUERY_LIMITED_INFORMATION only
            raw_handle = ::OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION,
                FALSE, pid);
        }

        if (raw_handle == nullptr) {
            // Access denied or process terminated; increment skipped count
            snapshot.skipped_process_count++;
            continue;
        }

        common::WindowsHandle process_handle(raw_handle);
        telemetry::ProcessMetric metric;
        metric.pid = static_cast<uint32_t>(pid);

        // 1. Process Executable Name
        std::wstring image_path(MAX_PATH, L'\0');
        DWORD path_size = static_cast<DWORD>(image_path.size());
        if (::QueryFullProcessImageNameW(process_handle.Get(), 0, image_path.data(), &path_size)) {
            image_path.resize(path_size);
            metric.name = ExtractBaseName(image_path);
        } else {
            metric.name = "[Unknown]";
        }

        // 2. Working Set & Page Faults
        PROCESS_MEMORY_COUNTERS pmc{};
        if (::GetProcessMemoryInfo(process_handle.Get(), &pmc, sizeof(pmc))) {
            metric.working_set_bytes = pmc.WorkingSetSize;
            metric.page_fault_count = pmc.PageFaultCount;
        }

        // 3. Process CPU Times & PID Reuse Protection
        FILETIME creation_time{}, exit_time{}, kernel_time{}, user_time{};
        if (::GetProcessTimes(process_handle.Get(), &creation_time, &exit_time, &kernel_time, &user_time)) {
            const uint64_t curr_creation = FileTimeToUint64(creation_time);
            const uint64_t curr_kernel = FileTimeToUint64(kernel_time);
            const uint64_t curr_user = FileTimeToUint64(user_time);
            const uint64_t curr_total_cpu = curr_kernel + curr_user;

            auto it = previous_samples_.find(pid);
            if (it != previous_samples_.end() &&
                IsSameProcessInstance(true, it->second.creation_time_raw, curr_creation)) {

                const uint64_t elapsed_ns = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(now - it->second.timestamp).count());

                metric.cpu_percent = CalculateProcessCpuPercent(
                    it->second.total_cpu_time_raw,
                    curr_total_cpu,
                    elapsed_ns,
                    logical_processors_);

                it->second.total_cpu_time_raw = curr_total_cpu;
                it->second.timestamp = now;
            } else {
                // First observation or PID was reused: record baseline; CPU % is nullopt
                previous_samples_[pid] = ProcessPreviousSample{
                    .creation_time_raw = curr_creation,
                    .total_cpu_time_raw = curr_total_cpu,
                    .timestamp = now
                };
                metric.cpu_percent = std::nullopt;
            }
        }

        collected_processes.emplace_back(std::move(metric));
    }

    // Clean up terminated processes from previous_samples_ map to prevent memory growth
    for (auto it = previous_samples_.begin(); it != previous_samples_.end();) {
        if (seen_pids.find(it->first) == seen_pids.end()) {
            it = previous_samples_.erase(it);
        } else {
            ++it;
        }
    }

    // Sort: highest CPU usage first.
    // If CPU % is not available, sort by working set bytes descending.
    std::sort(collected_processes.begin(), collected_processes.end(),
              [](const telemetry::ProcessMetric& a, const telemetry::ProcessMetric& b) {
                  const bool a_has_cpu = a.cpu_percent.has_value();
                  const bool b_has_cpu = b.cpu_percent.has_value();

                  if (a_has_cpu && b_has_cpu) {
                      if (a.cpu_percent.value() != b.cpu_percent.value()) {
                          return a.cpu_percent.value() > b.cpu_percent.value();
                      }
                      return a.working_set_bytes > b.working_set_bytes;
                  }
                  if (a_has_cpu != b_has_cpu) {
                      return a_has_cpu; // Process with valid CPU delta ranks higher
                  }
                  if (a.working_set_bytes != b.working_set_bytes) {
                      return a.working_set_bytes > b.working_set_bytes;
                  }
                  return a.pid < b.pid;
              });

    if (collected_processes.size() > top_n) {
        collected_processes.resize(top_n);
    }

    snapshot.processes = std::move(collected_processes);
#endif

    return snapshot;
}

void ProcessCollector::Reset() {
    previous_samples_.clear();
}

} // namespace collectors
