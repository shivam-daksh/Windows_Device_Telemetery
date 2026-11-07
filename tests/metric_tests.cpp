#include "collectors/system_cpu_collector.h"
#include "collectors/memory_collector.h"
#include "collectors/process_collector.h"
#include "collectors/power_collector.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "[-] Assertion FAILED: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_NEAR(a, b, eps) \
    do { \
        if (std::fabs((a) - (b)) > (eps)) { \
            std::cerr << "[-] Assertion FAILED: |" #a " - " #b "| <= " #eps \
                      << " (got " << (a) << " vs " << (b) << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void TestSystemCpuCalculations() {
    std::cout << "[*] Running TestSystemCpuCalculations...\n";

    // Standard case:
    // prev: idle=1000, kernel=2000, user=1000
    // curr: idle=1200, kernel=2500, user=1500
    // delta_idle = 200, delta_kernel = 500, delta_user = 500
    // total = 500 + 500 = 1000
    // busy = total - delta_idle = 800
    // cpu% = 80.0%
    auto cpu = collectors::CalculateSystemCpuUtilization(1000, 2000, 1000, 1200, 2500, 1500);
    ASSERT_TRUE(cpu.has_value());
    ASSERT_NEAR(cpu.value(), 80.0, 0.001);

    // 100% Busy: delta_idle = 0
    auto cpu_max = collectors::CalculateSystemCpuUtilization(1000, 2000, 1000, 1000, 2500, 1500);
    ASSERT_TRUE(cpu_max.has_value());
    ASSERT_NEAR(cpu_max.value(), 100.0, 0.001);

    // 0% Busy: delta_idle == total
    auto cpu_idle = collectors::CalculateSystemCpuUtilization(1000, 2000, 1000, 2000, 2500, 1500);
    ASSERT_TRUE(cpu_idle.has_value());
    ASSERT_NEAR(cpu_idle.value(), 0.0, 0.001);

    // Zero time delta: curr == prev
    auto cpu_zero = collectors::CalculateSystemCpuUtilization(1000, 2000, 1000, 1000, 2000, 1000);
    ASSERT_FALSE(cpu_zero.has_value());

    // Decreasing kernel counter
    auto cpu_dec_kernel = collectors::CalculateSystemCpuUtilization(1000, 2000, 1000, 1200, 1999, 1500);
    ASSERT_FALSE(cpu_dec_kernel.has_value());

    // Decreasing user counter
    auto cpu_dec_user = collectors::CalculateSystemCpuUtilization(1000, 2000, 1000, 1200, 2500, 999);
    ASSERT_FALSE(cpu_dec_user.has_value());

    // Decreasing idle counter
    auto cpu_dec_idle = collectors::CalculateSystemCpuUtilization(1000, 2000, 1000, 999, 2500, 1500);
    ASSERT_FALSE(cpu_dec_idle.has_value());

    // Anomaly: delta_idle slightly exceeds total due to timer skew -> clamps safely to 0.0%
    auto cpu_skew = collectors::CalculateSystemCpuUtilization(1000, 2000, 1000, 2050, 2500, 1500);
    ASSERT_TRUE(cpu_skew.has_value());
    ASSERT_NEAR(cpu_skew.value(), 0.0, 0.001);
}

void TestMemoryCalculations() {
    std::cout << "[*] Running TestMemoryCalculations...\n";

    // 4 GB used out of 16 GB = 25.0%
    const uint64_t four_gb = 4ULL * 1024 * 1024 * 1024;
    const uint64_t sixteen_gb = 16ULL * 1024 * 1024 * 1024;
    auto mem_pct = collectors::CalculateMemoryUtilizationPercent(four_gb, sixteen_gb);
    ASSERT_TRUE(mem_pct.has_value());
    ASSERT_NEAR(mem_pct.value(), 25.0, 0.001);

    // 0 GB used = 0.0%
    auto mem_zero = collectors::CalculateMemoryUtilizationPercent(0, sixteen_gb);
    ASSERT_TRUE(mem_zero.has_value());
    ASSERT_NEAR(mem_zero.value(), 0.0, 0.001);

    // Total = 0 bytes -> nullopt
    auto mem_no_total = collectors::CalculateMemoryUtilizationPercent(1000, 0);
    ASSERT_FALSE(mem_no_total.has_value());

    // Used > Total -> clamps to 100.0%
    auto mem_over = collectors::CalculateMemoryUtilizationPercent(20ULL * 1024 * 1024 * 1024, sixteen_gb);
    ASSERT_TRUE(mem_over.has_value());
    ASSERT_NEAR(mem_over.value(), 100.0, 0.001);
}

void TestProcessCpuCalculations() {
    std::cout << "[*] Running TestProcessCpuCalculations...\n";

    // Process used 1.0 second of CPU time over 1.0 second wall-clock on 4-core machine:
    // delta_cpu_100ns = 10,000,000 (1.0 second)
    // elapsed_wall_ns = 1,000,000,000 ns (1.0 second)
    // logical_processors = 4
    // Expected = 25.0%
    const uint64_t one_sec_100ns = 10'000'000ULL;
    const uint64_t one_sec_ns = 1'000'000'000ULL;
    auto proc_cpu = collectors::CalculateProcessCpuPercent(
        0, one_sec_100ns, one_sec_ns, 4);
    ASSERT_TRUE(proc_cpu.has_value());
    ASSERT_NEAR(proc_cpu.value(), 25.0, 0.001);

    // Fully saturating 4 cores on a 4-core machine:
    // delta_cpu_100ns = 40,000,000 (4.0 seconds across cores)
    auto proc_full = collectors::CalculateProcessCpuPercent(
        0, 4 * one_sec_100ns, one_sec_ns, 4);
    ASSERT_TRUE(proc_full.has_value());
    ASSERT_NEAR(proc_full.value(), 100.0, 0.001);

    // Single core 100% on 8 cores = 12.5%
    auto proc_eight_core = collectors::CalculateProcessCpuPercent(
        0, one_sec_100ns, one_sec_ns, 8);
    ASSERT_TRUE(proc_eight_core.has_value());
    ASSERT_NEAR(proc_eight_core.value(), 12.5, 0.001);

    // Zero wall-clock time -> nullopt
    auto proc_zero_wall = collectors::CalculateProcessCpuPercent(0, one_sec_100ns, 0, 4);
    ASSERT_FALSE(proc_zero_wall.has_value());

    // Zero logical processors -> nullopt
    auto proc_zero_cores = collectors::CalculateProcessCpuPercent(0, one_sec_100ns, one_sec_ns, 0);
    ASSERT_FALSE(proc_zero_cores.has_value());

    // Decreasing process CPU time -> nullopt
    auto proc_decreasing = collectors::CalculateProcessCpuPercent(500, 400, one_sec_ns, 4);
    ASSERT_FALSE(proc_decreasing.has_value());
}

void TestPidReuseProtection() {
    std::cout << "[*] Running TestPidReuseProtection...\n";

    const uint64_t create_time_a = 133500000000000000ULL;
    const uint64_t create_time_b = 133500000500000000ULL;

    // Previous sample exists, same creation timestamp -> Same process instance!
    ASSERT_TRUE(collectors::IsSameProcessInstance(true, create_time_a, create_time_a));

    // Previous sample exists, DIFFERENT creation timestamp -> PID reused by new process!
    ASSERT_FALSE(collectors::IsSameProcessInstance(true, create_time_a, create_time_b));

    // First sample for PID (has_previous = false) -> Cannot calculate delta
    ASSERT_FALSE(collectors::IsSameProcessInstance(false, create_time_a, create_time_a));
}

void TestPowerTelemetryDecoding() {
    std::cout << "[*] Running TestPowerTelemetryDecoding...\n";

    // AC Line status
    ASSERT_TRUE(collectors::DecodeAcLineStatus(0) == "Offline");
    ASSERT_TRUE(collectors::DecodeAcLineStatus(1) == "Online");
    ASSERT_TRUE(collectors::DecodeAcLineStatus(255) == "Unknown");
    ASSERT_TRUE(collectors::DecodeAcLineStatus(42) == "Unknown");

    // Battery flags
    ASSERT_TRUE(collectors::DecodeBatteryFlag(128) == "NoBattery");
    ASSERT_TRUE(collectors::DecodeBatteryFlag(255) == "Unknown");
    ASSERT_TRUE(collectors::DecodeBatteryFlag(1) == "High");
    ASSERT_TRUE(collectors::DecodeBatteryFlag(2) == "Low");
    ASSERT_TRUE(collectors::DecodeBatteryFlag(4) == "Critical");
    ASSERT_TRUE(collectors::DecodeBatteryFlag(8) == "Charging");
    ASSERT_TRUE(collectors::DecodeBatteryFlag(9) == "Charging|High");
    ASSERT_TRUE(collectors::DecodeBatteryFlag(10) == "Charging|Low");
    ASSERT_TRUE(collectors::DecodeBatteryFlag(12) == "Charging|Critical");
    ASSERT_TRUE(collectors::DecodeBatteryFlag(0) == "Normal");

    // Battery percent
    auto pct_valid = collectors::DecodeBatteryPercent(85);
    ASSERT_TRUE(pct_valid.has_value());
    ASSERT_TRUE(pct_valid.value() == 85);

    auto pct_zero = collectors::DecodeBatteryPercent(0);
    ASSERT_TRUE(pct_zero.has_value());
    ASSERT_TRUE(pct_zero.value() == 0);

    auto pct_unknown = collectors::DecodeBatteryPercent(255);
    ASSERT_FALSE(pct_unknown.has_value()); // Must NOT be treated as zero

    auto pct_invalid = collectors::DecodeBatteryPercent(150);
    ASSERT_FALSE(pct_invalid.has_value());

    // Battery remaining time
    auto time_valid = collectors::DecodeBatteryRemainingSeconds(7200);
    ASSERT_TRUE(time_valid.has_value());
    ASSERT_TRUE(time_valid.value() == 7200);

    auto time_unknown = collectors::DecodeBatteryRemainingSeconds(0xFFFFFFFF);
    ASSERT_FALSE(time_unknown.has_value()); // Must NOT be treated as zero

    auto time_unknown_cast = collectors::DecodeBatteryRemainingSeconds(static_cast<uint32_t>(-1));
    ASSERT_FALSE(time_unknown_cast.has_value());
}

int main() {
    std::cout << "========================================\n"
              << "Running Device Telemetry Metric Tests\n"
              << "========================================\n";

    TestSystemCpuCalculations();
    TestMemoryCalculations();
    TestProcessCpuCalculations();
    TestPidReuseProtection();
    TestPowerTelemetryDecoding();

    std::cout << "[+] All metric tests PASSED successfully!\n";
    return 0;
}
