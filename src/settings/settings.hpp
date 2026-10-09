// settings — what the player chose and opensu keeps between runs, in one JSON file under the
// config directory (`config::Config::configDir`).
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "config/config.hpp"
#include "input/shortcuts.hpp"
#include "install_folders.hpp"
#include "library/emulator_choice.hpp"
#include "library/library_query.hpp"
#include "library/play_history.hpp"
#include "library/sections.hpp"

namespace opensu::settings {

/// iiSU's icon size level bounds and default (`xmbIconSizeLevel`, navigation.md §2.2).
inline constexpr int minIconSize = 1;
inline constexpr int maxIconSize = 20;
inline constexpr int defaultIconSize = 9;

/// The interface scale in percent: 100 is the size the layouts are drawn at.
inline constexpr int minUiScale = 70;
inline constexpr int maxUiScale = 150;
inline constexpr int defaultUiScale = 100;
inline constexpr int uiScaleStep = 5;

/// The scale `step` choices from `current`, which A moves one forward, wrapping at either end.
[[nodiscard]] int steppedUiScale(int current, int step) noexcept;

struct Settings {
    /// How Library lays out its folders (iiSU `romCategoryLayoutMode`).
    library::LibraryMode libraryMode{library::LibraryMode::Standard};

    /// Whether the dock stays up on Library (iiSU `persistentNavBarOnPlatforms`, which iiSU
    /// defaults to off; openSU defaults to on so the way back to Home stays visible).
    bool pinLibraryDock{true};

    /// How large Library's tiles are, 1 to 20 (iiSU `xmbIconSizeLevel`).
    int iconSize{defaultIconSize};

    /// The sort, installed-only and source filters. The search text and the hidden-only filter
    /// are for one run and are not kept.
    library::ViewOptions view;

    /// The games the player hid.
    library::HiddenGames hidden;

    /// When each game was last launched from opensu.
    library::PlayHistory lastPlayed;

    /// The emulator picked for each ROM that has a pick.
    library::EmulatorChoices emulators;

    /// How the home grid scrolls, when the player chose; else OPENSU_HOME_MODE (iiSU
    /// `singleScreenHomeDashboardMode`).
    std::optional<config::HomeMode> homeMode;

    /// Whether the UI sounds play.
    bool uiSounds{true};

    /// ROM roots the player set; the environment's, else the discovered ones, when empty.
    std::vector<std::filesystem::path> romFolders;

    /// Steam install roots the player set; the environment's, else the discovered ones, when empty.
    std::vector<std::filesystem::path> steamRoots;

    /// Where games install.
    InstallFolders installFolders;

    /// How large the interface is drawn, in percent (`minUiScale` to `maxUiScale`).
    int uiScale{defaultUiScale};

    /// The player's changes to the shortcut table.
    input::ShortcutOverrides shortcuts;

    bool operator==(const Settings&) const = default;
};

/// `base` with what the player set in `settings` in place of the environment's: the home mode and
/// the ROM and Steam roots.
[[nodiscard]] config::Config resolved(config::Config base, const Settings& settings);

class Store {
  public:
    explicit Store(std::filesystem::path file);

    /// The saved settings. A file that is missing gives the defaults; one that cannot be read or
    /// parsed, or a value that is not one of the choices, is reported and takes the default.
    [[nodiscard]] Settings load() const;

    /// Replaces the file whole. False with `error` when it cannot; the old file is then kept.
    [[nodiscard]] bool save(const Settings& settings, std::string& error) const;

    [[nodiscard]] const std::filesystem::path& file() const noexcept {
        return file_;
    }

  private:
    std::filesystem::path file_;
};

} // namespace opensu::settings
