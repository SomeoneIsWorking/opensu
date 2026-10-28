// folder_access — whether this process may read or write in a folder, as the OS decides it.
#pragma once

#include <cstdint>
#include <filesystem>

namespace opensu::fileio {

enum class FolderUse : std::uint8_t { Read, Write };

/// Whether this process may list (Read) or create files in (Write) `folder`. POSIX asks
/// access(2) (`folder_access_posix.cpp`); Windows lists or creates a probe file
/// (`folder_access_win32.cpp`).
[[nodiscard]] bool mayUseFolder(const std::filesystem::path& folder, FolderUse use);

} // namespace opensu::fileio
