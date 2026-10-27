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
#include "layer_settings.hpp"
#include "library/library_query.hpp"
#include "mapping_editor.hpp"
#include "path_editor.hpp"
#include "plugin_settings.hpp"
#include "preferences.hpp"
#include "settings/install_folders.hpp"
#include "shortcut_editor.hpp"
#include "steam_library_settings.hpp"
#include "ui/screen_stack.hpp"
#include "ui/setting_row.hpp"
#include "volume_control.hpp"
#include "wine_settings.hpp"

namespace opensu::app {

class SettingsController {
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
        /// The sources the source filter cycles through.
        std::function<std::vector<library::SourceChoice>()> sourceChoices;
        /// Where each store installs when no folder is set, as far as it is known now.
        std::function<settings::StoreFolders()> storeFolders;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    /// What the rows act on besides the preferences.
    struct Services {
        PathEditor& paths;
        ShortcutEditor& shortcutEditor;
        VolumeControl& volume;
        const input::Shortcuts& shortcuts;
        EmulatorSettings& emulators;
        LayerSettings& layers;
        SteamLibrarySettings& steamLibraries;
        PluginSettings& plugins;
        ChordSettings& chords;
        MappingEditor& mappings;
        WineSettings& wine;
        IntegrationSettings& integrations;
        ArtworkSettings& artwork;
        /// Which pages and rows show: those of the integrations in force.
        const IntegrationsInForce& inForce;
    };

    /// The Settings screens go on `navigator`'s stack, under `chrome`; `defaults` is the
    /// environment's configuration, which the settings override.
    SettingsController(gpu2d::ui::Navigator& navigator, const ui::Chrome& chrome, Services services,
                       audio::SdlSoundPlayer& sounds, Preferences& preferences,
                       const config::Config& defaults, Hooks hooks)
        : navigator_{navigator}, chrome_{chrome}, paths_{services.paths}, shortcutEditor_{services.shortcutEditor},
          volume_{services.volume}, shortcuts_{services.shortcuts}, emulators_{services.emulators},
          layers_{services.layers}, steamLibraries_{services.steamLibraries},
          plugins_{services.plugins}, chords_{services.chords}, mappings_{services.mappings},
          wine_{services.wine}, integrations_{services.integrations}, artwork_{services.artwork},
          inForce_{services.inForce}, sounds_{sounds}, preferences_{preferences},
          defaults_{defaults}, hooks_{std::move(hooks)} {
    }

    /// Opens the screen on its first category.
    void open();
    /// Opens the screen on the category with id `id` (the first when there is none).
    void openCategory(const std::string& id);
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
    /// Acts on row `id`: `step` is 0 for A, else how many places a choice moves.
    void run(const std::string& id, int step);
    /// Sets the slider `id` to `level`.
    void setLevel(const std::string& id, int level);
    /// Asks the row's question, then runs it.
    void confirm(const ui::SettingsRow& row);
    /// Opens the page row `id` leads to.
    void openPage(const std::string& id);
    /// Back on the page `id`: stays when it stepped back within the page.
    [[nodiscard]] gpu2d::ui::Screen::BackReply backFrom(const std::string& id);
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
    /// Pushes the root screen on `category`, after reading what the categories show.
    void show(const std::string& category);
    /// Closes Settings with iiSU's Close sound.
    void closeAll();
    /// Keeps the preferences and shows the change.
    void changed();
    /// Puts Integrations' staged changes in effect and says so.
    void applyIntegrations();
    void runAppearance(const std::string& id, int step);
    void runLibrary(const std::string& id, int step);
    void runAudio(const std::string& id, int step);
    void runControls(const std::string& id);
    void runLayer(const std::string& id, int step);
    void chooseFolder(const std::string& id);
    void editInstallFolder(const std::string& title, const std::string& id);
    void addRoot(const std::string& id);

    gpu2d::ui::Navigator& navigator_;
    const ui::Chrome& chrome_;
    PathEditor& paths_;
    ShortcutEditor& shortcutEditor_;
    VolumeControl& volume_;
    const input::Shortcuts& shortcuts_;
    EmulatorSettings& emulators_;
    LayerSettings& layers_;
    SteamLibrarySettings& steamLibraries_;
    PluginSettings& plugins_;
    ChordSettings& chords_;
    MappingEditor& mappings_;
    WineSettings& wine_;
    IntegrationSettings& integrations_;
    ArtworkSettings& artwork_;
    const IntegrationsInForce& inForce_;
    audio::SdlSoundPlayer& sounds_;
    Preferences& preferences_;
    const config::Config& defaults_;
    Hooks hooks_;
    /// The categories and pages as last built; emptied by `refresh`.
    std::optional<std::vector<ui::SettingsCategory>> categories_;
    std::map<std::string, ui::SettingsCategory, std::less<>> pages_;
};

} // namespace opensu::app
