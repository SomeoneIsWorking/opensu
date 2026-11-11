// interface_themes — the interface themes there are: the folders shipped under
// `<assets>/interface-themes/` (iiSU's look, the default, first) and each folder of the player's
// in `<data dir>/interface-themes/`, by the rule every kind of theme follows (`ThemeFolders`).
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "interface_theme.hpp"
#include "theme_folders.hpp"

namespace opensu::theme {

/// The theme the shell shows when the player chose none.
inline constexpr std::string_view defaultInterfaceTheme = "iisu";

class InterfaceThemes {
  public:
    InterfaceThemes(std::filesystem::path shipped, std::filesystem::path user)
        : folders_{std::move(shipped), std::move(user), defaultInterfaceTheme} {
    }

    /// Every theme that opens now: the shipped ones, the default first, then the player's; a
    /// folder that breaks the format is left out.
    [[nodiscard]] std::vector<std::shared_ptr<const InterfaceTheme>> list() const;

    /// The theme `id` (empty for the default); nothing, with `error` saying why, when it does not
    /// open.
    [[nodiscard]] std::shared_ptr<const InterfaceTheme> open(std::string_view id,
                                                             std::string& error) const;

    /// The theme `id`, or the default with the refusal logged when it does not open; the default
    /// built from the tokens' own defaults when even that one is gone.
    [[nodiscard]] std::shared_ptr<const InterfaceTheme> openOrDefault(std::string_view id) const;

    /// Where the player's own themes go.
    [[nodiscard]] const std::filesystem::path& userFolder() const noexcept {
        return folders_.userRoot();
    }

  private:
    ThemeFolders folders_;
};

} // namespace opensu::theme
