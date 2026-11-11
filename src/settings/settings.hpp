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
#include "input/keyboard_style.hpp"
#include "input/shortcuts.hpp"
#include "install_folders.hpp"
#include "integrations.hpp"
#include "library/home_choices.hpp"
#include "library/library_query.hpp"
#include "library/play_history.hpp"
#include "library/sections.hpp"
#include "library/wine.hpp"
#include "night_light_choice.hpp"
#include "save_share_kind.hpp"
#include "sound_set.hpp"

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

/// The player's multiplier on the display's scale for `percent`: 1 at 100%.
[[nodiscard]] float uiScaleFactor(int percent) noexcept;

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

    /// What the player put on Home or took off it.
    library::HomeChoices home;

    /// When each game was last launched from opensu.
    library::PlayHistory lastPlayed;

    /// How the home grid scrolls, when the player chose; else OPENSU_HOME_MODE (iiSU
    /// `singleScreenHomeDashboardMode`).
    std::optional<config::HomeMode> homeMode;

    /// Whether the UI sounds play.
    bool uiSounds{true};

    /// Built-in cores' games draw in the shell's window, the core still in its own process
    /// (`GameOverlay`'s `embedsGames`).
    bool embeddedRunner{false};
    /// Whether openSU looks for ROM folders when `romFolders` is empty; off once the player removed
    /// their last one, until they ask it to find them.
    bool findRomFolders{true};

    /// ROM roots the player set; the environment's, else the discovered ones, when empty.
    std::vector<std::filesystem::path> romFolders;
    /// Folders of games and shortcuts, read on every library reload; none until the player adds
    /// one.
    std::vector<std::filesystem::path> gameFolders;

    /// Steam install roots the player set; the environment's, else the discovered ones, when empty.
    std::vector<std::filesystem::path> steamRoots;

    /// Where games install.
    InstallFolders installFolders;

    /// The Wine binary Windows GOG and Epic games run under; the newest GE-Proton openSU keeps up
    /// to date when empty.
    std::filesystem::path wineRunner;
    /// Whether openSU checks for GE-Proton at start and daily, downloads the newest and removes
    /// all but the newest two; off, it checks only when asked and removes nothing.
    bool keepGeProtonCurrent{true};
    /// The games pinned to a runner of their own, by game id: its Wine binary.
    std::map<std::string, std::filesystem::path> wineRunnerPins;

    /// The games that run in a Wine prefix of their own instead of the shared one.
    library::wine::OwnPrefixes ownWinePrefixes;

    /// How large the interface is drawn, in percent (`minUiScale` to `maxUiScale`).
    int uiScale{defaultUiScale};

    /// The shell stands in portrait where the screen can be turned (`app::screen_orientation`).
    bool portrait{false};

    /// Which on-screen keyboard openSU's fields open.
    input::KeyboardStyle keyboardStyle{input::KeyboardStyle::DaisyWheel};

    /// The night light in session mode.
    NightLightChoice nightLight{NightLightChoice::Desktop};

    /// The console art theme the home shows (`artwork::ConsoleThemes`); empty for the default.
    std::string consoleTheme;
    /// The interface theme the shell is drawn and sounds in (`theme::InterfaceThemes`); empty for
    /// the default.
    std::string interfaceTheme;
    /// The name other openSUs on the LAN list this one by; empty for the machine's host name.
    std::string lanName;
    /// The STUN servers Play online asks, `host:port` and comma-separated; empty for the default.
    std::string onlineStunServers;
    /// Play online also dials over the embedded ZeroTier node, which reaches ZeroTier's roots.
    bool onlineZeroTier{true};
    /// A file holding the ZeroTier root set Play online's node uses; empty for ZeroTier's own
    /// roots.
    std::filesystem::path onlineZeroTierRoots;
    std::filesystem::path saveShareFolder;
    /// The WebDAV folder's address and user.
    std::string saveShareUrl;
    std::string saveShareUser;
    /// Where saves are shared beside the LAN; its passphrase and WebDAV password are secrets.
    SaveShareKind saveShare{SaveShareKind::Off};
    /// Which interface sounds play, when the player chose: one of openSU's sets, or iiSU's from
    /// the file placed in the iiSU folder (`artwork::IisuInstall`); else the interface theme's.
    std::optional<SoundSet> interfaceSounds;
    /// Whether the interface theme's menu music plays behind the menus.
    bool menuMusic{false};

    /// The player's changes to the shortcut table.
    input::ShortcutOverrides shortcuts;

    /// The plugins the player turned on or off, by id. A bundled plugin not listed runs.
    std::map<std::string, PluginChoice> plugins;

    /// The integrations the player turned on; all off until they do.
    IntegrationChoices integrations;

    /// Adds `folder` to `romFolders` unless it is there already.
    void addRomFolder(const std::filesystem::path& folder);
    /// Takes `folder` out of `romFolders`; the last one gone, none is looked for.
    void removeRomFolder(const std::filesystem::path& folder);
    /// Whether any folder the settings keep is `folder`.
    [[nodiscard]] bool namesFolder(const std::filesystem::path& folder) const;
    /// Adds `folder` to `gameFolders` unless it is there already.
    void addGameFolder(const std::filesystem::path& folder);

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
