#include "instance_lock.hpp"

#include <system_error>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "lucent/log.h"

namespace opensu::fileio {

struct InstanceLock::File {
    HANDLE handle{INVALID_HANDLE_VALUE};
};

InstanceLock::InstanceLock(const std::filesystem::path& file) : file_{std::make_unique<File>()} {
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    // No sharing: a second openSU cannot open the file while the first holds it.
    file_->handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_->handle == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        if (code != ERROR_SHARING_VIOLATION) {
            lucent::warn("host", "cannot open {}: error {}", file.string(), code);
        }
        return;
    }
    held_ = true;
}

InstanceLock::~InstanceLock() {
    if (file_->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(file_->handle);
    }
}

} // namespace opensu::fileio
