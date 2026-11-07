#include "collectors/system_cpu_collector.h"

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

namespace {

#if defined(_WIN32)
inline uint64_t FileTimeToUint64(const FILETIME& ft) noexcept {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | static_cast<uint64_t>(ft.dwLowDateTime);
}
#endif

} // namespace

std::optional<double> CalculateSystemCpuUtilization(
    uint64_t prev_idle, uint64_t prev_kernel, uint64_t prev_user,
    uint64_t curr_idle, uint64_t curr_kernel, uint64_t curr_user) {

    // Guard against decreasing or invalid counters
    if (curr_idle < prev_idle || curr_kernel < prev_kernel || curr_user < prev_user) {
        return std::nullopt;
    }

    const uint64_t delta_idle = curr_idle - prev_idle;
    const uint64_t delta_kernel = curr_kernel - prev_kernel;
    const uint64_t delta_user = curr_user - prev_user;

    // Windows kernel time includes idle time
    const uint64_t total = delta_kernel + delta_user;
    if (total == 0) {
        return std::nullopt; // Zero time elapsed
    }

    // Guard against edge cases where idle slightly exceeds total due to timer skew
    const uint64_t clamped_idle = std::min(delta_idle, total);
    const uint64_t busy = total - clamped_idle;

    const double percent = (static_cast<double>(busy) / static_cast<double>(total)) * 100.0;
    return std::clamp(percent, 0.0, 100.0);
}

telemetry::SystemCpuMetrics SystemCpuCollector::Collect() {
    telemetry::SystemCpuMetrics metrics;

#if defined(_WIN32)
    FILETIME idle_time{}, kernel_time{}, user_time{};
    if (!::GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        return metrics;
    }

    const uint64_t curr_idle = FileTimeToUint64(idle_time);
    const uint64_t curr_kernel = FileTimeToUint64(kernel_time);
    const uint64_t curr_user = FileTimeToUint64(user_time);

    if (!has_previous_sample_) {
        prev_idle_ = curr_idle;
        prev_kernel_ = curr_kernel;
        prev_user_ = curr_user;
        has_previous_sample_ = true;
        // First sample establishes baseline; no delta available
        metrics.cpu_utilization_percent = std::nullopt;
        return metrics;
    }

    metrics.cpu_utilization_percent = CalculateSystemCpuUtilization(
        prev_idle_, prev_kernel_, prev_user_,
        curr_idle, curr_kernel, curr_user);

    prev_idle_ = curr_idle;
    prev_kernel_ = curr_kernel;
    prev_user_ = curr_user;
#endif

    return metrics;
}

void SystemCpuCollector::Reset() noexcept {
    has_previous_sample_ = false;
    prev_idle_ = 0;
    prev_kernel_ = 0;
    prev_user_ = 0;
}

} // namespace collectors
