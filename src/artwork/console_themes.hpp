// console_themes — the console art themes the player can choose: the folders shipped under
// `<assets>/consoles/` (Photos, the default, and Drawings), iiSU's when its file is placed in
// `<data dir>/themes/iiSU/` (`IisuInstall`), and every folder under `<data dir>/themes/` with a
// `theme.json`.
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "console_theme.hpp"
#include "iisu_install.hpp"
#include "theme/theme_folders.hpp"

namespace opensu::artwork {

/// The theme shown when the player chose none.
inline constexpr std::string_view defaultConsoleTheme = "photos";
/// iiSU's theme, read from the iiSU file placed in the themes folder.
inline constexpr std::string_view iisuConsoleTheme = "iisu";

class ConsoleThemes {
  public:
    ConsoleThemes(std::filesystem::path shipped, std::filesystem::path user)
        : folders_{std::move(shipped), std::move(user), defaultConsoleTheme},
          iisu_{folders_.userRoot() / std::filesystem::path{iisuFolderName}} {
    }

    /// Every theme there is now: the shipped ones (the default first), iiSU's when its file is
    /// placed, the player's own.
    [[nodiscard]] std::vector<ThemeInfo> list() const;

    /// The theme `id` (empty for the default). Nothing, with `error` saying why, when it cannot be
    /// opened.
    [[nodiscard]] std::shared_ptr<const ConsoleTheme> open(std::string_view id,
                                                           std::string& error) const;

    /// Where the player's own themes go.
    [[nodiscard]] const std::filesystem::path& userFolder() const noexcept {
        return folders_.userRoot();
    }
    /// iiSU's file placed in the themes folder, which the interface sounds read too.
    [[nodiscard]] const IisuInstall& iisu() const noexcept {
        return iisu_;
    }

  private:
    [[nodiscard]] std::shared_ptr<const ConsoleTheme>
    openFolder(std::string id, const std::filesystem::path& folder, std::string& error) const;

    theme::ThemeFolders folders_;
    IisuInstall iisu_;
};

} // namespace opensu::artwork
