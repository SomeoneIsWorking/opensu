// settings — what the player chose and opensu keeps between runs, in one JSON file under the
// config directory (`config::Config::configDir`).
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config/config.hpp"
#include "input/shortcuts.hpp"
#include "install_folders.hpp"
#include "integrations.hpp"
#include "library/library_query.hpp"
#include "library/play_history.hpp"
#include "library/sections.hpp"
#include "library/wine.hpp"

namespace opensu::settings {

/// The file's name in the config directory.
inline constexpr std::string_view settingsFile = "settings.json";

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

/// What the player chose for one plugin.
struct PluginChoice {
    bool enabled{false};
    /// The permissions the player accepted (`plugin::Manifest::permissions`); a plugin whose
    /// permissions changed since does not run until they are accepted again.
    std::vector<std::string> granted;

    bool operator==(const PluginChoice&) const = default;
};

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

    /// The Wine binary Windows GOG and Epic games run under; the first one found when empty.
    std::filesystem::path wineRunner;

    /// The games that run in a Wine prefix of their own instead of the shared one.
    library::wine::OwnPrefixes ownWinePrefixes;

    /// How large the interface is drawn, in percent (`minUiScale` to `maxUiScale`).
    int uiScale{defaultUiScale};

    /// The player's changes to the shortcut table.
    input::ShortcutOverrides shortcuts;

    /// The plugins the player turned on or off, by id. A bundled plugin not listed runs.
    std::map<std::string, PluginChoice> plugins;

    /// The integrations the player chose; absent until the first-run chooser was answered.
    std::optional<IntegrationChoices> integrations;

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
