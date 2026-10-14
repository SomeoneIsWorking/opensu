// mapping_editor — the button mappings: kept in `mappings.json` (`settings::MappingStore`),
// resolved for a game as it launches (`forGame`), and edited on the mapping page for one scope at a
// time: a game's own (from its details page), an emulator's or the global one (from Settings ›
// Controls, which also holds the global Steam Input switch). A row's output is picked from a list;
// a chord is added by holding a button and pressing another (`ChordCapture`), then picking what it
// does. While Steam Input has a game, its mapping is refused and the page says why.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "audio/sdl_sound_player.hpp"
#include "chord_capture.hpp"
#include "gamepad/event.hpp"
#include "input/button_map.hpp"
#include "input/mapping_scopes.hpp"
#include "library/game.hpp"
#include "settings/mapping_store.hpp"
#include "ui/mapping_page.hpp"
#include "ui/settings_page.hpp"

namespace opensu::app {

/// The keys `game`'s mapping is found by: its id and the catalog id of the emulator it runs on.
[[nodiscard]] input::GameKeys mappingKeys(const library::Game& game);

class MappingEditor {
  public:
    /// An emulator a mapping can be kept for.
    struct Emulator {
        std::string id;
        std::string name;
    };

    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The mappings changed and were saved.
        std::function<void()> changed;
        /// The catalog's emulators, for Settings › Controls.
        std::function<std::vector<Emulator>()> emulators;
    };

    /// `shell` is the shortcut table, whose chords a game chord may not take.
    MappingEditor(ui::MappingPage& page, settings::MappingStore store,
                  audio::SdlSoundPlayer& sounds, const input::Shortcuts& shell, Hooks hooks);

    [[nodiscard]] const input::MappingScopes& scopes() const noexcept {
        return scopes_;
    }
    /// The mapping `game` plays with: none while Steam Input has it.
    [[nodiscard]] input::ButtonMap forGame(const library::Game& game) const {
        return scopes_.resolve(mappingKeys(game));
    }

    /// Opens the page on `game`'s own mapping.
    void openGame(const library::Game& game);
    /// Opens the page on emulator `id`'s mapping.
    void openEmulator(const std::string& id, const std::string& name);
    /// Opens the page on the global mapping.
    void openGlobal();
    void close();
    [[nodiscard]] bool active() const noexcept {
        return page_.isOpen();
    }
    /// Whether the page is listening for a chord, which takes the pad's events whole.
    [[nodiscard]] bool capturing() const noexcept {
        return page_.isOpen() && page_.mode() == ui::MappingMode::Listening;
    }

    /// A button while the page is up and not listening.
    void act(gamepad::Button button);
    /// Pad events while listening: the chord, or B alone to cancel.
    void capturePad(const std::vector<gamepad::Event>& events);

    /// Settings › Controls' rows: the global mapping, an emulator's, and the global Steam Input
    /// switch.
    [[nodiscard]] std::vector<ui::SettingsRow> settingsRows() const;
    /// Whether the Settings row with `id` is one of these.
    [[nodiscard]] static bool owns(std::string_view id);
    /// Acts on Settings row `id`: `step` is 0 for A and -1 or 1 for Left and Right.
    void runSetting(const std::string& id, int step);

  private:
    /// What the page edits.
    struct Target {
        input::MappingScope scope;
        std::string title;
        /// For a game's: the keys its inherited mapping is found by.
        input::GameKeys keys;
        /// A Steam game, whose pads Steam may read itself.
        bool steamGame{false};
    };
    /// One entry of an output list.
    struct Remove {
        bool operator==(const Remove&) const = default;
    };
    using Choice = std::variant<input::PadOutput, input::Action, Remove>;

    void open(Target target);
    /// The mapping the page shows: the scope's own, else the one it inherits.
    [[nodiscard]] input::ButtonMap shown() const;
    /// Where the shown mapping comes from, or why it cannot change.
    [[nodiscard]] std::string note() const;
    [[nodiscard]] std::vector<ui::MappingRow> rows() const;
    /// Whether Steam Input has the game the page is on.
    [[nodiscard]] bool steamInputHasIt() const;
    void show();
    /// Acts on the focused row.
    void runRow(const ui::MappingRow& row);
    /// Puts up what `button` can send.
    void pickButton(gamepad::Button button);
    /// Puts up what `chord` can do, with Remove when the mapping has it.
    void pickChord(const input::PadChord& chord);
    /// Puts up `choices` under `title`, focused on `current` where it is one.
    void pick(std::string title, std::vector<Choice> choices, const std::optional<Choice>& current);
    /// Acts on the focused choice of the list up.
    void choose();
    /// Applies `change` to the scope's mapping, made from the shown one if it had none, and keeps
    /// it; says why when it is refused.
    void edit(const std::function<std::string(input::ButtonMap&)>& change);
    void save();

    ui::MappingPage& page_;
    settings::MappingStore store_;
    audio::SdlSoundPlayer& sounds_;
    const input::Shortcuts& shell_;
    Hooks hooks_;
    input::MappingScopes scopes_;
    /// What the page edits while it is open.
    Target target_;
    ChordCapture capture_;
    /// What the list up picks for: a button, a chord, or the game's Steam Input.
    std::variant<std::monostate, gamepad::Button, input::PadChord> picking_;
    bool pickingSteam_{false};
    std::vector<Choice> choices_;
    /// The emulator Settings › Controls has picked.
    std::size_t emulator_{0};
};

} // namespace opensu::app
