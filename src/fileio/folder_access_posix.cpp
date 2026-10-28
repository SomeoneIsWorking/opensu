#include "folder_access.hpp"

#include <unistd.h>

namespace opensu::fileio {

bool mayUseFolder(const std::filesystem::path& folder, FolderUse use) {
    return access(folder.c_str(), (use == FolderUse::Write ? W_OK : R_OK) | X_OK) == 0;
}

} // namespace opensu::fileio
