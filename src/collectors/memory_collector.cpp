#include "collectors/memory_collector.h"

#include <algorithm>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace collectors {

std::optional<double> CalculateMemoryUtilizationPercent(uint64_t used_bytes, uint64_t total_bytes) {
    if (total_bytes == 0) {
        return std::nullopt;
    }
    const uint64_t clamped_used = std::min(used_bytes, total_bytes);
    const double percent = (static_cast<double>(clamped_used) / static_cast<double>(total_bytes)) * 100.0;
    return std::clamp(percent, 0.0, 100.0);
}

telemetry::SystemMemoryMetrics MemoryCollector::Collect() {
    telemetry::SystemMemoryMetrics metrics;

#if defined(_WIN32)
    MEMORYSTATUSEX mem_status{};
    mem_status.dwLength = sizeof(MEMORYSTATUSEX);

    if (::GlobalMemoryStatusEx(&mem_status)) {
        metrics.physical_memory_total_bytes = mem_status.ullTotalPhys;
        metrics.physical_memory_available_bytes = mem_status.ullAvailPhys;

        if (mem_status.ullTotalPhys >= mem_status.ullAvailPhys) {
            metrics.physical_memory_used_bytes = mem_status.ullTotalPhys - mem_status.ullAvailPhys;
        } else {
            metrics.physical_memory_used_bytes = 0;
        }

        metrics.physical_memory_used_percent = CalculateMemoryUtilizationPercent(
            metrics.physical_memory_used_bytes,
            metrics.physical_memory_total_bytes);

        metrics.virtual_memory_total_bytes = mem_status.ullTotalVirtual;
        metrics.virtual_memory_available_bytes = mem_status.ullAvailVirtual;
        metrics.memory_load_percent = mem_status.dwMemoryLoad;
    }
#endif

    return metrics;
}

} // namespace collectors
