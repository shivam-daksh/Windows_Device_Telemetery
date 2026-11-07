#include "common/cli_parser.h"
#include "telemetry/telemetry_runner.h"

#include <iostream>

int main(int argc, char* argv[]) {
    const auto parse_result = common::ParseCommandLine(argc, argv);

    if (parse_result.config.show_help) {
        common::PrintUsage(std::cout);
        return 0;
    }

    if (parse_result.config.show_version) {
        common::PrintVersion(std::cout);
        return 0;
    }

    if (!parse_result.success) {
        std::cerr << "[-] Error: " << parse_result.error_message << "\n\n";
        common::PrintUsage(std::cerr);
        return 1;
    }

    telemetry::TelemetryRunner runner(parse_result.config);

    if (!runner.Initialize()) {
        std::cerr << "[-] Fatal: Failed to initialize telemetry runner. Exiting.\n";
        return 1;
    }

    return runner.Run();
}
