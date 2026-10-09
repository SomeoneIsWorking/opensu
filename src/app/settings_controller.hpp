// settings_controller — the Settings screen's contents and buttons. Builds the categories from the
// saved preferences (every user-meaningful setting opensu keeps, and the environment's defaults
// they override), acts on the focused row, and keeps each change through `Preferences`, the one
// owner of the settings file. Folder rows go through `PathEditor`.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "audio/sound_player.hpp"
#include "config/config.hpp"
#include "gamepad/event.hpp"
#include "input/shortcuts.hpp"
#include "library/library_query.hpp"
#include "path_editor.hpp"
#include "preferences.hpp"
#include "shortcut_editor.hpp"
#include "ui/settings_page.hpp"
#include "volume_control.hpp"

namespace opensu::app {

class SettingsController {
  public:
    /// What the screen asks of the shell app.
    struct Hooks {
        /// Gives the screen the layout, pin, icon size, scroll mode and sound setting the
        /// preferences now hold.
        std::function<void()> applyLayout;
        /// Shows the shelf again from its start, after what the library shows changed.
        std::function<void()> reshow;
        /// Reads the stores again.
        std::function<void()> reloadCatalog;
        /// The sources the source filter cycles through.
        std::function<std::vector<library::SourceChoice>()> sourceChoices;
        /// The Steam libraries the client has now.
        std::function<std::vector<std::filesystem::path>()> steamLibraries;
        /// Tells the player something; `error` marks a refusal.
        std::function<void(const std::string& text, bool error)> say;
    };

    /// What the rows act on besides the preferences.
    struct Services {
        PathEditor& paths;
        ShortcutEditor& shortcutEditor;
        VolumeControl& volume;
        const input::Shortcuts& shortcuts;
    };

    /// `defaults` is the environment's configuration, which the settings override.
    SettingsController(ui::SettingsPage& page, Services services, audio::SoundPlayer& sounds,
                       Preferences& preferences, const config::Config& defaults, Hooks hooks)
        : page_{page}, paths_{services.paths}, shortcutEditor_{services.shortcutEditor},
          volume_{services.volume}, shortcuts_{services.shortcuts}, sounds_{sounds},
          preferences_{preferences}, defaults_{defaults}, hooks_{std::move(hooks)} {
    }

    /// Opens the screen on its first category.
    void open();
    /// Closes the screen with iiSU's Close sound.
    void close();
    /// A button while the screen is up and no chooser is over it.
    void act(gamepad::Button button);
    /// A click on a slider's track at `level`.
    void chooseLevel(int level);
    /// Shows the rows again after something outside the screen changed one.
    void refresh();
    /// The pad goes back to the list of categories; the trail's "Settings" level does this.
    void showCategories();
    /// The focused category's name, or empty when the screen is closed.
    [[nodiscard]] std::string category() const;
    /// Whether a button now changes the focused setting, which the corner prompt says.
    [[nodiscard]] bool changes() const;

  private:
    [[nodiscard]] std::vector<ui::SettingsCategory> build() const;
    [[nodiscard]] ui::SettingsCategory appearancePage() const;
    [[nodiscard]] ui::SettingsCategory libraryPage() const;
    [[nodiscard]] ui::SettingsCategory audioPage() const;
    [[nodiscard]] ui::SettingsCategory controlsPage() const;
    [[nodiscard]] ui::SettingsCategory installPage() const;
    [[nodiscard]] ui::SettingsCategory aboutPage() const;
    /// Keeps the preferences and shows the change.
    void changed();
    /// Acts on `row`: `step` is 0 for A and -1 or 1 for Left and Right.
    void run(const ui::SettingsRow& row, int step);
    void runAppearance(const std::string& id, int step);
    void runLibrary(const std::string& id, int step);
    void runAudio(const std::string& id, int step);
    void runControls(const std::string& id);
    void chooseFolder(const std::string& id);
    void editInstallFolder(const std::string& title, const std::string& id);
    void addRoot(const std::string& id);
    void stepIconSize(int delta);

    ui::SettingsPage& page_;
    PathEditor& paths_;
    ShortcutEditor& shortcutEditor_;
    VolumeControl& volume_;
    const input::Shortcuts& shortcuts_;
    audio::SoundPlayer& sounds_;
    Preferences& preferences_;
    const config::Config& defaults_;
    Hooks hooks_;
    /// The Steam libraries as of the screen opening or a Steam folder changing.
    std::vector<std::filesystem::path> libraries_;
};

} // namespace opensu::app
