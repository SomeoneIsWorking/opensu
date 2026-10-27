// layer_settings — Settings › Emulators' pages for the setting layers: Global, each console (its
// emulator and its video settings) and each built-in core (its video settings). The Emulators page
// has a row opening Global's page and one each opening the list of consoles and of cores, whose
// rows open each layer's page. A value set on the page reads "Set for <layer>", one inherited
// reads where it comes from, and each choice has "not set" as its first option, which clears it. The Shader preset row opens the preset picker
// (`ShaderPresetPicker`) in the page's place. The core layer is set only here; a game's own values
// and its console's are also on the game settings tabs (`GameSettingsPage`). Every game's own
// emulator pick is reset here.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game_settings.hpp"
#include "library/emulators.hpp"
#include "settings/setting_layers.hpp"
#include "settings/video_settings.hpp"
#include "shader_preset_picker.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class LayerSettings {
  public:
    struct Hooks {
        /// An emulator pick changed: the library applies it.
        std::function<void()> picksChanged;
        /// A video setting changed: the details page shows it.
        std::function<void()> videoChanged;
    };

    /// What B did on an open page.
    enum class Back : std::uint8_t {
        /// Moved back within the picker, or from it to its layer's page.
        Within,
        /// The layer's page is to close, back to the Emulators page.
        Layer,
    };

    /// `found` is what the emulator search found, read as the page is built; `pack` the shader
    /// presets the picker lists.
    LayerSettings(GameSettings& gameSettings,
                  std::function<const library::roms::Emulators&()> found,
                  const library::roms::ShaderPack& pack, Hooks hooks)
        : gameSettings_{gameSettings}, found_{std::move(found)}, picker_{pack},
          hooks_{std::move(hooks)} {
    }

    /// The Emulators page's rows: Global's page, and the lists of consoles and of cores.
    [[nodiscard]] std::vector<ui::SettingsRow> pageRows() const;
    /// The list of consoles or of cores `id` opens, or nothing for another id.
    [[nodiscard]] std::optional<ui::SettingsCategory> listPage(std::string_view id) const;
    /// The open layer's page, its id the row that opened it; nothing when none is open.
    [[nodiscard]] std::optional<ui::SettingsCategory> page() const;
    /// The open layer's name, as the breadcrumb reads it after the category; empty for none.
    [[nodiscard]] std::string openName() const;
    [[nodiscard]] bool isOpen() const noexcept {
        return open_.has_value();
    }
    void close() noexcept {
        open_.reset();
        picker_.close();
    }
    /// B on the open page.
    [[nodiscard]] Back back();
    /// The row to focus after the page changed: the picker's row just left; empty for the first.
    [[nodiscard]] std::string returnRow() const;
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
    /// The layer's name: "Global", the console's or the core's.
    [[nodiscard]] std::string nameOf(const settings::Layer& layer) const;
    [[nodiscard]] ui::SettingsRow emulatorRow(const std::string& system) const;
    [[nodiscard]] ui::SettingsRow videoRow(const settings::Layer& layer,
                                           const settings::ChoiceKey& video) const;
    /// The row before the note that a shader preset replaces it.
    [[nodiscard]] ui::SettingsRow videoRowOf(const settings::Layer& layer,
                                             const settings::ChoiceKey& video) const;
    void stepEmulator(const std::string& system, int step);
    void stepVideo(const settings::Layer& layer, std::string_view key, int step);
    [[nodiscard]] ui::SettingsRow shaderRow(const settings::Layer& layer) const;
    /// The preset that applies at `layer` without its own value: Global's under a console.
    [[nodiscard]] std::string inheritedShader(const settings::Layer& layer) const;
    void pressPicker(const settings::Layer& layer, const std::string& id);
    void saveVideo();
    void clearLayer(const settings::Layer& layer);

    GameSettings& gameSettings_;
    std::function<const library::roms::Emulators&()> found_;
    ShaderPresetPicker picker_;
    Hooks hooks_;
    std::optional<settings::Layer> open_;
    /// The picker was just left for its layer's page.
    bool leftPicker_{false};
};

} // namespace opensu::app
