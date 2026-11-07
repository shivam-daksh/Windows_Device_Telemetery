#pragma once

#include <string>
#include <cstdint>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace common {

/**
 * @brief Formats a Windows API error code into a human-readable error description.
 *
 * @param error_code The system error code (typically obtained via GetLastError()).
 * @return A descriptive error string.
 */
inline std::string FormatWindowsError(uint32_t error_code) {
#if defined(_WIN32)
    if (error_code == 0) {
        return "Operation completed successfully.";
    }

    LPSTR message_buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD size = ::FormatMessageA(
        flags,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message_buffer),
        0,
        nullptr);

    if (size == 0 || message_buffer == nullptr) {
        return "Unknown error code: " + std::to_string(error_code);
    }

    std::string message(message_buffer, size);
    ::LocalFree(message_buffer);

    // Strip trailing newlines and carriage returns
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' ')) {
        message.pop_back();
    }

    return message + " (Error Code: " + std::to_string(error_code) + ")";
#else
    return "Error Code: " + std::to_string(error_code);
#endif
}

} // namespace common
