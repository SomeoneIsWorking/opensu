// win32 — what the launch module's Windows backends share: a kernel handle closed with its owner,
// UTF-8 and UTF-16 text, and a command line CreateProcessW splits back into its arguments.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace opensu::launch::win32 {

/// A kernel handle closed when it goes out of scope.
class Handle {
  public:
    Handle() noexcept = default;
    explicit Handle(HANDLE handle) noexcept : handle_{handle} {
    }
    ~Handle() {
        close();
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&&) = delete;
    Handle& operator=(Handle&&) = delete;

    [[nodiscard]] HANDLE get() const noexcept {
        return handle_;
    }
    [[nodiscard]] bool valid() const noexcept {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }
    [[nodiscard]] HANDLE* put() noexcept {
        close();
        return &handle_;
    }
    /// Gives up the handle without closing it.
    [[nodiscard]] HANDLE release() noexcept {
        HANDLE handle = handle_;
        handle_ = nullptr;
        return handle;
    }
    void reset(HANDLE handle) noexcept {
        close();
        handle_ = handle;
    }
    void close() noexcept {
        if (valid()) {
            CloseHandle(handle_);
        }
        handle_ = nullptr;
    }

  private:
    HANDLE handle_{nullptr};
};

/// `text` (UTF-8) as UTF-16.
[[nodiscard]] inline std::wstring wide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int size =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

/// `text` (UTF-16) as UTF-8.
[[nodiscard]] inline std::string narrow(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

/// The last Win32 error, as a number for a log line.
[[nodiscard]] inline std::string lastError() {
    return std::to_string(GetLastError());
}

/// `program` and `args` as one command line that CommandLineToArgvW splits back unchanged.
[[nodiscard]] std::wstring commandLine(const std::string& program,
                                       const std::vector<std::string>& args);

} // namespace opensu::launch::win32
