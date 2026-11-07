cmake_minimum_required(VERSION 3.20)
project(WindowsDeviceTelemetry VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler Warnings and Flags
if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core Telemetry Library Sources
set(CORE_SOURCES
    src/collectors/system_cpu_collector.cpp
    src/collectors/memory_collector.cpp
    src/collectors/process_collector.cpp
    src/collectors/power_collector.cpp
    src/telemetry/telemetry_runner.cpp
    src/output/csv_writer.cpp
    src/common/cli_parser.cpp
)

# Common Include Directory
set(CORE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Main Executable
add_executable(device_telemetry src/main.cpp ${CORE_SOURCES})
target_include_directories(device_telemetry PRIVATE ${CORE_INCLUDE_DIR})

if(WIN32)
    target_link_libraries(device_telemetry PRIVATE kernel32 psapi)
endif()

# Enable CTest
enable_testing()

# Test 1: Metric Calculations
add_executable(metric_tests
    tests/metric_tests.cpp
    src/collectors/system_cpu_collector.cpp
    src/collectors/memory_collector.cpp
    src/collectors/process_collector.cpp
    src/collectors/power_collector.cpp
)
target_include_directories(metric_tests PRIVATE ${CORE_INCLUDE_DIR})
if(WIN32)
    target_link_libraries(metric_tests PRIVATE kernel32 psapi)
endif()
add_test(NAME MetricCalculations COMMAND metric_tests)

# Test 2: CSV Formatting and Escaping
add_executable(csv_tests
    tests/csv_tests.cpp
    src/output/csv_writer.cpp
)
target_include_directories(csv_tests PRIVATE ${CORE_INCLUDE_DIR})
add_test(NAME CsvOutputFormat COMMAND csv_tests)

# Test 3: CLI Parsing and Validation
add_executable(cli_tests
    tests/cli_tests.cpp
    src/common/cli_parser.cpp
)
target_include_directories(cli_tests PRIVATE ${CORE_INCLUDE_DIR})
add_test(NAME CliValidation COMMAND cli_tests)
