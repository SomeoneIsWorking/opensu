#include "folder_access.hpp"

#include <string>
#include <system_error>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace opensu::fileio {

bool mayUseFolder(const std::filesystem::path& folder, FolderUse use) {
    if (use == FolderUse::Read) {
        std::error_code error;
        const std::filesystem::directory_iterator listing{folder, error};
        return !error;
    }
    // ACLs decide; the only sure answer is creating a file, gone again when its handle closes.
    const std::filesystem::path probe =
        folder / (L".opensu-probe-" + std::to_wstring(GetCurrentProcessId()));
    HANDLE file = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(file);
    return true;
}

} // namespace opensu::fileio
