#pragma once

#include "telemetry/telemetry_sample.h"

#include <cstdint>
#include <optional>
#include <string>

namespace collectors {

/**
 * @brief Decodes the Windows AC line status value.
 *
 * 0 = Offline (battery)
 * 1 = Online (AC)
 * 255 = Unknown
 */
std::string DecodeAcLineStatus(uint8_t status);

/**
 * @brief Decodes the Windows SYSTEM_POWER_STATUS BatteryFlag bitmask.
 *
 * Bit 0 (1): High (> 66%)
 * Bit 1 (2): Low (< 33%)
 * Bit 2 (4): Critical (< 5%)
 * Bit 3 (8): Charging
 * Bit 7 (128): No system battery
 * 255: Unknown status
 */
std::string DecodeBatteryFlag(uint8_t flag);

/**
 * @brief Decodes the battery life percent value.
 *
 * 0..100 = percentage
 * 255 = Unknown status (returns nullopt)
 */
std::optional<uint32_t> DecodeBatteryPercent(uint8_t life_percent);

/**
 * @brief Decodes the remaining battery lifetime in seconds.
 *
 * -1 (0xFFFFFFFF) = Unknown / on AC (returns nullopt)
 */
std::optional<uint32_t> DecodeBatteryRemainingSeconds(uint32_t life_time_seconds);

/**
 * @brief Collector for power, AC line, and battery state using GetSystemPowerStatus.
 */
class PowerCollector {
public:
    PowerCollector() = default;

    /**
     * @brief Collects current system power telemetry.
     *
     * @return PowerMetrics with decoded status and optional numeric metrics.
     */
    telemetry::PowerMetrics Collect();
};

} // namespace collectors
