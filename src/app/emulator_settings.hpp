// emulator_settings — the Settings screen's Emulators category: every emulator of the catalog,
// whether and how it is installed, Download, Update and Remove (Remove only for an AppImage openSU
// downloaded, and only on a second press), the update check, and the emulator each system's games
// run on unless a game has its own pick.
#pragma once

#include <functional>
#include <optional>
#include <string>

#include "emulator_downloads.hpp"
#include "preferences.hpp"
#include "ui/settings_page.hpp"

namespace opensu::app {

class EmulatorSettings {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// A system's pick changed: the library applies it.
        std::function<void()> picksChanged;
    };

    EmulatorSettings(EmulatorDownloads& downloads, Preferences& preferences, Hooks hooks)
        : downloads_{downloads}, preferences_{preferences}, hooks_{std::move(hooks)} {
    }

    /// The category's id, which its rows' ids start with.
    static constexpr std::string_view categoryId = "emulators";

    /// Searches for the emulators again; the category shows what was found.
    void rescan();
    /// The catalog's emulators.
    [[nodiscard]] const library::roms::EmulatorCatalog& catalog() const noexcept {
        return found_.catalog();
    }
    [[nodiscard]] ui::SettingsCategory page() const;
    /// Whether the row with `id` is one of this category's.
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: `step` is 0 for A and -1 or 1 for Left and Right.
    void run(const std::string& id, int step);
    /// Forgets a Remove pressed once; focus moved away from it.
    void disarm() noexcept {
        armed_.reset();
    }

  private:
    void runEmulator(const library::roms::EmulatorEntry& entry);
    void runRemove(const library::roms::EmulatorEntry& entry);
    void stepSystem(const std::string& system, int step);
    [[nodiscard]] std::string valueOf(const library::roms::EmulatorEntry& entry) const;
    [[nodiscard]] std::string noteOf(const library::roms::EmulatorEntry& entry) const;

    EmulatorDownloads& downloads_;
    Preferences& preferences_;
    Hooks hooks_;
    library::roms::Emulators found_{downloads_.discover()};
    /// The emulator whose Remove was pressed once.
    std::optional<std::string> armed_;
};

} // namespace opensu::app
