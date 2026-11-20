// shortcut_chord — key edges turned into the shortcut combinations they complete, for a watcher
// that sees a game's keys rather than the shell's window's: XInput2's raw keys inside Gamescope
// (`session::GameKeys`) and the keys forwarded to a game embedded on macOS
// (`game_overlay_layer_host.mm`).
#pragma once

#include <optional>
#include <set>

#include "key_names.hpp"

namespace opensu::input {

/// Turns key edges into key combinations. A held key's repeats fire once, and a modifier alone
/// fires nothing.
class ShortcutChord {
  public:
    /// `key` is a key code; the combination it completes when it goes down.
    [[nodiscard]] std::optional<Combo> key(int key, bool pressed);

  private:
    bool ctrl_{};
    bool shift_{};
    std::set<int> down_;
};

} // namespace opensu::input
