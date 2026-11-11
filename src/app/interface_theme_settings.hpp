// interface_theme_settings — the one owner of the interface theme in force: Settings › Appearance ›
// Interface theme steps through the themes there are (`theme::InterfaceThemes`), and `current`
// is the chosen one opened, or the default when it does not open. Choosing a theme gives back
// what it sets to the theme: the scroll mode and the interface sounds the player had picked.
#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "config/config.hpp"
#include "preferences.hpp"
#include "theme/interface_themes.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class InterfaceThemeSettings {
  public:
    /// The shipped themes are read from `shipped`, the player's from `user`; `changed` runs when
    /// the choice changed.
    InterfaceThemeSettings(Preferences& preferences, std::filesystem::path shipped,
                           std::filesystem::path user, std::function<void()> changed)
        : preferences_{preferences}, themes_{std::move(shipped), std::move(user)},
          changed_{std::move(changed)} {
    }

    /// The Appearance page's row.
    [[nodiscard]] ui::SettingsRow row() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Steps through the themes, wrapping; A steps forward.
    void run(const std::string& id, int step);

    /// The theme in force, opened again only when the choice changed.
    [[nodiscard]] std::shared_ptr<const theme::InterfaceTheme> current();

    /// How the home grid scrolls: the player's choice, else `environment`'s, else the theme's.
    [[nodiscard]] config::HomeMode homeMode(std::optional<config::HomeMode> environment);

  private:
    Preferences& preferences_;
    theme::InterfaceThemes themes_;
    std::function<void()> changed_;
    /// The choice `current_` was opened for.
    std::string opened_;
    std::shared_ptr<const theme::InterfaceTheme> current_;
};

} // namespace opensu::app
