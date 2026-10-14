// plugin_settings — the Settings screen's Plugins category: each plugin with a switch, where it
// came from, its version, what it can read and reach, and how it is doing (its report, its last
// fault, or why it stopped or was refused). Turning on a plugin the player added first asks in
// the consent dialog, which lists its permissions in words.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "audio/sdl_sound_player.hpp"
#include "gamepad/event.hpp"
#include "plugin_service.hpp"
#include "ui/session_dialog.hpp"
#include "ui/settings_page.hpp"

namespace opensu::app {

class PluginSettings {
  public:
    /// After a choice made in the dialog: the screen shows it.
    using Changed = std::function<void()>;

    PluginSettings(PluginService& plugins, ui::SessionDialog& dialog, audio::SdlSoundPlayer& sounds,
                   std::filesystem::path home, Changed changed)
        : plugins_{plugins}, dialog_{dialog}, sounds_{sounds}, home_{std::move(home)},
          changed_{std::move(changed)} {
    }

    /// The category's id, which its rows' ids start with.
    static constexpr std::string_view categoryId = "plugins";

    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: A on a plugin's switch turns it off, on, or asks first.
    void run(const std::string& id, int step);

    /// Whether the consent dialog is up for a plugin, which is when it takes every button.
    [[nodiscard]] bool active() const noexcept {
        return asking_.has_value() && dialog_.isOpen();
    }
    /// A pad button while the dialog is up.
    void act(gamepad::Button button);

  private:
    void ask(const plugin::PluginInfo& info);
    void close();
    [[nodiscard]] std::string shown(const std::filesystem::path& path) const;

    PluginService& plugins_;
    ui::SessionDialog& dialog_;
    audio::SdlSoundPlayer& sounds_;
    std::filesystem::path home_;
    Changed changed_;
    /// The plugin the dialog asks about.
    std::optional<std::string> asking_;
};

} // namespace opensu::app
