#include "theme_folders.hpp"

#include <algorithm>
#include <system_error>

namespace opensu::theme {
namespace {

/// The folders in `root` with a manifest, by name.
std::vector<std::filesystem::path> foldersIn(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> folders;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator{root, ec}) {
        if (entry.is_directory(ec) &&
            std::filesystem::is_regular_file(entry.path() / manifestFile, ec)) {
            folders.push_back(entry.path());
        }
    }
    std::ranges::sort(folders);
    return folders;
}

/// Whether `name` is one folder's name: no separator, not a step up.
bool plainName(std::string_view name) {
    return !name.empty() && name != "." && name != ".." &&
           name.find_first_of("/\\") == std::string_view::npos;
}

} // namespace

std::vector<ThemeFolder> ThemeFolders::shipped() const {
    std::vector<ThemeFolder> themes;
    for (const std::filesystem::path& folder : foldersIn(shipped_)) {
        themes.push_back(ThemeFolder{folder.filename().string(), folder});
    }
    std::ranges::stable_partition(themes, [this](const ThemeFolder& theme) {
        return theme.id == default_;
    });
    return themes;
}

std::vector<ThemeFolder> ThemeFolders::user() const {
    std::vector<ThemeFolder> themes;
    for (const std::filesystem::path& folder : foldersIn(user_)) {
        themes.push_back(
            ThemeFolder{std::string{userThemePrefix} + folder.filename().string(), folder});
    }
    return themes;
}

std::optional<std::filesystem::path> ThemeFolders::folderOf(std::string_view id,
                                                            std::string& error) const {
    if (id.empty()) {
        id = default_;
    }
    if (id.starts_with(userThemePrefix)) {
        const std::string_view folder = id.substr(userThemePrefix.size());
        if (!plainName(folder)) {
            error = std::string{id} + " is not a theme folder name";
            return std::nullopt;
        }
        return user_ / folder;
    }
    if (!plainName(id)) {
        error = std::string{id} + " is not a theme";
        return std::nullopt;
    }
    return shipped_ / id;
}

} // namespace opensu::theme
