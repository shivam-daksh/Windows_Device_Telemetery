#include "output/csv_writer.h"

#include <iomanip>
#include <sstream>
#include <system_error>

namespace output {

namespace {

std::string FormatDouble(std::optional<double> val) {
    if (!val.has_value()) {
        return "";
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << val.value();
    return ss.str();
}

template <typename T>
std::string FormatOptionalInt(std::optional<T> val) {
    if (!val.has_value()) {
        return "";
    }
    return std::to_string(val.value());
}

} // namespace

std::string EscapeCsvField(std::string_view field) {
    bool needs_quoting = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quoting = true;
            break;
        }
    }

    if (!needs_quoting) {
        return std::string(field);
    }

    std::string result;
    result.reserve(field.size() + 10);
    result.push_back('"');
    for (char c : field) {
        if (c == '"') {
            result.push_back('"');
            result.push_back('"');
        } else {
            result.push_back(c);
        }
    }
    result.push_back('"');
    return result;
}

std::string FormatSystemRecord(const telemetry::SystemMetricsRecord& record) {
    std::ostringstream ss;
    ss << EscapeCsvField(record.timestamp_utc) << ','
       << record.elapsed_ms << ','
       << FormatDouble(record.cpu_utilization_percent) << ','
       << record.physical_memory_total_bytes << ','
       << record.physical_memory_available_bytes << ','
       << FormatDouble(record.physical_memory_used_percent) << ','
       << record.virtual_memory_total_bytes << ','
       << record.virtual_memory_available_bytes << ','
       << record.memory_load_percent << ','
       << EscapeCsvField(record.ac_line_status) << ','
       << EscapeCsvField(record.battery_flag) << ','
       << FormatOptionalInt(record.battery_percent) << ','
       << FormatOptionalInt(record.battery_remaining_seconds) << ','
       << record.skipped_process_count;
    return ss.str();
}

std::string FormatProcessRecord(const telemetry::ProcessMetricsRecord& record) {
    std::ostringstream ss;
    ss << EscapeCsvField(record.timestamp_utc) << ','
       << record.process_id << ','
       << EscapeCsvField(record.process_name) << ','
       << FormatDouble(record.process_cpu_percent) << ','
       << record.working_set_bytes << ','
       << record.page_fault_count;
    return ss.str();
}

CsvWriter::~CsvWriter() {
    Close();
}

CsvWriter::CsvWriter(CsvWriter&& other) noexcept
    : system_csv_path_(std::move(other.system_csv_path_)),
      process_csv_path_(std::move(other.process_csv_path_)),
      system_file_(std::move(other.system_file_)),
      process_file_(std::move(other.process_file_)),
      include_processes_(other.include_processes_) {}

CsvWriter& CsvWriter::operator=(CsvWriter&& other) noexcept {
    if (this != &other) {
        Close();
        system_csv_path_ = std::move(other.system_csv_path_);
        process_csv_path_ = std::move(other.process_csv_path_);
        system_file_ = std::move(other.system_file_);
        process_file_ = std::move(other.process_file_);
        include_processes_ = other.include_processes_;
    }
    return *this;
}

bool CsvWriter::Initialize(const std::filesystem::path& output_directory, bool include_processes) {
    Close();
    include_processes_ = include_processes;

    std::error_code ec;
    std::filesystem::create_directories(output_directory, ec);
    if (ec) {
        return false;
    }

    system_csv_path_ = output_directory / "system_metrics.csv";
    const bool system_exists = std::filesystem::exists(system_csv_path_, ec) &&
                               std::filesystem::file_size(system_csv_path_, ec) > 0;

    system_file_.open(system_csv_path_, std::ios::out | std::ios::app);
    if (!system_file_.is_open()) {
        return false;
    }

    if (!system_exists) {
        system_file_ << kSystemHeader << "\n";
        system_file_.flush();
    }

    if (include_processes_) {
        process_csv_path_ = output_directory / "process_metrics.csv";
        const bool process_exists = std::filesystem::exists(process_csv_path_, ec) &&
                                    std::filesystem::file_size(process_csv_path_, ec) > 0;

        process_file_.open(process_csv_path_, std::ios::out | std::ios::app);
        if (!process_file_.is_open()) {
            system_file_.close();
            return false;
        }

        if (!process_exists) {
            process_file_ << kProcessHeader << "\n";
            process_file_.flush();
        }
    }

    return true;
}

bool CsvWriter::WriteSystemRecord(const telemetry::SystemMetricsRecord& record) {
    if (!system_file_.is_open()) {
        return false;
    }
    system_file_ << FormatSystemRecord(record) << "\n";
    system_file_.flush();
    return !system_file_.fail();
}

bool CsvWriter::WriteProcessRecord(const telemetry::ProcessMetricsRecord& record) {
    if (!include_processes_ || !process_file_.is_open()) {
        return false;
    }
    process_file_ << FormatProcessRecord(record) << "\n";
    process_file_.flush();
    return !process_file_.fail();
}

void CsvWriter::Flush() {
    if (system_file_.is_open()) {
        system_file_.flush();
    }
    if (process_file_.is_open()) {
        process_file_.flush();
    }
}

void CsvWriter::Close() {
    Flush();
    if (system_file_.is_open()) {
        system_file_.close();
    }
    if (process_file_.is_open()) {
        process_file_.close();
    }
}

} // namespace output
