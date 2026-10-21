// layer_settings — Settings › Emulators' pages for the setting layers: Everywhere, each console
// (its emulator and its video settings) and each built-in core (its video settings). The Emulators
// page lists a row opening each; an open page takes the category's place until B closes it. A
// value set on the page reads "Set for <layer>", one inherited reads where it comes from, and each
// choice steps through "not set" too, which clears it. Every game's own emulator pick is reset
// here.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game_settings.hpp"
#include "library/emulators.hpp"
#include "settings/setting_layers.hpp"
#include "settings/video_settings.hpp"
#include "ui/settings_page.hpp"

namespace opensu::app {

class LayerSettings {
  public:
    struct Hooks {
        /// An emulator pick changed: the library applies it.
        std::function<void()> picksChanged;
    };

    /// `found` is what the emulator search found, read as the page is built.
    LayerSettings(GameSettings& gameSettings,
                  std::function<const library::roms::Emulators&()> found, Hooks hooks)
        : gameSettings_{gameSettings}, found_{std::move(found)}, hooks_{std::move(hooks)} {
    }

    /// The rows the Emulators page opens each layer's page with.
    [[nodiscard]] std::vector<ui::SettingsRow> openRows() const;
    /// The open layer's page in the place of category `id` labelled `label`, or nothing when
    /// none is open.
    [[nodiscard]] std::optional<ui::SettingsCategory> page(const std::string& id,
                                                           const std::string& label) const;
    /// The open layer's name, as the breadcrumb reads it after the category; empty for none.
    [[nodiscard]] std::string openName() const;
    [[nodiscard]] bool isOpen() const noexcept {
        return open_.has_value();
    }
    void close() noexcept {
        open_.reset();
    }
    /// The Emulators page's row that opens the open layer's page.
    [[nodiscard]] std::string openRowId() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: `step` is 0 for A and -1 or 1 for Left and Right.
    void run(const std::string& id, int step);

    /// How many games have an emulator pick of their own.
    [[nodiscard]] std::size_t gamePicks() const;
    /// Forgets every game's own pick; the systems' stay.
    void clearGamePicks();

  private:
    /// The layer's name: "Everywhere", the console's or the core's.
    [[nodiscard]] std::string nameOf(const settings::Layer& layer) const;
    [[nodiscard]] ui::SettingsRow emulatorRow(const std::string& system) const;
    [[nodiscard]] ui::SettingsRow videoRow(const settings::Layer& layer,
                                           const settings::VideoKey& video) const;
    void stepEmulator(const std::string& system, int step);
    void stepVideo(const settings::Layer& layer, std::string_view key, int step);
    void clearLayer(const settings::Layer& layer);

    GameSettings& gameSettings_;
    std::function<const library::roms::Emulators&()> found_;
    Hooks hooks_;
    std::optional<settings::Layer> open_;
};

} // namespace opensu::app
