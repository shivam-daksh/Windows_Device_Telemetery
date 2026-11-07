#include "common/cli_parser.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "[-] Assertion FAILED: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

void TestDefaultArguments() {
    std::cout << "[*] Running TestDefaultArguments...\n";

    const char* argv[] = {"device_telemetry.exe"};
    const auto res = common::ParseCommandLine(1, argv);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.config.interval_ms, 1000u);
    ASSERT_EQ(res.config.duration_seconds, 60u);
    ASSERT_EQ(res.config.output_dir, "output");
    ASSERT_EQ(res.config.top_processes, 20u);
    ASSERT_TRUE(res.config.include_processes);
    ASSERT_FALSE(res.config.show_help);
    ASSERT_FALSE(res.config.show_version);
}

void TestCustomValidArguments() {
    std::cout << "[*] Running TestCustomValidArguments...\n";

    const char* argv[] = {
        "device_telemetry.exe",
        "--interval-ms", "500",
        "--duration-seconds", "120",
        "--output-dir", "custom_output",
        "--top-processes", "50",
        "--include-processes", "false"
    };
    const auto res = common::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);

    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.config.interval_ms, 500u);
    ASSERT_EQ(res.config.duration_seconds, 120u);
    ASSERT_EQ(res.config.output_dir, "custom_output");
    ASSERT_EQ(res.config.top_processes, 50u);
    ASSERT_FALSE(res.config.include_processes);

    // Test inline equals syntax
    const char* argv_eq[] = {
        "device_telemetry.exe",
        "--interval-ms=2500",
        "--include-processes=true"
    };
    const auto res_eq = common::ParseCommandLine(sizeof(argv_eq) / sizeof(argv_eq[0]), argv_eq);
    ASSERT_TRUE(res_eq.success);
    ASSERT_EQ(res_eq.config.interval_ms, 2500u);
    ASSERT_TRUE(res_eq.config.include_processes);
}

void TestIntervalBoundaries() {
    std::cout << "[*] Running TestIntervalBoundaries...\n";

    // Min boundary (250 ms): Valid
    const char* argv_min[] = {"device_telemetry.exe", "--interval-ms", "250"};
    ASSERT_TRUE(common::ParseCommandLine(3, argv_min).success);

    // Below min (249 ms): Invalid
    const char* argv_under[] = {"device_telemetry.exe", "--interval-ms", "249"};
    const auto res_under = common::ParseCommandLine(3, argv_under);
    ASSERT_FALSE(res_under.success);

    // Max boundary (60000 ms): Valid
    const char* argv_max[] = {"device_telemetry.exe", "--interval-ms", "60000"};
    ASSERT_TRUE(common::ParseCommandLine(3, argv_max).success);

    // Above max (60001 ms): Invalid
    const char* argv_over[] = {"device_telemetry.exe", "--interval-ms", "60001"};
    const auto res_over = common::ParseCommandLine(3, argv_over);
    ASSERT_FALSE(res_over.success);
}

void TestDurationValidation() {
    std::cout << "[*] Running TestDurationValidation...\n";

    // Duration = 0: Invalid
    const char* argv_zero[] = {"device_telemetry.exe", "--duration-seconds", "0"};
    ASSERT_FALSE(common::ParseCommandLine(3, argv_zero).success);

    // Duration = 1: Valid
    const char* argv_one[] = {"device_telemetry.exe", "--duration-seconds", "1"};
    ASSERT_TRUE(common::ParseCommandLine(3, argv_one).success);
}

void TestTopProcessesBoundaries() {
    std::cout << "[*] Running TestTopProcessesBoundaries...\n";

    // 0: Valid (disables process recording or top 0)
    const char* argv_zero[] = {"device_telemetry.exe", "--top-processes", "0"};
    ASSERT_TRUE(common::ParseCommandLine(3, argv_zero).success);

    // 1000: Valid (max boundary)
    const char* argv_max[] = {"device_telemetry.exe", "--top-processes", "1000"};
    ASSERT_TRUE(common::ParseCommandLine(3, argv_max).success);

    // 1001: Invalid (exceeds 1000 to prevent unbounded allocation)
    const char* argv_over[] = {"device_telemetry.exe", "--top-processes", "1001"};
    ASSERT_FALSE(common::ParseCommandLine(3, argv_over).success);
}

void TestInvalidInputs() {
    std::cout << "[*] Running TestInvalidInputs...\n";

    // Non-numeric interval
    const char* argv_nan_interval[] = {"device_telemetry.exe", "--interval-ms", "invalid"};
    ASSERT_FALSE(common::ParseCommandLine(3, argv_nan_interval).success);

    // Negative interval
    const char* argv_neg_interval[] = {"device_telemetry.exe", "--interval-ms", "-500"};
    ASSERT_FALSE(common::ParseCommandLine(3, argv_neg_interval).success);

    // Non-numeric duration
    const char* argv_nan_duration[] = {"device_telemetry.exe", "--duration-seconds", "not_a_number"};
    ASSERT_FALSE(common::ParseCommandLine(3, argv_nan_duration).success);

    // Invalid boolean for --include-processes
    const char* argv_bad_bool[] = {"device_telemetry.exe", "--include-processes", "maybe"};
    ASSERT_FALSE(common::ParseCommandLine(3, argv_bad_bool).success);

    // Missing value at end of arguments
    const char* argv_missing[] = {"device_telemetry.exe", "--interval-ms"};
    ASSERT_FALSE(common::ParseCommandLine(2, argv_missing).success);

    // Unknown argument
    const char* argv_unknown[] = {"device_telemetry.exe", "--unknown-flag"};
    ASSERT_FALSE(common::ParseCommandLine(2, argv_unknown).success);
}

void TestHelpAndVersionFlags() {
    std::cout << "[*] Running TestHelpAndVersionFlags...\n";

    const char* argv_help1[] = {"device_telemetry.exe", "--help"};
    ASSERT_TRUE(common::ParseCommandLine(2, argv_help1).config.show_help);

    const char* argv_help2[] = {"device_telemetry.exe", "-h"};
    ASSERT_TRUE(common::ParseCommandLine(2, argv_help2).config.show_help);

    const char* argv_ver1[] = {"device_telemetry.exe", "--version"};
    ASSERT_TRUE(common::ParseCommandLine(2, argv_ver1).config.show_version);

    const char* argv_ver2[] = {"device_telemetry.exe", "-v"};
    ASSERT_TRUE(common::ParseCommandLine(2, argv_ver2).config.show_version);
}

int main() {
    std::cout << "========================================\n"
              << "Running Device Telemetry CLI Tests\n"
              << "========================================\n";

    TestDefaultArguments();
    TestCustomValidArguments();
    TestIntervalBoundaries();
    TestDurationValidation();
    TestTopProcessesBoundaries();
    TestInvalidInputs();
    TestHelpAndVersionFlags();

    std::cout << "[+] All CLI tests PASSED successfully!\n";
    return 0;
}
