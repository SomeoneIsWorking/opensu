#include "console_themes.hpp"

namespace opensu::artwork {

std::vector<ThemeInfo> ConsoleThemes::list() const {
    std::vector<ThemeInfo> themes;
    std::string error;
    for (const theme::ThemeFolder& shipped : folders_.shipped()) {
        if (const auto theme = openFolder(shipped.id, shipped.folder, error)) {
            themes.push_back(theme->info());
        }
    }
    if (const std::shared_ptr<const IisuPack> pack = iisu_.pack()) {
        themes.push_back(ThemeInfo{.id = std::string{iisuConsoleTheme},
                                   .name = "iiSU",
                                   .note = "iiSU's, from " + pack->file().filename().string()});
    }
    for (const theme::ThemeFolder& own : folders_.user()) {
        if (const auto theme = openFolder(own.id, own.folder, error)) {
            themes.push_back(theme->info());
        }
    }
    return themes;
}

std::shared_ptr<const ConsoleTheme> ConsoleThemes::open(std::string_view id,
                                                        std::string& error) const {
    if (id == iisuConsoleTheme) {
        std::shared_ptr<const IisuPack> pack = iisu_.pack();
        if (pack == nullptr) {
            error = "no iiSU release or starter pack in " + iisu_.folder().string();
            return nullptr;
        }
        return std::make_shared<const IisuTheme>(std::move(pack));
    }
    const std::optional<std::filesystem::path> folder = folders_.folderOf(id, error);
    if (!folder) {
        return nullptr;
    }
    return openFolder(std::string{id.empty() ? std::string_view{folders_.defaultId()} : id},
                      *folder, error);
}

std::shared_ptr<const ConsoleTheme> ConsoleThemes::openFolder(std::string id,
                                                              const std::filesystem::path& folder,
                                                              std::string& error) const {
    // A theme may use a shipped theme's glyphs; its cards are always its own.
    const std::string glyphs = FolderTheme::glyphsFrom(folder);
    std::string unshared;
    const std::optional<std::filesystem::path> shared =
        glyphs.empty() || glyphs.starts_with(theme::userThemePrefix)
            ? std::nullopt
            : folders_.folderOf(glyphs, unshared);
    return FolderTheme::open(std::move(id), folder, shared ? *shared / "glyphs" : folder / "glyphs",
                             error);
}

} // namespace opensu::artwork
