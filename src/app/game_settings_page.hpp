// game_settings_page — the game settings tabs, from the quick menu over the running game or from
// its details page: Global, its console (the System layer; ROM games only) and Game. A game on a
// built-in core has Aspect, Integer scale, Sampling, Filter and Shader preset; every game has the
// Frame generation group where the platform has frame generation (`FrameGeneration::available`):
// Mode, then the values the mode uses, or only why it cannot be set when the player's
// `Lossless.dll` is not found. Global's are plain values.
// On the System and Game tabs a value is inherited until its Override is ticked, which copies what
// it inherited into that layer; unticking deletes the key there. A value opens its list of choices
// (the preset picker for the shader). Every change is written to the store at once and handed to
// `changed`, which applies it to the running game. The core layer is no tab: Settings › Emulators
// sets it, and here it shows only as where a value comes from.
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "audio/sdl_sound_player.hpp"
#include "emulator_downloads.hpp"
#include "frame_generation.hpp"
#include "game_settings.hpp"
#include "gamepad/event.hpp"
#include "library/game.hpp"
#include "settings/setting_layers.hpp"
#include "shader_preset_picker.hpp"
#include "ui/settings_tabs.hpp"

namespace opensu::app {

/// Whether the tabs have anything for `game`: video on a built-in core of `catalog`, or frame
/// generation where it can be set.
[[nodiscard]] bool offersGameSettings(const library::Game& game,
                                      const library::roms::EmulatorCatalog& catalog,
                                      bool frameGeneration);

class GameSettingsPage {
  public:
    struct Hooks {
        /// A value of `game`'s layers changed and was saved.
        std::function<void(const library::Game& game)> changed;
    };

    /// The tabs, in the order the strip shows them.
    enum class Tab : std::uint8_t { Global, System, Game };

    GameSettingsPage(ui::SettingsTabs& page, GameSettings& gameSettings,
                     const EmulatorDownloads& downloads, FrameGeneration& frameGeneration,
                     audio::SdlSoundPlayer& sounds, Hooks hooks)
        : page_{page}, gameSettings_{gameSettings}, downloads_{downloads},
          frameGeneration_{frameGeneration}, picker_{downloads.shaderPack()}, sounds_{sounds},
          hooks_{std::move(hooks)} {
    }

    /// Whether `game` has anything to set here.
    [[nodiscard]] bool offers(const library::Game& game) const;

    /// Opens on `game`'s Game tab; `back` runs once B has closed the page.
    void open(const library::Game& game, std::function<void()> back);
    /// Closes the page without running `back`.
    void close() noexcept;
    [[nodiscard]] bool isOpen() const noexcept {
        return page_.isOpen();
    }
    /// A button while the page is up: it takes them all.
    void act(gamepad::Button button);
    /// Shows tab `index` of the strip.
    void showTab(std::size_t index);
    /// The tab on show, as the strip names it; empty while closed.
    [[nodiscard]] std::string tabName() const;
    /// The focused row and where in it: `<row id>:override` or `<row id>:value`; empty while
    /// closed.
    [[nodiscard]] std::string focusName() const;

  private:
    [[nodiscard]] settings::Layer layerOf(Tab tab) const;
    [[nodiscard]] std::vector<Tab> tabs() const;
    /// The strip's name for `tab`: "Global", the console's, "Game".
    [[nodiscard]] std::string labelOf(Tab tab) const;
    [[nodiscard]] ui::TabsView view() const;
    [[nodiscard]] std::vector<ui::TabRow> tabRows() const;
    /// The Frame generation group: its heading, then Mode and the values the mode uses.
    void addFrameGenRows(std::vector<ui::TabRow>& rows) const;
    [[nodiscard]] std::vector<ui::TabRow> listRows() const;
    /// The row of one setting: `key`, its value as `label` reads it, its default.
    [[nodiscard]] ui::TabRow
    settingRow(const std::string& id, std::string_view label, std::string_view key,
               const std::string& fallback,
               const std::function<std::string(const std::string&)>& shownAs) const;
    /// What the tab's layer has for `key`, its own or what it inherits, else `fallback`.
    [[nodiscard]] std::string valueAt(const settings::Layer& layer, std::string_view key,
                                      const std::string& fallback) const;
    void press();
    void toggleOverride(std::string_view key, const std::string& fallback);
    void pick(const std::string& id);
    /// The store changed: saved, shown and applied.
    void changed();
    void show();

    ui::SettingsTabs& page_;
    GameSettings& gameSettings_;
    const EmulatorDownloads& downloads_;
    FrameGeneration& frameGeneration_;
    ShaderPresetPicker picker_;
    audio::SdlSoundPlayer& sounds_;
    Hooks hooks_;
    std::optional<library::Game> game_;
    settings::LayerKeys keys_;
    std::function<void()> back_;
    Tab tab_{Tab::Game};
    /// The game runs on a built-in core, so it has the video rows.
    bool video_{false};
    /// The key whose choices are listed; empty when none is.
    std::string listing_;
};

} // namespace opensu::app
