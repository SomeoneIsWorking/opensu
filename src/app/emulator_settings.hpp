// emulator_settings — the Settings screen's Emulators category: every emulator of the catalog,
// whether and how it is installed, Download, Update and Remove (Remove only for an AppImage openSU
// downloaded, and only on a second press), the same for libretro's shader pack, the update check,
// the rows that open each setting layer's page (`LayerSettings`), and the BIOS each chosen built-in
// core runs on (its core info's files, present or missing in the runner's system folder, or HLE, an
// open BIOS built in, or none needed when none is there).
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "emulator_downloads.hpp"
#include "ui/settings_page.hpp"

namespace opensu::app {

class EmulatorSettings {
  public:
    struct Hooks {
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    EmulatorSettings(EmulatorDownloads& downloads, Hooks hooks)
        : downloads_{downloads}, hooks_{std::move(hooks)} {
    }

    /// The category's id, which its rows' ids start with.
    static constexpr std::string_view categoryId = "emulators";

    /// Searches for the emulators again; the category shows what was found.
    void rescan();
    /// The catalog's emulators.
    [[nodiscard]] const library::roms::EmulatorCatalog& catalog() const noexcept {
        return found_.catalog();
    }
    /// What the search found: each emulator's install and each system's automatic one.
    [[nodiscard]] const library::roms::Emulators& found() const noexcept {
        return found_;
    }
    /// The category, with `layerRows` after Check for updates.
    [[nodiscard]] ui::SettingsCategory page(std::vector<ui::SettingsRow> layerRows = {}) const;
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
    /// Cancels `id`'s download, says it is up to date, or starts it; `ours` is whether openSU
    /// downloaded what is there.
    void runDownload(std::string_view id, const std::string& name, bool ours);
    /// Arms Remove of `id`, or on the second press removes it.
    void runRemove(std::string_view id, const std::string& name);
    /// The shader pack's Download / Update row and its Remove.
    void addShaderPackRows(ui::SettingsCategory& page) const;
    /// What `id`'s row reads while it downloads or when `ours`; nothing otherwise.
    [[nodiscard]] std::optional<std::string> downloadValue(std::string_view id, bool ours) const;
    /// Each chosen built-in core's BIOS: what it runs on, and each file it looks for.
    void addBiosRows(ui::SettingsCategory& page) const;
    [[nodiscard]] std::string valueOf(const library::roms::EmulatorEntry& entry) const;
    [[nodiscard]] std::string noteOf(const library::roms::EmulatorEntry& entry) const;

    EmulatorDownloads& downloads_;
    Hooks hooks_;
    library::roms::Emulators found_{downloads_.discover()};
    /// The emulator whose Remove was pressed once.
    std::optional<std::string> armed_;
};

} // namespace opensu::app
