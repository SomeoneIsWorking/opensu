// game_settings_store — the setting layers (`SettingLayers`) in `game-settings.json` under the
// config directory: each layer's own keys only, written whole and atomically; a damaged file or
// entry is reported and left out.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "setting_layers.hpp"

namespace opensu::settings {

/// The file's name in the config directory.
inline constexpr std::string_view gameSettingsFile = "game-settings.json";

class GameSettingsStore {
  public:
    explicit GameSettingsStore(std::filesystem::path file) : file_{std::move(file)} {
    }

    /// The saved layers. A missing file gives none; an unreadable or unparsable file, or a
    /// value that is not text, is reported and left out.
    [[nodiscard]] SettingLayers load() const;

    /// Replaces the file whole. False with `error` when it cannot; the old file is then kept.
    [[nodiscard]] bool save(const SettingLayers& layers, std::string& error) const;

    [[nodiscard]] const std::filesystem::path& file() const noexcept {
        return file_;
    }

  private:
    std::filesystem::path file_;
};

} // namespace opensu::settings
