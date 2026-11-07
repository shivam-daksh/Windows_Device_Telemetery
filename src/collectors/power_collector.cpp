#include "collectors/power_collector.h"

#include <vector>

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

std::string DecodeAcLineStatus(uint8_t status) {
    switch (status) {
        case 0:
            return "Offline";
        case 1:
            return "Online";
        case 255:
        default:
            return "Unknown";
    }
}

std::string DecodeBatteryFlag(uint8_t flag) {
    if (flag == 255) {
        return "Unknown";
    }
    if ((flag & 128) != 0) {
        return "NoBattery";
    }

    std::vector<std::string> parts;
    if ((flag & 8) != 0) {
        parts.emplace_back("Charging");
    }
    if ((flag & 4) != 0) {
        parts.emplace_back("Critical");
    } else if ((flag & 2) != 0) {
        parts.emplace_back("Low");
    } else if ((flag & 1) != 0) {
        parts.emplace_back("High");
    }

    if (parts.empty()) {
        if (flag == 0) {
            return "Normal";
        }
        return "Unknown";
    }

    std::string result = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        result += "|" + parts[i];
    }
    return result;
}

std::optional<uint32_t> DecodeBatteryPercent(uint8_t life_percent) {
    if (life_percent == 255 || life_percent > 100) {
        return std::nullopt;
    }
    return static_cast<uint32_t>(life_percent);
}

std::optional<uint32_t> DecodeBatteryRemainingSeconds(uint32_t life_time_seconds) {
    if (life_time_seconds == static_cast<uint32_t>(-1) || life_time_seconds == 0xFFFFFFFF) {
        return std::nullopt;
    }
    return life_time_seconds;
}

telemetry::PowerMetrics PowerCollector::Collect() {
    telemetry::PowerMetrics metrics;

#if defined(_WIN32)
    SYSTEM_POWER_STATUS sps{};
    if (::GetSystemPowerStatus(&sps)) {
        metrics.ac_line_status = DecodeAcLineStatus(sps.ACLineStatus);
        metrics.battery_flag = DecodeBatteryFlag(sps.BatteryFlag);
        metrics.battery_percent = DecodeBatteryPercent(sps.BatteryLifePercent);
        metrics.battery_remaining_seconds = DecodeBatteryRemainingSeconds(sps.BatteryLifeTime);
    }
#endif

    return metrics;
}

} // namespace collectors
