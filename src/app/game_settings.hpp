// game_settings — the setting layers as the shell holds them: loaded from `game-settings.json`
// (with the emulator picks an older `settings.json` kept moved in once), saved with a toast when
// the file cannot be written, and the words a value's layer is shown with.
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "library/emulator_catalog.hpp"
#include "settings/game_settings_store.hpp"
#include "settings/setting_layers.hpp"

namespace opensu::app {

/// What a game's value reads beside it for where it comes from: "From Super Nintendo", "From
/// Snes9x (built-in)", "Set for this game", "Set everywhere", or "Default" for none.
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

  private:
    settings::GameSettingsStore store_;
    std::function<void(const std::string&)> onFailure_;
    settings::SettingLayers layers_;
};

} // namespace opensu::app
