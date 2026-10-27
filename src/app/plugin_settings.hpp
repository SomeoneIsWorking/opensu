// plugin_settings — the Settings screen's Plugins category: each plugin with a switch, where it
// came from, its version, what it can read and reach, and how it is doing (its report, its last
// fault, or why it stopped or was refused). Turning on a plugin the player added first asks, the
// question listing its permissions in words.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "plugin_service.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class PluginSettings {
  public:
    /// After a plugin was turned on from the question: the screen shows it.
    using Changed = std::function<void()>;

    /// The question goes on `navigator`.
    PluginSettings(PluginService& plugins, gpu2d::ui::Navigator& navigator,
                   audio::SdlSoundPlayer& sounds, std::filesystem::path home, Changed changed)
        : plugins_{plugins}, navigator_{navigator}, sounds_{sounds}, home_{std::move(home)},
          changed_{std::move(changed)} {
    }

    /// The category's id, which its rows' ids start with.
    static constexpr std::string_view categoryId = "plugins";

    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: A on a plugin's switch turns it off, on, or asks first.
    void run(const std::string& id, int step);

  private:
    void ask(const plugin::PluginInfo& info);

    PluginService& plugins_;
    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
    std::filesystem::path home_;
    Changed changed_;
};

} // namespace opensu::app
