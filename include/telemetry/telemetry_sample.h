#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace telemetry {

/**
 * @brief Telemetry metrics for system-wide CPU utilization.
 */
struct SystemCpuMetrics {
    std::optional<double> cpu_utilization_percent; // nullopt on first sample or counter anomalies
};

/**
 * @brief Telemetry metrics for system physical and virtual memory.
 */
struct SystemMemoryMetrics {
    uint64_t physical_memory_total_bytes{0};
    uint64_t physical_memory_available_bytes{0};
    uint64_t physical_memory_used_bytes{0};
    std::optional<double> physical_memory_used_percent;
    uint64_t virtual_memory_total_bytes{0};
    uint64_t virtual_memory_available_bytes{0};
    uint32_t memory_load_percent{0};
};

/**
 * @brief Telemetry metrics for battery and power source states.
 */
struct PowerMetrics {
    std::string ac_line_status{"Unknown"};
    std::string battery_flag{"Unknown"};
    std::optional<uint32_t> battery_percent;          // nullopt if unknown (255)
    std::optional<uint32_t> battery_remaining_seconds; // nullopt if unknown (-1)
};

/**
 * @brief Telemetry metrics for an individual process.
 */
struct ProcessMetric {
    uint32_t pid{0};
    std::string name;
    std::optional<double> cpu_percent; // nullopt on baseline sample
    uint64_t working_set_bytes{0};
    uint64_t page_fault_count{0};
};

/**
 * @brief Snapshot of active accessible processes and skipped count.
 */
struct ProcessSnapshot {
    std::vector<ProcessMetric> processes;
    uint32_t skipped_process_count{0};
};

/**
 * @brief Complete record written to system_metrics.csv.
 */
struct SystemMetricsRecord {
    std::string timestamp_utc;
    uint64_t elapsed_ms{0};
    std::optional<double> cpu_utilization_percent;
    uint64_t physical_memory_total_bytes{0};
    uint64_t physical_memory_available_bytes{0};
    std::optional<double> physical_memory_used_percent;
    uint64_t virtual_memory_total_bytes{0};
    uint64_t virtual_memory_available_bytes{0};
    uint32_t memory_load_percent{0};
    std::string ac_line_status{"Unknown"};
    std::string battery_flag{"Unknown"};
    std::optional<uint32_t> battery_percent;
    std::optional<uint32_t> battery_remaining_seconds;
    uint32_t skipped_process_count{0};
};

/**
 * @brief Complete record written to process_metrics.csv.
 */
struct ProcessMetricsRecord {
    std::string timestamp_utc;
    uint32_t process_id{0};
    std::string process_name;
    std::optional<double> process_cpu_percent;
    uint64_t working_set_bytes{0};
    uint64_t page_fault_count{0};
};

} // namespace telemetry
