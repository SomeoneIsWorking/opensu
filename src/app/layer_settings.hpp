// layer_settings — Settings › Emulators' rows for the setting layers: one opening Global's
// settings, one each opening the list of consoles and of built-in cores, whose rows open each
// one's. Every row reads how many settings its layer keeps; each opens the layered editor
// (`LayeredEditor`) on that scope. Every game's own emulator pick is reset here.
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game_settings.hpp"
#include "layered_settings.hpp"
#include "library/emulators.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class LayerSettings {
  public:
    /// `found` is what the emulator search found, read as the page is built.
    LayerSettings(GameSettings& gameSettings,
                  std::function<const library::roms::Emulators&()> found, LayeredSettings& layered)
        : gameSettings_{gameSettings}, found_{std::move(found)}, layered_{layered} {
    }

    /// The Emulators page's rows: Global's, and the lists of consoles and of cores.
    [[nodiscard]] std::vector<ui::SettingsRow> pageRows() const;
    /// The list of consoles or of cores `id` opens, or nothing for another id.
    [[nodiscard]] std::optional<ui::SettingsCategory> listPage(std::string_view id) const;
    /// The scope row `id` opens the editor on, or nothing for another row.
    [[nodiscard]] std::optional<EditorContext> contextOf(std::string_view id) const;
    [[nodiscard]] static bool owns(const std::string& id);

    /// How many games have an emulator pick of their own.
    [[nodiscard]] std::size_t gamePicks() const;
    /// Forgets every game's own pick; the systems' stay.
    void clearGamePicks();

  private:
    [[nodiscard]] std::string keptText(const EditorContext& context) const;

    GameSettings& gameSettings_;
    std::function<const library::roms::Emulators&()> found_;
    LayeredSettings& layered_;
};

} // namespace opensu::app
