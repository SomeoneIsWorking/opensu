#include "owner_only_write.hpp"

#include <system_error>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace opensu::fileio {
namespace {

namespace fs = std::filesystem;

/// Writes all of `bytes` to `file`, then flushes it to disk.
bool writeAll(HANDLE file, std::string_view bytes) {
    while (!bytes.empty()) {
        DWORD wrote = 0;
        if (WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &wrote, nullptr) == 0 ||
            wrote == 0) {
            return false;
        }
        bytes.remove_prefix(wrote);
    }
    return FlushFileBuffers(file) != 0;
}

bool failed(const fs::path& file, const char* what, DWORD code, std::string& error) {
    error = std::string{"cannot "} + what + " " + file.string() + ": " +
            std::system_category().message(static_cast<int>(code));
    return false;
}

} // namespace

// The profile's own folders are readable by their user alone; the file inherits that.
bool writeOwnerOnly(const fs::path& file, std::string_view bytes, std::string& error) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    if (ec) {
        error = "cannot create " + file.parent_path().string() + ": " + ec.message();
        return false;
    }
    fs::path staged = file;
    staged += ".part-" + std::to_string(GetCurrentProcessId());
    HANDLE out = CreateFileW(staged.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        return failed(file, "create", GetLastError(), error);
    }
    const bool written = writeAll(out, bytes);
    const DWORD writeError = GetLastError();
    CloseHandle(out);
    if (!written) {
        fs::remove(staged, ec);
        return failed(file, "write", writeError, error);
    }
    if (MoveFileExW(staged.c_str(), file.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        const DWORD moveError = GetLastError();
        fs::remove(staged, ec);
        return failed(file, "replace", moveError, error);
    }
    return true;
}

} // namespace opensu::fileio
