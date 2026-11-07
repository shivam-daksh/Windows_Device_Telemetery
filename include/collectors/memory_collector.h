#pragma once

#include "telemetry/telemetry_sample.h"

#include <cstdint>
#include <optional>

namespace collectors {

/**
 * @brief Calculates memory utilization percentage from used and total byte counts.
 *
 * @param used_bytes Number of bytes currently consumed.
 * @param total_bytes Total capacity in bytes.
 * @return Utilization percentage in [0.0, 100.0], or nullopt if total_bytes == 0.
 */
std::optional<double> CalculateMemoryUtilizationPercent(uint64_t used_bytes, uint64_t total_bytes);

/**
 * @brief Collector for physical and virtual memory telemetry using GlobalMemoryStatusEx.
 */
class MemoryCollector {
public:
    MemoryCollector() = default;

    /**
     * @brief Collects current system memory statistics.
     *
     * @return SystemMemoryMetrics populated from GlobalMemoryStatusEx.
     */
    telemetry::SystemMemoryMetrics Collect();
};

} // namespace collectors
