#include "telemetry/telemetry_runner.h"

#include <csignal>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace telemetry {

namespace {

static TelemetryRunner* g_active_runner = nullptr;

#if defined(_WIN32)
BOOL WINAPI WindowsConsoleCtrlHandler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT || ctrl_type == CTRL_CLOSE_EVENT) {
        if (g_active_runner != nullptr) {
            std::cout << "\n[!] Termination requested by user (Ctrl+C). Flushing and shutting down safely...\n";
            g_active_runner->RequestStop();
            return TRUE;
        }
    }
    return FALSE;
}
#else
void PosixSignalHandler(int signal_num) {
    if (signal_num == SIGINT || signal_num == SIGTERM) {
        if (g_active_runner != nullptr) {
            std::cout << "\n[!] Termination requested by signal. Flushing and shutting down safely...\n";
            g_active_runner->RequestStop();
        }
    }
}
#endif

} // namespace

std::string FormatUtcTimestamp(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    const auto millis = duration_cast<milliseconds>(tp.time_since_epoch());
    const auto ms = millis % 1000;
    const std::time_t tt = system_clock::to_time_t(tp);
    std::tm tm_buf{};

#if defined(_WIN32)
    gmtime_s(&tm_buf, &tt);
#else
    gmtime_r(&tt, &tm_buf);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count()
       << 'Z';
    return ss.str();
}

TelemetryRunner::TelemetryRunner(common::TelemetryConfig config)
    : config_(std::move(config)) {}

TelemetryRunner::~TelemetryRunner() {
    RestoreSignalHandlers();
}

void TelemetryRunner::SetupSignalHandlers() {
    g_active_runner = this;
#if defined(_WIN32)
    ::SetConsoleCtrlHandler(WindowsConsoleCtrlHandler, TRUE);
#else
    std::signal(SIGINT, PosixSignalHandler);
    std::signal(SIGTERM, PosixSignalHandler);
#endif
}

void TelemetryRunner::RestoreSignalHandlers() {
#if defined(_WIN32)
    ::SetConsoleCtrlHandler(WindowsConsoleCtrlHandler, FALSE);
#else
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
#endif
    if (g_active_runner == this) {
        g_active_runner = nullptr;
    }
}

void TelemetryRunner::RequestStop() noexcept {
    stop_requested_.store(true);
}

void TelemetryRunner::SleepUntilWithCancel(std::chrono::steady_clock::time_point target_time) {
    while (!stop_requested_.load()) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= target_time) {
            break;
        }
        const auto remaining = target_time - now;
        const auto slice = std::min(
            remaining,
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(50)));
        std::this_thread::sleep_for(slice);
    }
}

bool TelemetryRunner::Initialize() {
    std::cout << "[+] Initializing telemetry output files in '" << config_.output_dir << "'...\n";
    if (!csv_writer_.Initialize(config_.output_dir, config_.include_processes)) {
        std::cerr << "[-] Error: Failed to initialize CSV writers at output directory: "
                  << config_.output_dir << "\n";
        return false;
    }

    std::cout << "[+] Output initialized:\n"
              << "    System telemetry:  " << csv_writer_.GetSystemFilePath() << "\n";
    if (config_.include_processes) {
        std::cout << "    Process telemetry: " << csv_writer_.GetProcessFilePath() << "\n";
    }
    return true;
}

int TelemetryRunner::Run() {
    SetupSignalHandlers();

    std::cout << "\n======================================================\n"
              << "  Windows Device Telemetry Monitor Running\n"
              << "  Sampling Interval: " << config_.interval_ms << " ms\n"
              << "  Planned Duration:  " << config_.duration_seconds << " seconds\n"
              << "  Top Processes:     " << (config_.include_processes ? std::to_string(config_.top_processes) : "Disabled") << "\n"
              << "  Press Ctrl+C to stop cleanly at any time.\n"
              << "======================================================\n\n";

    const auto start_monotonic_time = std::chrono::steady_clock::now();
    const auto target_duration = std::chrono::seconds(config_.duration_seconds);
    const auto interval = std::chrono::milliseconds(config_.interval_ms);

    uint64_t sample_index = 0;

    while (!stop_requested_.load()) {
        const auto sample_wall_time = std::chrono::system_clock::now();
        const auto sample_monotonic_time = std::chrono::steady_clock::now();
        const auto elapsed_duration = sample_monotonic_time - start_monotonic_time;
        const uint64_t elapsed_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_duration).count());

        const std::string timestamp_utc = FormatUtcTimestamp(sample_wall_time);

        // 1. Collect System CPU
        const auto cpu_metrics = cpu_collector_.Collect();

        // 2. Collect Memory
        const auto mem_metrics = memory_collector_.Collect();

        // 3. Collect Power
        const auto power_metrics = power_collector_.Collect();

        // 4. Collect Processes
        telemetry::ProcessSnapshot proc_snapshot;
        if (config_.include_processes) {
            proc_snapshot = process_collector_.Collect(config_.top_processes);
        }

        // 5. Build and Write System Record
        telemetry::SystemMetricsRecord sys_record;
        sys_record.timestamp_utc = timestamp_utc;
        sys_record.elapsed_ms = elapsed_ms;
        sys_record.cpu_utilization_percent = cpu_metrics.cpu_utilization_percent;
        sys_record.physical_memory_total_bytes = mem_metrics.physical_memory_total_bytes;
        sys_record.physical_memory_available_bytes = mem_metrics.physical_memory_available_bytes;
        sys_record.physical_memory_used_percent = mem_metrics.physical_memory_used_percent;
        sys_record.virtual_memory_total_bytes = mem_metrics.virtual_memory_total_bytes;
        sys_record.virtual_memory_available_bytes = mem_metrics.virtual_memory_available_bytes;
        sys_record.memory_load_percent = mem_metrics.memory_load_percent;
        sys_record.ac_line_status = power_metrics.ac_line_status;
        sys_record.battery_flag = power_metrics.battery_flag;
        sys_record.battery_percent = power_metrics.battery_percent;
        sys_record.battery_remaining_seconds = power_metrics.battery_remaining_seconds;
        sys_record.skipped_process_count = proc_snapshot.skipped_process_count;

        if (!csv_writer_.WriteSystemRecord(sys_record)) {
            std::cerr << "[-] Error: Failed to write system metrics record to CSV.\n";
        }

        // 6. Write Process Records
        if (config_.include_processes) {
            for (const auto& proc : proc_snapshot.processes) {
                telemetry::ProcessMetricsRecord proc_record;
                proc_record.timestamp_utc = timestamp_utc;
                proc_record.process_id = proc.pid;
                proc_record.process_name = proc.name;
                proc_record.process_cpu_percent = proc.cpu_percent;
                proc_record.working_set_bytes = proc.working_set_bytes;
                proc_record.page_fault_count = proc.page_fault_count;

                if (!csv_writer_.WriteProcessRecord(proc_record)) {
                    std::cerr << "[-] Error: Failed to write process record to CSV for PID: "
                              << proc.pid << "\n";
                }
            }
        }

        // 7. Output Console Progress Line
        std::cout << "[" << timestamp_utc << "] Sample #" << sample_index + 1
                  << " | Elapsed: " << elapsed_ms << "ms"
                  << " | CPU: ";
        if (sys_record.cpu_utilization_percent.has_value()) {
            std::cout << std::fixed << std::setprecision(1) << sys_record.cpu_utilization_percent.value() << "%";
        } else {
            std::cout << "Init";
        }

        std::cout << " | Mem Used: ";
        if (sys_record.physical_memory_used_percent.has_value()) {
            std::cout << std::fixed << std::setprecision(1) << sys_record.physical_memory_used_percent.value() << "%";
        } else {
            std::cout << "N/A";
        }

        std::cout << " | Power: " << sys_record.ac_line_status;
        if (sys_record.battery_percent.has_value()) {
            std::cout << " (" << sys_record.battery_percent.value() << "%)";
        }

        if (config_.include_processes) {
            std::cout << " | Procs Monitored: " << proc_snapshot.processes.size()
                      << " (Skipped: " << proc_snapshot.skipped_process_count << ")";
        }
        std::cout << std::endl;

        // Check if duration has been reached
        if (elapsed_duration >= target_duration) {
            std::cout << "\n[+] Target duration reached (" << config_.duration_seconds << "s). Stopping.\n";
            break;
        }

        // Schedule next sample without drift
        ++sample_index;
        const auto next_sample_target = start_monotonic_time + (sample_index * interval);
        SleepUntilWithCancel(next_sample_target);
    }

    csv_writer_.Close();
    RestoreSignalHandlers();

    std::cout << "\n[+] Telemetry collection completed successfully.\n"
              << "    Total samples captured: " << (sample_index + 1) << "\n"
              << "    Telemetry files safely written and closed.\n";

    return 0;
}

} // namespace telemetry
