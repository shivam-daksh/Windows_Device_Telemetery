#include "common/cli_parser.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string_view>
#include <vector>

namespace common {

namespace {

bool ParseUnsignedInt(std::string_view sv, uint32_t& out_value) {
    if (sv.empty()) {
        return false;
    }
    // Disallow leading spaces or signs
    for (char c : sv) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    const auto* first = sv.data();
    const auto* last = sv.data() + sv.size();
    auto [ptr, ec] = std::from_chars(first, last, out_value);
    return ec == std::errc() && ptr == last;
}

std::string ToLower(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool ParseBool(std::string_view sv, bool& out_val) {
    std::string lower = ToLower(sv);
    if (lower == "true" || lower == "1" || lower == "yes") {
        out_val = true;
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no") {
        out_val = false;
        return true;
    }
    return false;
}

} // namespace

CliParseResult ParseCommandLine(int argc, const char* const argv[]) {
    CliParseResult result;
    std::vector<std::string_view> args;
    args.reserve(argc > 0 ? static_cast<size_t>(argc - 1) : 0);
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr) {
            args.emplace_back(argv[i]);
        }
    }

    for (size_t i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];

        if (arg == "--help" || arg == "-h" || arg == "/?") {
            result.config.show_help = true;
            return result;
        }
        if (arg == "--version" || arg == "-v") {
            result.config.show_version = true;
            return result;
        }

        std::string_view opt_name;
        std::string_view opt_val;
        bool has_inline_val = false;

        const auto eq_pos = arg.find('=');
        if (eq_pos != std::string_view::npos) {
            opt_name = arg.substr(0, eq_pos);
            opt_val = arg.substr(eq_pos + 1);
            has_inline_val = true;
        } else {
            opt_name = arg;
        }

        auto get_value = [&](std::string_view& val) -> bool {
            if (has_inline_val) {
                val = opt_val;
                return true;
            }
            if (i + 1 < args.size()) {
                val = args[++i];
                return true;
            }
            result.success = false;
            result.error_message = "Missing value for option '" + std::string(opt_name) + "'.";
            return false;
        };

        if (opt_name == "--interval-ms" || opt_name == "-i") {
            std::string_view val_str;
            if (!get_value(val_str)) return result;

            uint32_t val = 0;
            if (!ParseUnsignedInt(val_str, val)) {
                result.success = false;
                result.error_message = "Invalid numeric value for --interval-ms: '" + std::string(val_str) + "'.";
                return result;
            }
            if (val < 250) {
                result.success = false;
                result.error_message = "Sampling interval must be at least 250 ms (got " + std::to_string(val) + " ms).";
                return result;
            }
            if (val > 60000) {
                result.success = false;
                result.error_message = "Sampling interval cannot exceed 60000 ms (got " + std::to_string(val) + " ms).";
                return result;
            }
            result.config.interval_ms = val;
        } else if (opt_name == "--duration-seconds" || opt_name == "-d") {
            std::string_view val_str;
            if (!get_value(val_str)) return result;

            uint32_t val = 0;
            if (!ParseUnsignedInt(val_str, val)) {
                result.success = false;
                result.error_message = "Invalid numeric value for --duration-seconds: '" + std::string(val_str) + "'.";
                return result;
            }
            if (val == 0) {
                result.success = false;
                result.error_message = "Duration must be a positive integer greater than zero.";
                return result;
            }
            if (val > 31536000) { // 1 year limit to prevent overflow
                result.success = false;
                result.error_message = "Duration exceeds maximum supported limit (31536000 seconds).";
                return result;
            }
            result.config.duration_seconds = val;
        } else if (opt_name == "--output-dir" || opt_name == "-o") {
            std::string_view val_str;
            if (!get_value(val_str)) return result;

            if (val_str.empty()) {
                result.success = false;
                result.error_message = "Output directory path cannot be empty.";
                return result;
            }
            result.config.output_dir = std::string(val_str);
        } else if (opt_name == "--top-processes" || opt_name == "-n") {
            std::string_view val_str;
            if (!get_value(val_str)) return result;

            uint32_t val = 0;
            if (!ParseUnsignedInt(val_str, val)) {
                result.success = false;
                result.error_message = "Invalid numeric value for --top-processes: '" + std::string(val_str) + "'.";
                return result;
            }
            if (val > 1000) {
                result.success = false;
                result.error_message = "Top processes count cannot exceed 1000 to prevent unbounded allocations (got " +
                                       std::to_string(val) + ").";
                return result;
            }
            result.config.top_processes = val;
        } else if (opt_name == "--include-processes") {
            std::string_view val_str;
            if (!get_value(val_str)) return result;

            bool val = false;
            if (!ParseBool(val_str, val)) {
                result.success = false;
                result.error_message = "Invalid boolean value for --include-processes: '" + std::string(val_str) +
                                       "' (expected true or false).";
                return result;
            }
            result.config.include_processes = val;
        } else {
            result.success = false;
            result.error_message = "Unrecognized command-line argument: '" + std::string(opt_name) + "'.";
            return result;
        }
    }

    return result;
}

void PrintUsage(std::ostream& os) {
    os << "Windows Device Telemetry Monitor\n"
       << "Usage:\n"
       << "  device_telemetry.exe [options]\n\n"
       << "Options:\n"
       << "  --interval-ms <N>           Sampling interval in milliseconds [250 - 60000] (default: 1000)\n"
       << "  --duration-seconds <N>      Monitoring duration in seconds (> 0) (default: 60)\n"
       << "  --output-dir <PATH>         Directory for CSV telemetry output files (default: output)\n"
       << "  --top-processes <N>         Maximum number of top CPU processes to record [0 - 1000] (default: 20)\n"
       << "  --include-processes <BOOL>  Enable or disable per-process telemetry (true|false) (default: true)\n"
       << "  --help, -h                  Display this help message and exit\n"
       << "  --version, -v               Display version information and exit\n\n"
       << "Examples:\n"
       << "  device_telemetry.exe --interval-ms 1000 --duration-seconds 60 --output-dir output\n"
       << "  device_telemetry.exe --interval-ms 500 --top-processes 10\n"
       << "  device_telemetry.exe --include-processes false --duration-seconds 300\n";
}

void PrintVersion(std::ostream& os) {
    os << "Windows Device Telemetry Monitor v1.0.0\n"
       << "Architecture: x64 | Target: Windows 10 / Windows 11\n"
       << "Native C++20 Telemetry Engineering Tool\n";
}

} // namespace common
