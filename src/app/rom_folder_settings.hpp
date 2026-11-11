// rom_folder_settings — the ROM folders section of Settings › Library: each folder the player added
// with Remove, which asks first, Add a ROM folder (the controller's chooser; what the picked folder
// holds is said as it is added) and Find ROM folders automatically. Forgetting a folder touches no
// file and keeps everything tied to its games by id; the OS's grant on a picked folder goes back
// once no setting names that folder.
#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "config/config.hpp"
#include "library/rom_tree.hpp"
#include "preferences.hpp"
#include "ui/setting_row.hpp"

namespace opensu::app {

class RomFolderSettings {
  public:
    struct Hooks {
        /// The player's ROM folders changed: kept, and read by the catalog from now on.
        std::function<void()> changed;
        /// Tells the player something done.
        std::function<void(const std::string& text)> say;
    };

    /// `defaults` is the environment's configuration; `tree` names folders and releases grants.
    RomFolderSettings(Preferences& preferences, const config::Config& defaults,
                      gpu2d::ui::Navigator& navigator, audio::SdlSoundPlayer& sounds,
                      std::unique_ptr<const library::roms::RomTree> tree, Hooks hooks)
        : preferences_{preferences}, defaults_{defaults}, navigator_{navigator}, sounds_{sounds},
          tree_{std::move(tree)}, hooks_{std::move(hooks)} {
    }

    /// The row whose press opens the chooser; the controller owns that flow.
    static constexpr std::string_view addId = "roms.add";

    /// The section's rows, heading first.
    [[nodiscard]] std::vector<ui::SettingsRow> rows() const;
    /// Whether row `id` is one `run` acts on.
    [[nodiscard]] static bool owns(const std::string& id);
    /// Acts on row `id`: Remove asks first, Find ROM folders automatically forgets them all.
    void run(const std::string& id, int step);
    /// Adds the picked `folder` and says what it holds: no games and where they go, or the loose
    /// files left out because their extension is more than one console's.
    void add(const std::filesystem::path& folder);

  private:
    void ask(const std::filesystem::path& folder);
    void remove(const std::filesystem::path& folder);
    void findAutomatically();
    /// Gives back the OS's grant on `folder`, which the settings no longer hold as a ROM folder.
    void release(const std::filesystem::path& folder) const;

    Preferences& preferences_;
    const config::Config& defaults_;
    gpu2d::ui::Navigator& navigator_;
    audio::SdlSoundPlayer& sounds_;
    std::unique_ptr<const library::roms::RomTree> tree_;
    Hooks hooks_;
};

} // namespace opensu::app
