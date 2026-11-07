#include "output/csv_writer.h"
#include "telemetry/telemetry_sample.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "[-] Assertion FAILED: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

void TestCsvEscaping() {
    std::cout << "[*] Running TestCsvEscaping...\n";

    // Plain string without special chars
    ASSERT_EQ(output::EscapeCsvField("svchost.exe"), "svchost.exe");

    // String with comma
    ASSERT_EQ(output::EscapeCsvField("hello,world"), "\"hello,world\"");

    // String with double quotes
    ASSERT_EQ(output::EscapeCsvField("process \"name\""), "\"process \"\"name\"\"\"");

    // String with newline
    ASSERT_EQ(output::EscapeCsvField("line1\nline2"), "\"line1\nline2\"");

    // String with comma and quotes
    ASSERT_EQ(output::EscapeCsvField("a, \"b\", c"), "\"a, \"\"b\"\", c\"");
}

void TestSystemRecordFormatting() {
    std::cout << "[*] Running TestSystemRecordFormatting...\n";

    telemetry::SystemMetricsRecord rec;
    rec.timestamp_utc = "2026-11-08T00:00:00.000Z";
    rec.elapsed_ms = 1000;
    rec.cpu_utilization_percent = 25.50;
    rec.physical_memory_total_bytes = 17179869184ULL;
    rec.physical_memory_available_bytes = 8589934592ULL;
    rec.physical_memory_used_percent = 50.00;
    rec.virtual_memory_total_bytes = 34359738368ULL;
    rec.virtual_memory_available_bytes = 20000000000ULL;
    rec.memory_load_percent = 50;
    rec.ac_line_status = "Online";
    rec.battery_flag = "Charging";
    rec.battery_percent = 85;
    rec.battery_remaining_seconds = 3600;
    rec.skipped_process_count = 5;

    std::string formatted = output::FormatSystemRecord(rec);
    std::string expected =
        "2026-11-08T00:00:00.000Z,1000,25.50,17179869184,8589934592,50.00,34359738368,"
        "20000000000,50,Online,Charging,85,3600,5";

    ASSERT_EQ(formatted, expected);

    // Test with unavailable (nullopt) values: must be empty, not 0!
    rec.cpu_utilization_percent = std::nullopt;
    rec.physical_memory_used_percent = std::nullopt;
    rec.battery_percent = std::nullopt;
    rec.battery_remaining_seconds = std::nullopt;

    std::string formatted_nulls = output::FormatSystemRecord(rec);
    std::string expected_nulls =
        "2026-11-08T00:00:00.000Z,1000,,17179869184,8589934592,,34359738368,"
        "20000000000,50,Online,Charging,,,5";

    ASSERT_EQ(formatted_nulls, expected_nulls);
}

void TestProcessRecordFormatting() {
    std::cout << "[*] Running TestProcessRecordFormatting...\n";

    telemetry::ProcessMetricsRecord rec;
    rec.timestamp_utc = "2026-11-08T00:00:00.000Z";
    rec.process_id = 1234;
    rec.process_name = "test_proc.exe";
    rec.process_cpu_percent = 4.25;
    rec.working_set_bytes = 10485760;
    rec.page_fault_count = 350;

    std::string formatted = output::FormatProcessRecord(rec);
    std::string expected = "2026-11-08T00:00:00.000Z,1234,test_proc.exe,4.25,10485760,350";
    ASSERT_EQ(formatted, expected);

    // Test with nullopt CPU (initial sample)
    rec.process_cpu_percent = std::nullopt;
    std::string formatted_init = output::FormatProcessRecord(rec);
    std::string expected_init = "2026-11-08T00:00:00.000Z,1234,test_proc.exe,,10485760,350";
    ASSERT_EQ(formatted_init, expected_init);

    // Test process name with comma or quotes
    rec.process_name = "proc, \"special\".exe";
    std::string formatted_esc = output::FormatProcessRecord(rec);
    std::string expected_esc = "2026-11-08T00:00:00.000Z,1234,\"proc, \"\"special\"\".exe\",,10485760,350";
    ASSERT_EQ(formatted_esc, expected_esc);
}

void TestCsvWriterFileOperations() {
    std::cout << "[*] Running TestCsvWriterFileOperations...\n";

    const std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "telemetry_test_output";
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);

    {
        output::CsvWriter writer;
        ASSERT_TRUE(writer.Initialize(test_dir, true));
        ASSERT_TRUE(writer.IsOpen());

        telemetry::SystemMetricsRecord sys_rec;
        sys_rec.timestamp_utc = "2026-11-08T00:00:00.000Z";
        sys_rec.elapsed_ms = 0;
        sys_rec.ac_line_status = "Online";
        sys_rec.battery_flag = "NoBattery";

        ASSERT_TRUE(writer.WriteSystemRecord(sys_rec));

        telemetry::ProcessMetricsRecord proc_rec;
        proc_rec.timestamp_utc = "2026-11-08T00:00:00.000Z";
        proc_rec.process_id = 4;
        proc_rec.process_name = "System";
        proc_rec.working_set_bytes = 4096;

        ASSERT_TRUE(writer.WriteProcessRecord(proc_rec));
        writer.Close();
    }

    // Verify system CSV contains header and 1 data line
    const auto sys_path = test_dir / "system_metrics.csv";
    ASSERT_TRUE(std::filesystem::exists(sys_path));
    std::ifstream sys_in(sys_path);
    std::string line1, line2;
    ASSERT_TRUE(static_cast<bool>(std::getline(sys_in, line1)));
    ASSERT_TRUE(line1 == output::CsvWriter::kSystemHeader);
    ASSERT_TRUE(static_cast<bool>(std::getline(sys_in, line2)));
    ASSERT_FALSE(line2.empty());

    // Verify process CSV contains header and 1 data line
    const auto proc_path = test_dir / "process_metrics.csv";
    ASSERT_TRUE(std::filesystem::exists(proc_path));
    std::ifstream proc_in(proc_path);
    ASSERT_TRUE(static_cast<bool>(std::getline(proc_in, line1)));
    ASSERT_TRUE(line1 == output::CsvWriter::kProcessHeader);
    ASSERT_TRUE(static_cast<bool>(std::getline(proc_in, line2)));
    ASSERT_FALSE(line2.empty());

    std::filesystem::remove_all(test_dir, ec);
}

void TestInvalidOutputLocation() {
    std::cout << "[*] Running TestInvalidOutputLocation...\n";

    // Create a regular file, then attempt to use that regular file as an output directory
    const std::filesystem::path dummy_file = std::filesystem::temp_directory_path() / "telemetry_dummy_file.txt";
    std::ofstream(dummy_file) << "dummy";

    const std::filesystem::path invalid_dir = dummy_file / "sub_dir";

    output::CsvWriter writer;
    // Must fail safely without crashing
    const bool init_result = writer.Initialize(invalid_dir, true);
    ASSERT_FALSE(init_result);

    std::error_code ec;
    std::filesystem::remove(dummy_file, ec);
}

int main() {
    std::cout << "========================================\n"
              << "Running Device Telemetry CSV Tests\n"
              << "========================================\n";

    TestCsvEscaping();
    TestSystemRecordFormatting();
    TestProcessRecordFormatting();
    TestCsvWriterFileOperations();
    TestInvalidOutputLocation();

    std::cout << "[+] All CSV tests PASSED successfully!\n";
    return 0;
}
