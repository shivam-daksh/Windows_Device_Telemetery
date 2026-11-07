#pragma once

#include "telemetry/telemetry_sample.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace collectors {

struct ProcessPreviousSample {
    uint64_t creation_time_raw{0};
    uint64_t total_cpu_time_raw{0}; // 100-nanosecond units (kernel + user)
    std::chrono::steady_clock::time_point timestamp{};
};

/**
 * @brief Computes normalized process CPU utilization share of total system capacity.
 *
 * Formula:
 * delta_cpu_ns = (curr_cpu_time_100ns - prev_cpu_time_100ns) * 100
 * cpu_percent = (delta_cpu_ns / (elapsed_wall_nanoseconds * logical_processors)) * 100.0
 *
 * @param prev_cpu_time_100ns Previous cumulative process CPU time in 100-ns units.
 * @param curr_cpu_time_100ns Current cumulative process CPU time in 100-ns units.
 * @param elapsed_wall_nanoseconds Elapsed wall-clock time between measurements in nanoseconds.
 * @param logical_processors System logical processor count.
 * @return Normalized CPU percentage in [0.0, 100.0], or nullopt if invalid.
 */
std::optional<double> CalculateProcessCpuPercent(
    uint64_t prev_cpu_time_100ns,
    uint64_t curr_cpu_time_100ns,
    uint64_t elapsed_wall_nanoseconds,
    uint32_t logical_processors);

/**
 * @brief Evaluates whether a process sample can compute a valid CPU delta or must establish a new baseline.
 *
 * Checks creation timestamp to ensure PID reuse by a newer process is not confused with an older process.
 */
bool IsSameProcessInstance(bool has_previous, uint64_t prev_creation_time, uint64_t curr_creation_time);

/**
 * @brief Collector for per-process performance metrics (CPU, working set, page faults).
 */
class ProcessCollector {
public:
    explicit ProcessCollector(uint32_t logical_processors = 0);

    /**
     * @brief Enumerates accessible processes and returns the top N by CPU usage.
     *
     * @param top_n Maximum number of processes to return.
     * @return ProcessSnapshot containing top processes and skipped process count.
     */
    telemetry::ProcessSnapshot Collect(size_t top_n);

    /**
     * @brief Resets all cached previous sample baselines.
     */
    void Reset();

    [[nodiscard]] uint32_t GetLogicalProcessorCount() const noexcept {
        return logical_processors_;
    }

private:
    uint32_t logical_processors_{1};
    std::unordered_map<uint32_t, ProcessPreviousSample> previous_samples_;
};

} // namespace collectors
