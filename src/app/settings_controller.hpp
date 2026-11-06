// settings_controller — the Settings screen's contents and what its rows do. Builds the categories
// and the pages their rows open from the saved preferences (every user-meaningful setting opensu
// keeps, and the environment's defaults they override), acts on a row by its id, and keeps each
// change through `Preferences`, the one owner of the settings file. The screens themselves
// (`SettingsScreen`, `SettingsSubPage`) are interface-toolkit screens on the shell's stack; folder
// rows go through `PathEditor`.
#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/navigator.hpp"
#include "gpu2d/ui/screen.hpp"

#include "artwork_settings.hpp"
#include "audio/sdl_sound_player.hpp"
#include "chord_settings.hpp"
#include "config/config.hpp"
#include "emulator_settings.hpp"
#include "gamepad/event.hpp"
#include "input/shortcuts.hpp"
#include "integration_settings.hpp"
#include "integrations_in_force.hpp"
#include "interface_sound_settings.hpp"
#include "lan_profiles.hpp"
#include "lan_roms.hpp"
#include "lan_settings.hpp"
#include "layer_settings.hpp"
#include "layered_settings.hpp"
#include "library/library_query.hpp"
#include "mapping_editor.hpp"
#include "online_play.hpp"
#include "path_editor.hpp"
#include "plugin_settings.hpp"
#include "preferences.hpp"
#include "profile_settings.hpp"
#include "rom_folder_settings.hpp"
#include "save_settings.hpp"
#include "settings/install_folders.hpp"
#include "shelf_options.hpp"
#include "shortcut_editor.hpp"
#include "steam_library_settings.hpp"
#include "ui/row_widgets.hpp"
#include "ui/screen_stack.hpp"
#include "ui/setting_row.hpp"
#include "wine_settings.hpp"

namespace opensu::app {

class SettingsController final : public ui::RowActions {
  public:
    /// What the screen asks of the shell app.
    struct Hooks {
        /// Gives the screen the layout, pin, icon size, scroll mode, sound and keyboard settings
        /// the preferences now hold.
        std::function<void()> applyLayout;
        /// Shows the shelf again from its start, after what the library shows changed.
        std::function<void()> reshow;
        /// Reads the stores again.
        std::function<void()> reloadCatalog;
        /// Where each store installs when no folder is set, as far as it is known now.
        std::function<settings::StoreFolders()> storeFolders;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
        /// The ROM folders were changed: the catalog reads them from now on.
        std::function<void()> libraryFoldersChanged;
        /// The Guide menu is under Settings, which closing must leave open.
        std::function<bool()> overGuide;
    };

    /// What the rows act on besides the preferences.
    struct Services {
        PathEditor& paths;
        ShortcutEditor& shortcutEditor;
        const input::Shortcuts& shortcuts;
        EmulatorSettings& emulators;
        LayerSettings& layers;
        LayeredSettings& layered;
        SteamLibrarySettings& steamLibraries;
        PluginSettings& plugins;
        ChordSettings& chords;
        MappingEditor& mappings;
        WineSettings& wine;
        IntegrationSettings& integrations;
        ArtworkSettings& artwork;
        InterfaceSoundSettings& interfaceSounds;
        LanSettings& lan;
        LanRoms& lanRoms;
        LanProfiles& lanProfiles;
        OnlinePlay& online;
        SaveSettings& saves;
        ProfileSettings& profiles;
        ShelfOptions& shelf;
        /// Which pages and rows show: those of the integrations in force.
        const IntegrationsInForce& inForce;
    };

    /// The Settings screens go on `navigator`'s stack, under `chrome`; `defaults` is the
    /// environment's configuration, which the settings override.
    SettingsController(gpu2d::ui::Navigator& navigator, const ui::Chrome& chrome, Services services,
                       audio::SdlSoundPlayer& sounds, Preferences& preferences,
                       const config::Config& defaults, Hooks hooks)
        : navigator_{navigator}, chrome_{chrome}, paths_{services.paths},
          shortcutEditor_{services.shortcutEditor}, shortcuts_{services.shortcuts},
          emulators_{services.emulators}, layers_{services.layers}, layered_{services.layered},
          steamLibraries_{services.steamLibraries}, plugins_{services.plugins},
          chords_{services.chords}, mappings_{services.mappings}, wine_{services.wine},
          integrations_{services.integrations}, artwork_{services.artwork},
          interfaceSounds_{services.interfaceSounds}, lan_{services.lan}, online_{services.online},
          lanRoms_{services.lanRoms}, lanProfiles_{services.lanProfiles}, saves_{services.saves},
          profiles_{services.profiles}, shelf_{services.shelf}, inForce_{services.inForce},
          sounds_{sounds}, preferences_{preferences}, defaults_{defaults},
          hooks_{std::move(hooks)} {
    }

    /// Opens the screen on its first category.
    void open();
    /// Opens the screen on the category with id `id` (the first when there is none).
    void openCategory(const std::string& id);
    /// Opens the screen on category `id` with its row `row` focused.
    void openAt(const std::string& id, const std::string& row);
    /// Closes every Settings screen at once, asking nothing.
    void close();
    [[nodiscard]] bool isOpen() const;
    /// A button while the screen is up and no chooser is over it.
    void act(gamepad::Button button);
    /// Shows the rows again after something outside the screen changed one.
    void refresh();
    /// Takes what the Steam libraries' and SteamGridDB's workers finished. Main loop only.
    void service();
    /// Back to the categories from any page; the trail's "Settings" level does this.
    void showCategories();
    /// The chosen category's name, then each page open over it ("Emulators › Consoles › Super
    /// Nintendo"); empty when the screen is closed.
    [[nodiscard]] std::string category() const;

    // What the screens ask.
    /// The categories in their order.
    [[nodiscard]] const std::vector<ui::SettingsCategory>& categories();
    /// The category or page `id`; an empty one when there is none.
    [[nodiscard]] const ui::SettingsCategory& page(const std::string& id);
    /// The title of the page `id`.
    [[nodiscard]] std::string pageTitle(const std::string& id);
    void run(const std::string& id, int step) override;
    void setLevel(const std::string& id, int level) override;
    void openPage(const std::string& id) override;
    /// Library › Add a ROM folder: the folder picker, and the folder kept and read once picked.
    void addRomFolder();
    /// Library › Add a game folder: the chooser, then the catalog read again.
    void addGameFolder();
    /// Library › Import from a PC on your network: Settings › LAN, with the page `LanRoms::entry`
    /// names over it when there is one.
    void openLanImport();
    /// Back on the categories, or the menu button anywhere: closes Settings, asking first when
    /// Integrations has changes not applied. Stays when it asked.
    [[nodiscard]] gpu2d::ui::Screen::BackReply leave();

  private:
    [[nodiscard]] std::vector<ui::SettingsCategory> build() const;
    [[nodiscard]] ui::SettingsCategory appearancePage() const;
    [[nodiscard]] ui::SettingsCategory libraryPage() const;
    [[nodiscard]] ui::SettingsCategory emulatorsPage() const;
    [[nodiscard]] ui::SettingsCategory audioPage() const;
    [[nodiscard]] ui::SettingsCategory controlsPage() const;
    [[nodiscard]] ui::SettingsCategory installPage() const;
    [[nodiscard]] ui::SettingsCategory aboutPage() const;
    /// A page a row opens, built now.
    [[nodiscard]] std::optional<ui::SettingsCategory> subPage(const std::string& id) const;
    /// Pushes the root screen on `category` (its row `row` focused, if given), after reading what
    /// the categories show.
    void show(const std::string& category, const std::string& row = {});
    /// Closes Settings with iiSU's Close sound.
    void closeAll();
    /// Takes Settings' screens off the stack, and all above the Guide or the root under them.
    void dropScreens();
    /// Keeps the preferences and shows the change.
    void changed();
    /// Puts Integrations' staged changes in effect and says so.
    void applyIntegrations();
    void runAppearance(const std::string& id, int step);
    void runLibrary(const std::string& id, int step);
    void runControls(const std::string& id);
    void chooseFolder(const std::string& id);
    void editInstallFolder(const std::string& title, const std::string& id);
    void addRoot(const std::string& id);

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    PathEditor& paths_;
    ShortcutEditor& shortcutEditor_;
    const input::Shortcuts& shortcuts_;
    EmulatorSettings& emulators_;
    LayerSettings& layers_;
    LayeredSettings& layered_;
    SteamLibrarySettings& steamLibraries_;
    PluginSettings& plugins_;
    ChordSettings& chords_;
    MappingEditor& mappings_;
    WineSettings& wine_;
    IntegrationSettings& integrations_;
    ArtworkSettings& artwork_;
    InterfaceSoundSettings& interfaceSounds_;
    LanSettings& lan_;
    OnlinePlay& online_;
    LanRoms& lanRoms_;
    LanProfiles& lanProfiles_;
    SaveSettings& saves_;
    ProfileSettings& profiles_;
    ShelfOptions& shelf_;
    const IntegrationsInForce& inForce_;
    audio::SdlSoundPlayer& sounds_;
    Preferences& preferences_;
    const config::Config& defaults_;
    Hooks hooks_;
    RomFolderSettings romFolders_{preferences_,
                                  defaults_,
                                  navigator_,
                                  sounds_,
                                  library::roms::openRomTree(),
                                  RomFolderSettings::Hooks{.changed =
                                                               [this] {
                                                                   changed();
                                                                   hooks_.libraryFoldersChanged();
                                                               },
                                                           .say =
                                                               [this](const std::string& text) {
                                                                   hooks_.say(text, false);
                                                               }}};
    /// The categories and pages as last built; emptied by `refresh`.
    std::optional<std::vector<ui::SettingsCategory>> categories_;
    std::map<std::string, ui::SettingsCategory, std::less<>> pages_;
};

} // namespace opensu::app
