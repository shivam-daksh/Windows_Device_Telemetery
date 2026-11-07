#pragma once

#include <cstdint>
#include <string>
#include <ostream>

namespace common {

struct TelemetryConfig {
    uint32_t interval_ms = 1000;
    uint32_t duration_seconds = 60;
    std::string output_dir = "output";
    uint32_t top_processes = 20;
    bool include_processes = true;
    bool show_help = false;
    bool show_version = false;
};

struct CliParseResult {
    bool success = true;
    std::string error_message;
    TelemetryConfig config;
};

/**
 * @brief Parses and validates command-line arguments.
 *
 * @param argc Argument count.
 * @param argv Array of argument strings.
 * @return CliParseResult containing parsed configuration or error description.
 */
CliParseResult ParseCommandLine(int argc, const char* const argv[]);

/**
 * @brief Prints command-line usage and help documentation.
 *
 * @param os Output stream to write to.
 */
void PrintUsage(std::ostream& os);

/**
 * @brief Prints application version and metadata.
 *
 * @param os Output stream to write to.
 */
void PrintVersion(std::ostream& os);

} // namespace common
