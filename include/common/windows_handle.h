#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdint>
using HANDLE = void*;
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE (reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)))
#endif
inline int CloseHandle(HANDLE) { return 1; }
#endif

#include <utility>

namespace common {

/**
 * @brief RAII wrapper for native Windows HANDLE types.
 *
 * Ensures automatic resource cleanup via CloseHandle upon leaving scope.
 * Prevents handle leaks, supports move semantics, and prohibits copying.
 */
class WindowsHandle {
public:
    constexpr WindowsHandle() noexcept : handle_(nullptr) {}

    explicit WindowsHandle(HANDLE handle) noexcept : handle_(handle) {}

    ~WindowsHandle() noexcept {
        Close();
    }

    // Move-only semantics
    WindowsHandle(WindowsHandle&& other) noexcept : handle_(other.Release()) {}

    WindowsHandle& operator=(WindowsHandle&& other) noexcept {
        if (this != &other) {
            Reset(other.Release());
        }
        return *this;
    }

    WindowsHandle(const WindowsHandle&) = delete;
    WindowsHandle& operator=(const WindowsHandle&) = delete;

    [[nodiscard]] HANDLE Get() const noexcept {
        return handle_;
    }

    [[nodiscard]] HANDLE* Put() noexcept {
        Reset();
        return &handle_;
    }

    [[nodiscard]] HANDLE Release() noexcept {
        HANDLE temp = handle_;
        handle_ = nullptr;
        return temp;
    }

    void Reset(HANDLE new_handle = nullptr) noexcept {
        if (handle_ != new_handle) {
            Close();
            handle_ = new_handle;
        }
    }

    void Close() noexcept {
        if (IsValid()) {
#if defined(_WIN32)
            ::CloseHandle(handle_);
#else
            CloseHandle(handle_);
#endif
            handle_ = nullptr;
        }
    }

    [[nodiscard]] bool IsValid() const noexcept {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

    explicit operator bool() const noexcept {
        return IsValid();
    }

private:
    HANDLE handle_{nullptr};
};

} // namespace common
