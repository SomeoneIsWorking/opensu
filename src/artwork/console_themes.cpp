#include "console_themes.hpp"

#include <algorithm>
#include <system_error>

namespace opensu::artwork {
namespace {

/// The theme folders in `root` with a manifest, by name.
std::vector<std::filesystem::path> foldersIn(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> folders;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator{root, ec}) {
        if (entry.is_directory(ec) &&
            std::filesystem::is_regular_file(entry.path() / "theme.json", ec)) {
            folders.push_back(entry.path());
        }
    }
    std::ranges::sort(folders);
    return folders;
}

} // namespace

std::vector<ThemeInfo> ConsoleThemes::list() const {
    std::vector<ThemeInfo> themes;
    std::string error;
    std::vector<std::filesystem::path> shipped = foldersIn(shipped_);
    // The default leads.
    std::ranges::stable_partition(shipped, [](const std::filesystem::path& folder) {
        return folder.filename() == defaultConsoleTheme;
    });
    for (const std::filesystem::path& folder : shipped) {
        if (const auto theme = openFolder(folder.filename().string(), folder, error)) {
            themes.push_back(theme->info());
        }
    }
    if (const std::shared_ptr<const IisuPack> pack = iisu_.pack()) {
        themes.push_back(ThemeInfo{.id = std::string{iisuConsoleTheme},
                                   .name = "iiSU",
                                   .note = "iiSU's, from " + pack->file().filename().string()});
    }
    for (const std::filesystem::path& folder : foldersIn(user_)) {
        if (const auto theme = openFolder(std::string{userThemePrefix} + folder.filename().string(),
                                          folder, error)) {
            themes.push_back(theme->info());
        }
    }
    return themes;
}

std::shared_ptr<const ConsoleTheme> ConsoleThemes::open(std::string_view id,
                                                        std::string& error) const {
    if (id.empty()) {
        id = defaultConsoleTheme;
    }
    if (id == iisuConsoleTheme) {
        std::shared_ptr<const IisuPack> pack = iisu_.pack();
        if (pack == nullptr) {
            error = "no iiSU release or starter pack in " + iisu_.folder().string();
            return nullptr;
        }
        return std::make_shared<const IisuTheme>(std::move(pack));
    }
    if (id.starts_with(userThemePrefix)) {
        const std::string_view folder = id.substr(userThemePrefix.size());
        if (folder.empty() || folder.find('/') != std::string_view::npos || folder == "..") {
            error = std::string{id} + " is not a theme folder name";
            return nullptr;
        }
        return openFolder(std::string{id}, user_ / folder, error);
    }
    if (id.find('/') != std::string_view::npos || id == "..") {
        error = std::string{id} + " is not a theme";
        return nullptr;
    }
    return openFolder(std::string{id}, shipped_ / id, error);
}

std::shared_ptr<const ConsoleTheme> ConsoleThemes::openFolder(std::string id,
                                                              const std::filesystem::path& folder,
                                                              std::string& error) const {
    // A theme may use a shipped theme's glyphs; its cards are always its own.
    const std::string glyphs = FolderTheme::glyphsFrom(folder);
    const bool shared = !glyphs.empty() && glyphs.find('/') == std::string::npos && glyphs != "..";
    return FolderTheme::open(std::move(id), folder,
                             shared ? shipped_ / glyphs / "glyphs" : folder / "glyphs", error);
}

} // namespace opensu::artwork
