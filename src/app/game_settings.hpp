// game_settings — the setting layers as the shell holds them: loaded from `game-settings.json`
// (with the emulator picks an older `settings.json` kept and an older frame generation mode moved
// in once, then every value checked against the schema of what may be kept), saved with a toast
// when the file cannot be written, a layer cleared of everything the schema owns, and the words a
// layer and a value's source are shown with.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "library/emulator_catalog.hpp"
#include "settings/game_settings_store.hpp"
#include "settings/setting_layers.hpp"
#include "settings/setting_schema.hpp"

namespace opensu::app {

/// A layer as the player reads it: "Global", "Super Nintendo", "Snes9x", "This game".
[[nodiscard]] std::string layerName(const settings::Layer& layer,
                                    const library::roms::EmulatorCatalog& catalog);
/// What a game's value reads beside it for where it comes from: "From Super Nintendo", "From
/// Snes9x", "From Global", "Set for this game", or "Default" for none.
[[nodiscard]] std::string sourceText(const std::optional<settings::Layer>& from,
                                     const library::roms::EmulatorCatalog& catalog);

class GameSettings {
  public:
    /// Loads `store`, first moving in the emulator picks `settingsFile` keeps. `onFailure` is told
    /// why when a later save cannot be written.
    GameSettings(settings::GameSettingsStore store, const std::filesystem::path& settingsFile,
                 std::function<void(const std::string&)> onFailure);

    [[nodiscard]] settings::SettingLayers& layers() noexcept {
        return layers_;
    }
    [[nodiscard]] const settings::SettingLayers& layers() const noexcept {
        return layers_;
    }

    /// Writes the layers; on failure logs it and reports it, and keeps the old file.
    void save();
    /// Deletes every setting `layer` keeps and saves; how many went.
    std::size_t clear(const settings::Layer& layer);

  private:
    /// Every key the layers may keep, whatever the context: the video and frame generation
    /// settings, the emulator pick and any core's options.
    settings::Schema kept_;
    settings::GameSettingsStore store_;
    std::function<void(const std::string&)> onFailure_;
    settings::SettingLayers layers_;
};

} // namespace opensu::app
