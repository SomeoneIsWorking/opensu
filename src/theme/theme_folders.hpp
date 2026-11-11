// theme_folders — where a kind of theme lives and how an id names its folder: the shipped folders
// under the assets (the default first), then the player's under the data directory as
// `user/<folder>`, each a folder holding a `theme.json`. A path in an id is refused. The console
// art themes and the interface themes both follow it.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace opensu::theme {

/// A player's theme's id is this and its folder's name.
inline constexpr std::string_view userThemePrefix = "user/";
/// The manifest's file name in a theme folder.
inline constexpr std::string_view manifestFile = "theme.json";

struct ThemeFolder {
    std::string id;
    std::filesystem::path folder;
};

class ThemeFolders {
  public:
    ThemeFolders(std::filesystem::path shipped, std::filesystem::path user,
                 std::string_view defaultId)
        : shipped_{std::move(shipped)}, user_{std::move(user)}, default_{defaultId} {
    }

    /// The shipped theme folders, the default first, then the rest by name.
    [[nodiscard]] std::vector<ThemeFolder> shipped() const;
    /// The player's theme folders, by name.
    [[nodiscard]] std::vector<ThemeFolder> user() const;

    /// The folder `id` names (empty for the default); nothing, with `error` saying why, for an id
    /// that names no theme folder.
    [[nodiscard]] std::optional<std::filesystem::path> folderOf(std::string_view id,
                                                                std::string& error) const;

    [[nodiscard]] const std::filesystem::path& shippedRoot() const noexcept {
        return shipped_;
    }
    [[nodiscard]] const std::filesystem::path& userRoot() const noexcept {
        return user_;
    }
    [[nodiscard]] const std::string& defaultId() const noexcept {
        return default_;
    }

  private:
    std::filesystem::path shipped_;
    std::filesystem::path user_;
    std::string default_;
};

} // namespace opensu::theme
