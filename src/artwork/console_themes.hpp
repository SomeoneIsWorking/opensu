// console_themes — the console art themes the player can choose: the folders shipped under
// `<assets>/consoles/` (Photos, the default, and Drawings), iiSU's from the file the player
// chose, and every folder under `<data dir>/themes/` with a `theme.json`.
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "console_theme.hpp"

namespace opensu::artwork {

/// The theme shown when the player chose none.
inline constexpr std::string_view defaultConsoleTheme = "photos";
/// iiSU's theme, read from the player's iiSU file.
inline constexpr std::string_view iisuConsoleTheme = "iisu";
/// A player's own theme's id is this and its folder's name.
inline constexpr std::string_view userThemePrefix = "user/";

class ConsoleThemes {
  public:
    ConsoleThemes(std::filesystem::path shipped, std::filesystem::path user)
        : shipped_{std::move(shipped)}, user_{std::move(user)} {
    }

    /// Every theme there is now: the shipped ones (the default first), iiSU's, the player's own.
    [[nodiscard]] std::vector<ThemeInfo> list() const;

    /// The theme `id` (empty for the default), with `iisuFile` for iiSU's. Nothing, with `error`
    /// saying why, when it cannot be opened.
    [[nodiscard]] std::shared_ptr<const ConsoleTheme>
    open(std::string_view id, const std::filesystem::path& iisuFile, std::string& error) const;

    /// Where the player's own themes go.
    [[nodiscard]] const std::filesystem::path& userFolder() const noexcept {
        return user_;
    }

  private:
    [[nodiscard]] std::shared_ptr<const ConsoleTheme>
    openFolder(std::string id, const std::filesystem::path& folder, std::string& error) const;

    std::filesystem::path shipped_;
    std::filesystem::path user_;
};

} // namespace opensu::artwork
