#pragma once

#include "telemetry/telemetry_sample.h"

#include <cstdint>
#include <optional>

namespace collectors {

/**
 * @brief Computes system CPU utilization percentage from consecutive idle, kernel, and user times.
 *
 * Windows GetSystemTimes returns cumulative times where kernel time includes idle time.
 * Total time = delta_kernel + delta_user
 * Busy time = total - delta_idle
 * CPU % = (busy / total) * 100.0
 *
 * @param prev_idle Cumulative idle time in previous sample (100-ns units).
 * @param prev_kernel Cumulative kernel time in previous sample (100-ns units).
 * @param prev_user Cumulative user time in previous sample (100-ns units).
 * @param curr_idle Cumulative idle time in current sample (100-ns units).
 * @param curr_kernel Cumulative kernel time in current sample (100-ns units).
 * @param curr_user Cumulative user time in current sample (100-ns units).
 * @return CPU utilization percentage in [0.0, 100.0], or nullopt on invalid/zero deltas.
 */
std::optional<double> CalculateSystemCpuUtilization(
    uint64_t prev_idle, uint64_t prev_kernel, uint64_t prev_user,
    uint64_t curr_idle, uint64_t curr_kernel, uint64_t curr_user);

/**
 * @brief Collector for system CPU utilization using native Windows APIs.
 */
class SystemCpuCollector {
public:
    SystemCpuCollector() = default;

    /**
     * @brief Collects the current CPU sample and calculates utilization since the prior sample.
     *
     * @return SystemCpuMetrics containing cpu_utilization_percent (nullopt on first sample or error).
     */
    telemetry::SystemCpuMetrics Collect();

    /**
     * @brief Resets the previous sample baseline.
     */
    void Reset() noexcept;

private:
    bool has_previous_sample_{false};
    uint64_t prev_idle_{0};
    uint64_t prev_kernel_{0};
    uint64_t prev_user_{0};
};

} // namespace collectors
