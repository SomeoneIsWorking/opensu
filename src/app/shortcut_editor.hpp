// shortcut_editor — remapping a shortcut on the Settings screen: the player picks an action, then
// presses the new key combination (or, for an action that takes a pad chord, holds any button and
// presses another). A combination another action has is refused with the reason and the editor
// keeps listening. What is chosen goes through `Preferences`, the one owner of the settings file.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gamepad/event.hpp"
#include "input/shortcuts.hpp"
#include "preferences.hpp"

namespace opensu::app {

class ShortcutEditor {
  public:
    /// The note a row shows while it listens for a chord.
    static constexpr std::string_view listeningNote =
        "Any button can be held; Esc or B alone cancels";

    /// Tells the player something; `error` marks a refusal.
    using Say = std::function<void(const std::string& text, bool error)>;

    /// `stopped` is told when it stops listening, whether it took a combination or not;
    /// `changed` after the table changed and was saved.
    ShortcutEditor(input::Shortcuts& shortcuts, Preferences& preferences, Say say,
                   std::function<void()> stopped, std::function<void()> changed)
        : shortcuts_{shortcuts}, preferences_{preferences}, say_{std::move(say)},
          stopped_{std::move(stopped)}, changed_{std::move(changed)} {
    }

    /// Listens for the new combination of `action`.
    void begin(input::Action action);
    [[nodiscard]] bool capturing() const noexcept {
        return target_.has_value();
    }
    [[nodiscard]] std::optional<input::Action> target() const noexcept {
        return target_;
    }
    /// Stops listening without a change.
    void cancel();

    /// A key combination; takes it if the table allows, else says why not and keeps listening.
    void captureKey(const input::Combo& combo);
    /// Pad events while listening: the first button held then another sets the action's chord; B
    /// pressed and released alone cancels.
    void capturePad(const std::vector<gamepad::Event>& events);

    /// Puts every shortcut back to the shipped one.
    void resetAll();
    /// Puts every pad chord back to the shipped one, keys as they are.
    void resetChords();

  private:
    void keep(const std::string& refused, const std::string& what);
    /// Saves the table and tells `changed`.
    void save();

    input::Shortcuts& shortcuts_;
    Preferences& preferences_;
    Say say_;
    std::function<void()> stopped_;
    std::function<void()> changed_;
    std::optional<input::Action> target_;
    /// The buttons held while listening, in the order they went down; the first is a chord's
    /// modifier.
    std::vector<gamepad::Button> held_;
    /// B went down with nothing held: its release alone cancels.
    bool loneB_{false};
};

} // namespace opensu::app
