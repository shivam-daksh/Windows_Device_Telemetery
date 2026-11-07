#pragma once

#include "common/cli_parser.h"
#include "collectors/system_cpu_collector.h"
#include "collectors/memory_collector.h"
#include "collectors/process_collector.h"
#include "collectors/power_collector.h"
#include "output/csv_writer.h"

#include <atomic>
#include <chrono>
#include <string>

namespace telemetry {

/**
 * @brief Formats a system_clock time point as an ISO 8601 UTC string (YYYY-MM-DDTHH:MM:SS.mmmZ).
 */
std::string FormatUtcTimestamp(std::chrono::system_clock::time_point tp);

/**
 * @brief Telemetry sampling engine orchestrating periodic metric collection and persistence.
 */
class TelemetryRunner {
public:
    explicit TelemetryRunner(common::TelemetryConfig config);
    ~TelemetryRunner();

    // Move and copy semantics
    TelemetryRunner(const TelemetryRunner&) = delete;
    TelemetryRunner& operator=(const TelemetryRunner&) = delete;

    /**
     * @brief Initializes output files and collectors.
     *
     * @return True if initialized successfully, false otherwise.
     */
    bool Initialize();

    /**
     * @brief Executes the periodic sampling loop until duration elapsed or Ctrl+C received.
     *
     * @return Exit code (0 for success, non-zero for fatal initialization or write failure).
     */
    int Run();

    /**
     * @brief Requests graceful termination of the sampling loop.
     */
    void RequestStop() noexcept;

    [[nodiscard]] bool IsStopRequested() const noexcept {
        return stop_requested_.load();
    }

private:
    void SetupSignalHandlers();
    void RestoreSignalHandlers();
    void SleepUntilWithCancel(std::chrono::steady_clock::time_point target_time);

    common::TelemetryConfig config_;
    std::atomic<bool> stop_requested_{false};

    collectors::SystemCpuCollector cpu_collector_;
    collectors::MemoryCollector memory_collector_;
    collectors::ProcessCollector process_collector_;
    collectors::PowerCollector power_collector_;
    output::CsvWriter csv_writer_;
};

} // namespace telemetry
