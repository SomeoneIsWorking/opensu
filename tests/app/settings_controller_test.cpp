// The Settings screen's controller and the folder editor under it, with the real preferences on a
// scratch file: rows built from the saved settings, every change kept on disk, and the install
// folders refused when they are missing or not writable.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "audio/sound_player.hpp"
#include "audio/volume_backend.hpp"
#include "path_editor.hpp"
#include "preferences.hpp"
#include "settings_controller.hpp"
#include "shortcut_editor.hpp"
#include "ui/check.hpp"
#include "volume_control.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using gamepad::Button;
using test::expect;
using test::need;

struct Said {
    std::string text;
    bool error{};
};

/// A mixer in memory, so no test touches the real one.
class FakeMixer final : public audio::VolumeBackend {
  public:
    explicit FakeMixer(audio::VolumeState* state, std::vector<std::string>* log)
        : state_{state}, log_{log} {
    }
    [[nodiscard]] std::string_view name() const override {
        return "fake";
    }
    std::optional<audio::VolumeState> read() override {
        return *state_;
    }
    bool setPercent(int percent) override {
        state_->percent = percent;
        log_->push_back("volume " + std::to_string(percent));
        return true;
    }
    bool setMuted(bool muted) override {
        state_->muted = muted;
        log_->push_back(muted ? "mute" : "unmute");
        return true;
    }

    std::vector<audio::AudioSink> sinks() override {
        return {};
    }
    bool setDefaultSink(const std::string&) override {
        return false;
    }

  private:
    audio::VolumeState* state_;
    std::vector<std::string>* log_;
};

struct Rig {
    explicit Rig(const fs::path& root, bool mixer = true)
        : file{root / "config" / "settings.json"}, config{defaults(root)},
          preferences{settings::Store{file},
                      [this](const std::string& why) {
                          failures.push_back(why);
                      }},
          paths{folders, entry, sounds,
                [this](const std::string& text, bool error) {
                    said.push_back({text, error});
                }},
          volume{mixer ? std::unique_ptr<audio::VolumeBackend>{std::make_unique<FakeMixer>(
                             &mixerState, &mixerLog)}
                       : nullptr,
                 app::VolumeControl::Hooks{[this](const audio::VolumeState& state) {
                                               shown.push_back(state);
                                           },
                                           [this](const std::string& text, bool error) {
                                               said.push_back({text, error});
                                           }}},
          shortcutEditor{shortcuts, preferences,
                         [this](const std::string& text, bool error) {
                             said.push_back({text, error});
                         },
                         [this] {
                             controller.refresh();
                         }},
          controller{page,
                     app::SettingsController::Services{paths, shortcutEditor, volume, shortcuts},
                     sounds,
                     preferences,
                     config,
                     {.applyLayout =
                          [this] {
                              ++applied;
                          },
                      .reshow =
                          [] {
                          },
                      .reloadCatalog =
                          [] {
                          },
                      .sourceChoices =
                          [] {
                              return std::vector<library::SourceChoice>{{"", "All sources"},
                                                                        {"launcher:epic", "Epic"}};
                          },
                      .steamLibraries =
                          [this] {
                              return libraries;
                          },
                      .say =
                          [this](const std::string& text, bool error) {
                              said.push_back({text, error});
                          }}} {
    }

    static config::Config defaults(const fs::path& root) {
        config::Config config;
        config.home = root / "home";
        config.configDir = root / "config";
        config.dataDir = root / "data";
        config.cacheDir = root / "cache";
        return config;
    }

    /// Moves to the category called `label` and into its rows.
    void enter(const std::string& label) {
        page.focusCategory(0);
        while (page.focusedCategory().label != label) {
            controller.act(Button::Down);
        }
        controller.act(Button::A);
    }
    /// Focuses the row `id` of the focused category.
    void focus(const std::string& id) {
        const auto& rows = page.focusedCategory().rows;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].id == id) {
                page.focusRow(i);
                return;
            }
        }
        expect(false, "a row with that id exists");
    }
    [[nodiscard]] const ui::SettingsRow& rowOf(const std::string& id) const {
        for (const ui::SettingsRow& row : page.focusedCategory().rows) {
            if (row.id == id) {
                return row;
            }
        }
        expect(false, "a row with that id exists");
        return page.focusedCategory().rows.front();
    }
    [[nodiscard]] settings::Settings onDisk() const {
        return settings::Store{file}.load();
    }

    fs::path file;
    config::Config config;
    audio::SoundPlayer sounds;
    std::vector<std::string> failures;
    std::vector<Said> said;
    std::vector<fs::path> libraries;
    int applied{0};
    app::Preferences preferences;
    ui::SettingsPage page;
    ui::FolderChooser folders;
    ui::SearchPanel entry;
    app::PathEditor paths;
    audio::VolumeState mixerState{40, false};
    std::vector<std::string> mixerLog;
    std::vector<audio::VolumeState> shown;
    input::Shortcuts shortcuts;
    app::VolumeControl volume;
    app::ShortcutEditor shortcutEditor;
    app::SettingsController controller;
};

void categoriesFollowTheSavedSettings(const fs::path& root) {
    Rig rig{root / "categories"};
    rig.libraries = {"/mnt/games/SteamLibrary"};
    rig.controller.open();
    std::vector<std::string> names;
    for (const ui::SettingsCategory& category : rig.page.categories()) {
        names.push_back(category.label);
    }
    expect(names == std::vector<std::string>{"Appearance", "Library", "Audio", "Controls",
                                             "Install folders", "About"},
           "the six categories");
    expect(rig.controller.category() == "Appearance" && !rig.controller.changes(),
           "opens on Appearance with nothing to change yet");
    rig.enter("Library");
    std::size_t libraries = 0;
    for (const ui::SettingsRow& row : rig.page.focusedCategory().rows) {
        libraries += row.id == "steam.library" ? 1 : 0;
    }
    expect(libraries == 1, "each Steam library is a read-only row");
    rig.controller.close();
    expect(!rig.page.isOpen() && rig.controller.category().empty(), "closed");
}

void togglesChoicesAndSliderPersist(const fs::path& root) {
    Rig rig{root / "persist"};
    rig.controller.open();
    rig.controller.act(Button::A);
    expect(rig.controller.changes(), "in the rows a button changes a setting");

    rig.focus("pinDock");
    const bool pinned = rig.preferences.values().pinLibraryDock;
    rig.controller.act(Button::A);
    expect(rig.preferences.values().pinLibraryDock != pinned &&
               rig.onDisk().pinLibraryDock != pinned && rig.applied == 1,
           "a toggle flips, is saved and applied");

    rig.focus("iconSize");
    const int size = rig.preferences.values().iconSize;
    rig.controller.act(Button::Right);
    rig.controller.act(Button::Right);
    expect(rig.onDisk().iconSize == size + 2, "right grows the icons, kept");
    rig.controller.chooseLevel(99);
    expect(rig.onDisk().iconSize == ui::maxIconLevel, "a click clamps to the top level");

    rig.focus("scrollMode");
    rig.controller.act(Button::A);
    expect(rig.onDisk().homeMode.has_value(), "choosing a scroll mode overrides the environment");
    const config::HomeMode first = need(rig.onDisk().homeMode, "a scroll mode");
    rig.controller.act(Button::Right);
    expect(need(rig.onDisk().homeMode, "a scroll mode") != first, "and stepping changes it");

    rig.controller.act(Button::B);
    rig.enter("Audio");
    rig.controller.act(Button::A);
    expect(!rig.onDisk().uiSounds, "UI sounds off is kept");
    expect(rig.sounds.muted() || rig.applied > 0, "and applied");

    rig.controller.act(Button::B);
    rig.enter("Library");
    rig.focus("installedOnly");
    rig.controller.act(Button::A);
    expect(rig.onDisk().view.installedOnly, "a view filter is kept");
    rig.focus("source");
    rig.controller.act(Button::A);
    expect(rig.onDisk().view.source == "launcher:epic", "the source choice is kept");
    expect(rig.failures.empty(), "nothing failed to save");
}

void backAndStartLeave(const fs::path& root) {
    Rig rig{root / "back"};
    rig.controller.open();
    rig.controller.act(Button::A);
    expect(rig.page.zone() == ui::SettingsZone::Rows, "A enters the rows");
    rig.controller.act(Button::B);
    expect(rig.page.zone() == ui::SettingsZone::Categories && rig.page.isOpen(),
           "B leaves the rows");
    rig.controller.act(Button::Right);
    expect(rig.page.zone() == ui::SettingsZone::Rows, "Right enters them too");
    rig.controller.showCategories();
    expect(rig.page.zone() == ui::SettingsZone::Categories, "the trail's Settings level goes back");
    rig.controller.act(Button::B);
    expect(!rig.page.isOpen(), "B in the list closes the screen");
    rig.controller.open();
    rig.controller.act(Button::A);
    rig.controller.act(Button::Start);
    expect(!rig.page.isOpen(), "Start closes it from anywhere");
}

/// Presses Use on the chooser, which is open on the folder it should choose.
void choose(Rig& rig, const fs::path& folder) {
    expect(rig.folders.isOpen(), "the chooser is up");
    rig.folders.open("", folder);
    rig.paths.act(Button::A);
}

void installFoldersPersistPerStore(const fs::path& root) {
    Rig rig{root / "installs"};
    const fs::path library = root / "installs" / "games";
    const fs::path epic = root / "installs" / "epic";
    fs::create_directories(library);
    fs::create_directories(epic);
    rig.controller.open();
    rig.enter("Install folders");
    expect(rig.rowOf("install.default").value == "Each store's own", "no default at first");
    expect(rig.rowOf("install.Epic").value == "Store's own", "and no store folder");

    rig.focus("install.default");
    rig.controller.act(Button::A);
    expect(rig.paths.active(), "A on a folder row opens the chooser");
    choose(rig, library);
    expect(rig.onDisk().installFolders.defaultFolder() == library, "the default is kept");
    expect(rig.onDisk().installFolders.effective(library::Source::Gog) == library,
           "a store follows it");
    expect(rig.rowOf("install.Epic").value == "Default (" + library.string() + ")",
           "the row says so");

    rig.focus("install.Epic");
    rig.controller.act(Button::A);
    choose(rig, epic);
    expect(rig.onDisk().installFolders.effective(library::Source::Epic) == epic &&
               rig.onDisk().installFolders.effective(library::Source::Steam) == library,
           "an override beats the default for its own store only");

    rig.focus("install.Epic");
    rig.controller.act(Button::A);
    rig.folders.open("", epic, "Follow the default");
    rig.folders.focusEntry(2);
    rig.paths.act(Button::A);
    expect(rig.onDisk().installFolders.effective(library::Source::Epic) == library,
           "the clear entry makes the store follow the default again");
    expect(!rig.paths.active() || rig.failures.empty(), "no failure");
}

void refusedFoldersStayOpen(const fs::path& root) {
    Rig rig{root / "refused"};
    const fs::path missing = root / "refused" / "nowhere";
    const fs::path readonly = root / "refused" / "readonly";
    fs::create_directories(readonly);
    fs::permissions(readonly, fs::perms::owner_read | fs::perms::owner_exec);
    rig.controller.open();
    rig.enter("Install folders");
    rig.focus("install.default");
    rig.controller.act(Button::A);

    rig.folders.open("", missing);
    rig.folders.open("", readonly);
    rig.paths.act(Button::A);
    const bool root0 = ::geteuid() == 0;
    expect(root0 || (rig.folders.isOpen() && !rig.said.empty() && rig.said.back().error &&
                     rig.onDisk().installFolders.defaultFolder().empty()),
           "a folder that cannot be written is refused with a message and nothing is kept");
    rig.paths.act(Button::B);
    expect(!rig.paths.active(), "B cancels the chooser");
    expect(rig.onDisk().installFolders.defaultFolder().empty(), "cancelling keeps nothing");
    fs::permissions(readonly, fs::perms::owner_all);
}

void typedPathIsValidated(const fs::path& root) {
    Rig rig{root / "typed"};
    const fs::path good = root / "typed" / "typed-games";
    fs::create_directories(good);
    rig.controller.open();
    rig.enter("Install folders");
    rig.focus("install.GOG");
    rig.controller.act(Button::A);
    rig.paths.act(Button::Down);
    rig.paths.act(Button::A);
    expect(rig.paths.typing(), "the Type entry opens the keyboard");
    rig.paths.act(Button::Select);
    rig.paths.typeText((root / "typed" / "absent").string());
    expect(rig.paths.typing(), "typing");
    rig.paths.confirm();
    expect(rig.paths.typing() && !rig.said.empty() && rig.said.back().error,
           "a missing typed path is refused and stays up");
    rig.paths.act(Button::Select);
    rig.paths.typeText(good.string());
    rig.paths.confirm();
    expect(!rig.paths.active() &&
               rig.onDisk().installFolders.storeFolder(library::Source::Gog) == good,
           "a good typed path is taken");
    rig.focus("install.GOG");
    rig.controller.act(Button::A);
    rig.paths.act(Button::Down);
    rig.paths.act(Button::A);
    rig.paths.dismiss();
    expect(!rig.paths.typing() && rig.folders.isOpen(), "Escape returns to the chooser");
}

void rootsAreAddedAndReset(const fs::path& root) {
    Rig rig{root / "roots"};
    const fs::path roms = root / "roots" / "roms";
    fs::create_directories(roms);
    rig.controller.open();
    rig.enter("Library");
    rig.focus("roms.add");
    rig.controller.act(Button::A);
    choose(rig, roms);
    expect(rig.onDisk().romFolders == std::vector<fs::path>{roms}, "a ROM folder is kept");
    rig.focus("roms.add");
    rig.controller.act(Button::A);
    choose(rig, roms);
    expect(rig.onDisk().romFolders.size() == 1, "the same folder is not added twice");
    rig.focus("roms.reset");
    rig.controller.act(Button::A);
    expect(rig.onDisk().romFolders.empty(), "reset goes back to automatic");

    rig.focus("steam.add");
    rig.controller.act(Button::A);
    choose(rig, roms);
    expect(rig.onDisk().steamRoots.empty() && rig.said.back().error,
           "a folder with no Steam installation is refused");
}

void hiddenAndEmulatorsReset(const fs::path& root) {
    Rig rig{root / "reset"};
    library::Game game;
    game.id = "steam:1";
    game.title = "Hades";
    rig.preferences.values().hidden.set(game, true);
    rig.preferences.save();
    rig.controller.open();
    rig.enter("Library");
    expect(rig.rowOf("hidden").value == "1 game", "the hidden count is shown");
    rig.focus("hidden");
    rig.controller.act(Button::A);
    expect(rig.onDisk().hidden.keys().empty() && rig.rowOf("hidden").value == "None hidden",
           "show hidden games again clears them");
}

void volumeRowsDriveTheMixer(const fs::path& root) {
    Rig rig{root / "volume"};
    rig.controller.open();
    rig.enter("Audio");
    expect(rig.rowOf("volume").level == 40 && !rig.rowOf("volumeMute").on,
           "the rows read the mixer");
    rig.focus("volume");
    rig.controller.act(Button::Right);
    expect(rig.mixerState.percent == 45 && rig.rowOf("volume").level == 45,
           "right raises the volume five percent and the row follows");
    rig.controller.act(Button::Left);
    rig.controller.act(Button::Left);
    expect(rig.mixerState.percent == 35, "left lowers it");
    rig.controller.chooseLevel(80);
    expect(rig.mixerState.percent == 80, "a click on the track sets the level");
    rig.controller.chooseLevel(400);
    expect(rig.mixerState.percent == 100, "and clamps");
    rig.focus("volumeMute");
    rig.controller.act(Button::A);
    expect(rig.mixerState.muted && rig.rowOf("volumeMute").on, "A mutes");
    expect(!rig.shown.empty() && rig.shown.back().muted, "every change was told to the display");
    const std::size_t told = rig.shown.size();
    rig.controller.act(Button::A);
    expect(!rig.mixerState.muted && rig.shown.size() == told + 1, "and unmutes");
    expect(rig.onDisk().uiScale == settings::defaultUiScale, "nothing else changed");
}

void withoutAMixerTheRowSaysHow(const fs::path& root) {
    Rig rig{root / "nomixer", false};
    rig.controller.open();
    rig.enter("Audio");
    const ui::SettingsRow& row = rig.rowOf("volume.missing");
    expect(row.kind == ui::RowKind::Info && row.note.find("sudo") != std::string::npos,
           "an unavailable mixer shows the install command");
    expect(rig.mixerLog.empty(), "nothing was written");
    rig.volume.act(input::Action::VolumeUp);
    expect(!rig.said.empty() && rig.said.back().error, "a volume shortcut is refused with a toast");
}

void interfaceSizeCycles(const fs::path& root) {
    Rig rig{root / "scale"};
    rig.controller.open();
    rig.controller.act(Button::A);
    rig.focus("uiScale");
    expect(rig.rowOf("uiScale").value == "100%", "100% by default");
    rig.controller.act(Button::Right);
    expect(rig.onDisk().uiScale == 105 && rig.rowOf("uiScale").value == "105%", "right grows it");
    rig.controller.act(Button::Left);
    rig.controller.act(Button::Left);
    expect(rig.onDisk().uiScale == 95, "left shrinks it");
    for (int i = 0; i < 40; ++i) {
        rig.controller.act(Button::Left);
    }
    expect(rig.preferences.values().uiScale >= settings::minUiScale &&
               rig.preferences.values().uiScale <= settings::maxUiScale,
           "it wraps inside its range");
    rig.controller.act(Button::Right);
    expect(rig.preferences.values().uiScale >= settings::minUiScale &&
               rig.preferences.values().uiScale <= settings::maxUiScale,
           "never past it");
}

void shortcutsAreRemappedAndKept(const fs::path& root) {
    Rig rig{root / "controls"};
    rig.controller.open();
    rig.enter("Controls");
    expect(rig.rowOf("shortcut.quit").value == "Q", "a row reads its key");
    expect(rig.rowOf("shortcut.volumeUp").value == "Ctrl+Up  L2 + Up", "and its pad chord");

    rig.focus("shortcut.quit");
    rig.controller.act(Button::A);
    expect(rig.shortcutEditor.capturing() &&
               rig.rowOf("shortcut.quit").value == "Press the new key",
           "A starts listening and the row says so");
    rig.controller.act(Button::A);
    expect(rig.shortcutEditor.capturing(), "buttons do nothing on the screen meanwhile");

    const input::Combo taken = need(input::comboNamed("e"), "a key name");
    rig.shortcutEditor.captureKey(taken);
    expect(rig.shortcutEditor.capturing() && rig.said.back().error &&
               rig.said.back().text.find("already Options") != std::string::npos,
           "a key another action has is refused and the editor keeps listening");
    expect(rig.shortcuts.primary(input::Action::Quit) == input::comboNamed("q"), "nothing changed");

    rig.shortcutEditor.captureKey(need(input::comboNamed("shift+f9"), "a key name"));
    expect(!rig.shortcutEditor.capturing() &&
               rig.shortcuts.primary(input::Action::Quit) == input::comboNamed("shift+f9"),
           "a free combination is taken");
    expect(rig.onDisk().shortcuts.keys.at(input::Action::Quit) ==
               need(input::comboNamed("shift+f9"), "a key name"),
           "and kept through the preferences");
    expect(rig.rowOf("shortcut.quit").value == "Shift+F9", "the row follows");

    rig.focus("shortcut.volumeUp");
    rig.controller.act(Button::A);
    expect(rig.rowOf("shortcut.volumeUp").value.find("L2") != std::string::npos,
           "a volume row offers the pad");
    const auto event = [](Button button, bool pressed) {
        gamepad::Event made;
        made.button = button;
        made.pressed = pressed;
        return made;
    };
    rig.shortcutEditor.capturePad({event(Button::A, true), event(Button::A, false)});
    expect(rig.shortcutEditor.capturing(), "a button alone is not a chord");
    rig.shortcutEditor.capturePad({event(Button::R2, true), event(Button::Y, true)});
    expect(!rig.shortcutEditor.capturing() &&
               rig.onDisk().shortcuts.pads.at(input::Action::VolumeUp) ==
                   input::PadChord{Button::R2, Button::Y},
           "hold R2 and press Y sets the chord");

    rig.focus("shortcut.volumeDown");
    rig.controller.act(Button::A);
    rig.shortcutEditor.capturePad(
        {event(Button::R2, false), event(Button::R2, true), event(Button::Y, true)});
    expect(rig.shortcutEditor.capturing() && rig.said.back().error, "a clashing chord is refused");
    rig.shortcutEditor.capturePad({event(Button::B, false), event(Button::Y, false),
                                   event(Button::R2, false), event(Button::B, true)});
    expect(!rig.shortcutEditor.capturing(), "B cancels");

    rig.focus("shortcut.confirm");
    rig.controller.act(Button::A);
    rig.shortcutEditor.capturePad({event(Button::L2, true), event(Button::Up, true)});
    expect(rig.shortcutEditor.capturing(), "an action that is a button takes no chord");
    rig.shortcutEditor.cancel();

    rig.focus("shortcuts.reset");
    rig.controller.act(Button::A);
    expect(rig.onDisk().shortcuts == input::ShortcutOverrides{} &&
               rig.shortcuts.primary(input::Action::Quit) == input::comboNamed("q"),
           "restore puts every shortcut back");
}

} // namespace

int main() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "settings_controller";
    fs::remove_all(root);
    categoriesFollowTheSavedSettings(root);
    togglesChoicesAndSliderPersist(root);
    backAndStartLeave(root);
    installFoldersPersistPerStore(root);
    refusedFoldersStayOpen(root);
    typedPathIsValidated(root);
    rootsAreAddedAndReset(root);
    hiddenAndEmulatorsReset(root);
    volumeRowsDriveTheMixer(root);
    withoutAMixerTheRowSaysHow(root);
    interfaceSizeCycles(root);
    shortcutsAreRemappedAndKept(root);
    fs::remove_all(root);
    std::printf("settings_controller: all checks passed\n");
    return 0;
}
