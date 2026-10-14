// chord_settings — the Settings screen's Guide chords category: one row per action a pad chord
// can perform, reading its chord, set with `ShortcutEditor`'s capture (hold Guide, or any other
// button, and press a button), and a row that puts every chord back as shipped. The Controls
// category's flat list still sets keys and chords alike.
#pragma once

#include <string>
#include <string_view>

#include "input/shortcuts.hpp"
#include "shortcut_editor.hpp"
#include "ui/settings_page.hpp"

namespace opensu::app {

class ChordSettings {
  public:
    ChordSettings(ShortcutEditor& editor, const input::Shortcuts& shortcuts)
        : editor_{editor}, shortcuts_{shortcuts} {
    }

    /// The category's id.
    static constexpr std::string_view categoryId = "chords";

    [[nodiscard]] ui::SettingsCategory page() const;
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: A listens for the action's new chord, or restores the shipped chords.
    void run(const std::string& id, int step);

  private:
    ShortcutEditor& editor_;
    const input::Shortcuts& shortcuts_;
};

} // namespace opensu::app
