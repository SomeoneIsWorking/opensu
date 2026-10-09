// shortcut_editor — remapping a shortcut on the Settings screen: the player picks an action, then
// presses the new key combination (or, for the volume, holds L2, R2, L3 or R3 and presses another
// button). A combination another action has is refused with the reason and the editor keeps
// listening. What is chosen goes through `Preferences`, the one owner of the settings file.
#pragma once

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "gamepad/event.hpp"
#include "input/shortcuts.hpp"
#include "preferences.hpp"

namespace opensu::app {

class ShortcutEditor {
  public:
    /// Tells the player something; `error` marks a refusal.
    using Say = std::function<void(const std::string& text, bool error)>;

    /// `stopped` is told when it stops listening, whether it took a combination or not.
    ShortcutEditor(input::Shortcuts& shortcuts, Preferences& preferences, Say say,
                   std::function<void()> stopped)
        : shortcuts_{shortcuts}, preferences_{preferences}, say_{std::move(say)},
          stopped_{std::move(stopped)} {
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
    /// Pad events while listening: a held modifier then another button sets the action's chord.
    void capturePad(const std::vector<gamepad::Event>& events);

    /// Puts every shortcut back to the shipped one.
    void resetAll();

  private:
    void keep(const std::string& refused, const std::string& what);

    input::Shortcuts& shortcuts_;
    Preferences& preferences_;
    Say say_;
    std::function<void()> stopped_;
    std::optional<input::Action> target_;
    std::set<gamepad::Button> held_;
};

} // namespace opensu::app
