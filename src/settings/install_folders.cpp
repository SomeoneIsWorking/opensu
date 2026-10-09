#include "install_folders.hpp"

#include <system_error>

#include <unistd.h>

namespace opensu::settings {

std::filesystem::path InstallFolders::storeFolder(library::Source store) const {
    const auto found = stores_.find(store);
    return found == stores_.end() ? std::filesystem::path{} : found->second;
}

void InstallFolders::setStore(library::Source store, std::filesystem::path folder) {
    if (folder.empty()) {
        stores_.erase(store);
    } else {
        stores_[store] = std::move(folder);
    }
}

std::optional<std::filesystem::path> InstallFolders::effective(library::Source store) const {
    if (std::filesystem::path own = storeFolder(store); !own.empty()) {
        return own;
    }
    if (!default_.empty()) {
        return default_;
    }
    return std::nullopt;
}

std::string refusal(const std::filesystem::path& folder, Need need) {
    if (!folder.is_absolute()) {
        return "the folder must be an absolute path";
    }
    std::error_code error;
    if (!std::filesystem::exists(folder, error)) {
        return "that folder does not exist";
    }
    if (!std::filesystem::is_directory(folder, error)) {
        return "that is not a folder";
    }
    if (access(folder.c_str(), (need == Need::Write ? W_OK : R_OK) | X_OK) != 0) {
        return need == Need::Write ? "that folder cannot be written to"
                                   : "that folder cannot be read";
    }
    return {};
}

} // namespace opensu::settings
