// emulator_settings — the Settings screen's Emulators category, in headed sections: the update
// check; the rows that open the setting layers' pages (`LayerSettings`); Downloads, a page with
// every emulator of the catalog (whether and how it is installed, Download, Update and Remove, the
// last only for what openSU downloaded and only once confirmed) and libretro's shader pack; and
// BIOS files, a page with the BIOS each chosen built-in core runs on (its core info's files,
// present or missing in the runner's system folder, or HLE, an open BIOS built in, or none needed
// when none is there).
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "emulator_downloads.hpp"
#include "ui/setting_row.hpp"

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
    /// The category, with `layerRows` under their own heading.
    [[nodiscard]] ui::SettingsCategory page(std::vector<ui::SettingsRow> layerRows = {}) const;
    /// The Downloads or BIOS files page `id` opens, or nothing for another id.
    [[nodiscard]] std::optional<ui::SettingsCategory> subPage(std::string_view id) const;
    /// Whether the row with `id` is one of this category's.
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: `step` is 0 for A and -1 or 1 for Left and Right. A Remove row's
    /// confirmation is asked before it runs.
    void run(const std::string& id, int step);

  private:
    void runEmulator(const library::roms::EmulatorEntry& entry);
    /// Cancels `id`'s download, says it is up to date, or starts it; `ours` is whether openSU
    /// downloaded what is there.
    void runDownload(std::string_view id, const std::string& name, bool ours);
    void runRemove(std::string_view id, const std::string& name);
    /// The shader pack's Download / Update row and its Remove.
    void addShaderPackRows(ui::SettingsCategory& page) const;
    /// Each emulator's Download / Update row and its Remove.
    void addEmulatorRows(ui::SettingsCategory& page) const;
    /// What `id`'s row reads while it downloads or when `ours`; nothing otherwise.
    [[nodiscard]] std::optional<std::string> downloadValue(std::string_view id, bool ours) const;
    /// Each chosen built-in core's BIOS: what it runs on, and each file it looks for.
    void addBiosRows(ui::SettingsCategory& page) const;
    [[nodiscard]] std::string valueOf(const library::roms::EmulatorEntry& entry) const;
    [[nodiscard]] std::string noteOf(const library::roms::EmulatorEntry& entry) const;

    EmulatorDownloads& downloads_;
    Hooks hooks_;
    library::roms::Emulators found_{downloads_.discover()};
};

} // namespace opensu::app
