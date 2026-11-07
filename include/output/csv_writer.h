#pragma once

#include "telemetry/telemetry_sample.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

namespace output {

/**
 * @brief Escapes a string for RFC 4180 CSV compliance.
 *
 * Encloses the field in double quotes if it contains commas, double quotes,
 * or newline characters. Any internal double quotes are escaped by doubling them.
 *
 * @param field The input string field.
 * @return Escaped string safe for CSV insertion.
 */
std::string EscapeCsvField(std::string_view field);

/**
 * @brief Formats a single SystemMetricsRecord into a CSV line (without trailing newline).
 */
std::string FormatSystemRecord(const telemetry::SystemMetricsRecord& record);

/**
 * @brief Formats a single ProcessMetricsRecord into a CSV line (without trailing newline).
 */
std::string FormatProcessRecord(const telemetry::ProcessMetricsRecord& record);

/**
 * @brief Thread-safe or sequenced CSV file writer for device telemetry streams.
 */
class CsvWriter {
public:
    CsvWriter() = default;
    ~CsvWriter();

    // Prohibit copying
    CsvWriter(const CsvWriter&) = delete;
    CsvWriter& operator=(const CsvWriter&) = delete;

    // Allow moving
    CsvWriter(CsvWriter&&) noexcept;
    CsvWriter& operator=(CsvWriter&&) noexcept;

    /**
     * @brief Initializes output directory and opens CSV files, writing headers if newly created.
     *
     * @param output_directory Path to the directory where CSV files will be stored.
     * @param include_processes Whether process telemetry file should be created.
     * @return True if files were successfully created/opened, false otherwise.
     */
    bool Initialize(const std::filesystem::path& output_directory, bool include_processes);

    /**
     * @brief Writes a system metrics record and flushes to disk.
     */
    bool WriteSystemRecord(const telemetry::SystemMetricsRecord& record);

    /**
     * @brief Writes a process metrics record and flushes to disk.
     */
    bool WriteProcessRecord(const telemetry::ProcessMetricsRecord& record);

    /**
     * @brief Flushes all active file streams.
     */
    void Flush();

    /**
     * @brief Closes all active streams.
     */
    void Close();

    [[nodiscard]] bool IsOpen() const noexcept { return system_file_.is_open(); }
    [[nodiscard]] std::filesystem::path GetSystemFilePath() const { return system_csv_path_; }
    [[nodiscard]] std::filesystem::path GetProcessFilePath() const { return process_csv_path_; }

    static constexpr std::string_view kSystemHeader =
        "timestamp_utc,elapsed_ms,cpu_utilization_percent,physical_memory_total_bytes,"
        "physical_memory_available_bytes,physical_memory_used_percent,virtual_memory_total_bytes,"
        "virtual_memory_available_bytes,memory_load_percent,ac_line_status,battery_flag,"
        "battery_percent,battery_remaining_seconds,skipped_process_count";

    static constexpr std::string_view kProcessHeader =
        "timestamp_utc,process_id,process_name,process_cpu_percent,working_set_bytes,page_fault_count";

private:
    std::filesystem::path system_csv_path_;
    std::filesystem::path process_csv_path_;
    std::ofstream system_file_;
    std::ofstream process_file_;
    bool include_processes_{false};
};

} // namespace output
