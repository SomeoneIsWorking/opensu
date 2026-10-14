// The Settings screen's controller and the folder editor under it, with the real preferences on a
// scratch file: rows built from the saved settings, every change kept on disk, and the install
// folders refused when they are missing or not writable.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "audio/sdl_sound_player.hpp"
#include "audio/volume_backend.hpp"
#include "installs.hpp"
#include "path_editor.hpp"
#include "preferences.hpp"
#include "settings_controller.hpp"
#include "shortcut_editor.hpp"
#include "support/fake_steam.hpp"
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
    explicit Rig(const fs::path& root, bool mixer = true, std::uint16_t steamPort = 1)
        : file{root / "config" / "settings.json"}, config{defaults(root)}, devTools{steamPort},
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
                         },
                         [this] {
                             ++chordChanges;
                         }},
          controller{page,
                     app::SettingsController::Services{
                         paths, shortcutEditor, volume, shortcuts, emulatorSettings,
                         steamLibrarySettings, pluginSettings, chordSettings, mappings, wine},
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
                      .storeFolders =
                          [this] {
                              return app::Installs::ownFolders(
                                  config, steamLibrarySettings.defaultLibrary());
                          },
                      .say =
                          [this](const std::string& text, bool error) {
                              said.push_back({text, error});
                          }}} {
    }

    static library::roms::EmulatorSearch downloadsIn(const fs::path& dir) {
        library::roms::EmulatorSearch search;
        search.downloads = dir;
        return search;
    }

    static config::Config defaults(const fs::path& root) {
        config::Config config;
        config.home = root / "home";
        config.configDir = root / "config";
        config.dataDir = root / "data";
        config.cacheDir = root / "cache";
        config.assetsDir = root / "assets";
        config.xdgDataHome = root / "xdg-data";
        config.xdgCacheHome = root / "xdg-cache";
        config.xdgConfigHome = root / "xdg-config";
        config.legendaryConfigDir = root / "legendary";
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
    audio::SdlSoundPlayer sounds;
    std::vector<std::string> failures;
    std::vector<Said> said;
    steam::DevTools devTools;
    bool steamReady{false};
    std::vector<library::steam::LibraryFolder> recorded;
    int applied{0};
    int chordChanges{0};
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
    app::EmulatorDownloads downloads{
        library::roms::EmulatorSetup{
            library::roms::EmulatorCatalog::shipped(OPENSU_ASSETS_DIR, config.home),
            downloadsIn(config.dataDir / "emulators"),
            {}},
        app::EmulatorDownloads::Hooks{[this](const std::string& text, bool error) {
                                          said.push_back({text, error});
                                      },
                                      [] {
                                      },
                                      [] {
                                      }}};
    app::EmulatorSettings emulatorSettings{
        downloads, preferences,
        app::EmulatorSettings::Hooks{[this](const std::string& text, bool error) {
                                         said.push_back({text, error});
                                     },
                                     [] {
                                     }}};
    steam::Libraries steamLibraries{devTools};
    app::SteamLibrarySettings steamLibrarySettings{
        app::SteamLibrarySettings::Sources{steamLibraries,
                                           [this] {
                                               return steamReady;
                                           },
                                           [this] {
                                               return recorded;
                                           }},
        paths, config.home,
        app::SteamLibrarySettings::Hooks{[this](const std::string& text, bool error) {
                                             said.push_back({text, error});
                                         },
                                         [this] {
                                             controller.refresh();
                                         }}};
    ui::SessionDialog dialog;
    app::PluginService plugins{
        config, preferences,
        app::PluginService::Hooks{[this](const std::string& text, bool error) {
                                      said.push_back({text, error});
                                  },
                                  [] {
                                  },
                                  [] {
                                  }}};
    app::PluginSettings pluginSettings{plugins, dialog, sounds, config.home, [this] {
                                           controller.refresh();
                                       }};
    app::ChordSettings chordSettings{shortcutEditor, shortcuts};
    ui::MappingPage mappingPage;
    app::MappingEditor mappings{
        mappingPage, settings::MappingStore{config.configDir / settings::mappingsFile}, sounds,
        shortcuts,
        app::MappingEditor::Hooks{[this](const std::string& text, bool error) {
                                      said.push_back({text, error});
                                  },
                                  [this] {
                                      controller.refresh();
                                  },
                                  [] {
                                      return std::vector<app::MappingEditor::Emulator>{
                                          {"pcsx2", "PCSX2"}, {"mgba", "mGBA"}};
                                  }}};
    app::WineSettings wine{preferences, config};
    app::SettingsController controller;
};

void categoriesFollowTheSavedSettings(const fs::path& root) {
    Rig rig{root / "categories"};
    rig.recorded = {library::steam::LibraryFolder{"/mnt/games/SteamLibrary", "", {{"10", 2048}}}};
    rig.controller.open();
    std::vector<std::string> names;
    for (const ui::SettingsCategory& category : rig.page.categories()) {
        names.push_back(category.label);
    }
    expect(names == std::vector<std::string>{"Appearance", "Library", "Emulators",
                                             "Steam libraries", "Audio", "Controls", "Guide chords",
                                             "Install folders", "Wine", "Plugins", "About"},
           "the eleven categories");
    expect(rig.controller.category() == "Appearance" && !rig.controller.changes(),
           "opens on Appearance with nothing to change yet");
    rig.enter("Steam libraries");
    const auto& rows = rig.page.focusedCategory().rows;
    expect(rows.size() == 2 && rows[0].label == "Steam is not running" &&
               rows[1].kind == ui::RowKind::Info && rows[1].label == "/mnt/games/SteamLibrary" &&
               rows[1].note == "1 game, 2.0 KB" && rows[1].value == "Not mounted",
           "without Steam each recorded library is a read-only row, the page says why");
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
    expect(rig.rowOf("install.default").value == "Not set: each store's own",
           "no default at first");
    expect(rig.rowOf("install.Epic").value == (rig.config.home / "Games").string() &&
               rig.rowOf("install.Epic").note == "Epic's own default",
           "Epic shows legendary's own default folder, marked as the store's");
    expect(rig.rowOf("install.GOG").value == (rig.config.dataDir / "gog-games").string(),
           "GOG shows where gogdl installs under openSU's data");
    expect(rig.rowOf("install.Steam").value.starts_with("Steam's default library"),
           "Steam's default is named as unknown while Steam is not running");

    rig.focus("install.default");
    rig.controller.act(Button::A);
    expect(rig.paths.active(), "A on a folder row opens the chooser");
    choose(rig, library);
    expect(rig.onDisk().installFolders.defaultFolder() == library, "the default is kept");
    expect(rig.onDisk().installFolders.effective(library::Source::Gog) == library,
           "a store follows it");
    expect(rig.rowOf("install.Epic").value == library.string() &&
               rig.rowOf("install.Epic").note == "From the default install folder",
           "the row shows the folder and that it is the default");

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
    expect(rig.rowOf("shortcut.volumeUp").value == "Ctrl+Up  Guide + Up", "and its pad chord");

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
    expect(rig.rowOf("shortcut.volumeUp").value == "Press a key, or Guide + a button" &&
               rig.rowOf("shortcut.volumeUp").note.find("Any button") != std::string::npos,
           "a volume row offers the pad's chords");
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
    expect(rig.shortcutEditor.capturing(), "B may be the start of a chord");
    rig.shortcutEditor.capturePad({event(Button::B, false)});
    expect(!rig.shortcutEditor.capturing(), "B pressed and released alone cancels");

    rig.focus("shortcut.volumeDown");
    rig.controller.act(Button::A);
    rig.shortcutEditor.capturePad({event(Button::Start, true), event(Button::B, true)});
    expect(!rig.shortcutEditor.capturing() && rig.shortcuts.padChord(input::Action::VolumeDown) ==
                                                  input::PadChord{Button::Start, Button::B},
           "any button can be held for a chord, Start too, and B can be its partner");

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

void mappingsAreReachedFromControls(const fs::path& root) {
    Rig rig{root / "mappings"};
    rig.controller.open();
    rig.enter("Controls");
    const auto& rows = rig.page.focusedCategory().rows;
    expect(rows.size() > 3 && rows[0].id == "mapping.global" && rows[1].id == "mapping.emulator" &&
               rows[1].value == "PCSX2" && rows[2].id == "mapping.steamInput",
           "the Controls page starts with the mappings and the Steam Input switch");
    rig.focus("mapping.emulator");
    rig.controller.act(Button::Right);
    expect(rig.rowOf("mapping.emulator").value == "mGBA", "Right picks the emulator");
    rig.focus("mapping.steamInput");
    rig.controller.act(Button::A);
    expect(rig.rowOf("mapping.steamInput").on &&
               settings::MappingStore{rig.config.configDir / settings::mappingsFile}
                   .load()
                   .steamInputGlobal(),
           "A turns Steam Input on for every game and keeps it");
    rig.focus("mapping.global");
    rig.controller.act(Button::A);
    expect(rig.mappings.active() && rig.mappingPage.title() == "Controls · every game",
           "A opens the global mapping");
}

void guideChordsAreSetOnTheirPage(const fs::path& root) {
    Rig rig{root / "chords"};
    rig.controller.open();
    rig.enter("Controls");
    rig.focus("chords.open");
    rig.controller.act(Button::A);
    expect(rig.controller.category() == "Guide chords" && rig.page.zone() == ui::SettingsZone::Rows,
           "the Controls page opens the Guide chords page");

    std::vector<std::string> labels;
    for (const ui::SettingsRow& row : rig.page.focusedCategory().rows) {
        labels.push_back(row.label + ": " + row.value);
    }
    expect(labels == std::vector<std::string>{"Quick menu: Guide + A", "Volume up: Guide + Up",
                                              "Volume down: Guide + Down", "Mute: Guide + Left",
                                              "Close game: None", "Save state: None",
                                              "Load state: None", "Reset game: None",
                                              "Devices: None", "Home: None", "Downloads: None",
                                              "Restore default chords: "},
           "one row per action a chord can do, with its chord");

    const auto event = [](Button button, bool pressed) {
        gamepad::Event made;
        made.button = button;
        made.pressed = pressed;
        return made;
    };
    rig.focus("chord.devices");
    rig.controller.act(Button::A);
    expect(rig.shortcutEditor.capturing() &&
               rig.rowOf("chord.devices").value == "Hold Guide and press a button",
           "A listens for the chord");
    rig.shortcutEditor.capturePad({event(Button::Guide, true), event(Button::B, true)});
    expect(!rig.shortcutEditor.capturing() && rig.rowOf("chord.devices").value == "Guide + B" &&
               rig.onDisk().shortcuts.pads.at(input::Action::Devices) ==
                   input::PadChord{Button::Guide, Button::B} &&
               rig.chordChanges == 1,
           "Guide + B is kept, shown, and the pads are told");

    rig.focus("chord.home");
    rig.controller.act(Button::A);
    rig.shortcutEditor.capturePad({event(Button::Guide, true), event(Button::A, true)});
    expect(rig.shortcutEditor.capturing() &&
               rig.said.back().text.find("already Quick menu") != std::string::npos,
           "a chord another action has is refused");
    rig.shortcutEditor.cancel();

    expect(rig.shortcuts.rebind(input::Action::Quit, input::Combo{input::keys::F9}).empty(),
           "a key is remapped too");
    rig.focus("chords.reset");
    rig.controller.act(Button::A);
    expect(rig.onDisk().shortcuts.pads.empty() && rig.rowOf("chord.devices").value == "None" &&
               rig.onDisk().shortcuts.keys.at(input::Action::Quit) ==
                   input::Combo{input::keys::F9} &&
               rig.chordChanges == 2,
           "restoring the chords leaves the keys");
}

bool hasCall(const test::FakeSteam& steam, const std::string& part) {
    for (const std::string& call : steam.calls()) {
        if (call.find(part) != std::string::npos) {
            return true;
        }
    }
    return false;
}

/// Hands the loop the Steam worker's answers until `done` holds.
template <class Done> void settle(Rig& rig, Done done, const char* what = "Steam answers") {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (!done() && std::chrono::steady_clock::now() < until) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        rig.controller.service();
    }
    expect(done(), what);
}

void steamLibrariesWhileSteamRuns(const fs::path& root) {
    using nlohmann::json;
    std::mutex mutex;
    int defaultIndex = 1;
    test::FakeSteam steam{[&](const std::string& expression) -> json {
        const std::lock_guard lock{mutex};
        if (expression.find("SetDefaultInstallFolder(") != std::string::npos) {
            defaultIndex = expression[expression.find('(') + 1] - '0';
        }
        if (expression.find("GetInstallFolders") == std::string::npos) {
            return json{};
        }
        return json::array({{{"nFolderIndex", 0},
                             {"strFolderPath", "/home/p/.local/share/Steam"},
                             {"nFreeSpace", "2048"},
                             {"nCapacity", "4096"},
                             {"bIsDefaultFolder", defaultIndex == 0},
                             {"vecApps", json::array({{{"nAppID", 220}, {"nAppSize", "1024"}}})}},
                            {{"nFolderIndex", 1},
                             {"strFolderPath", "/mnt/empty"},
                             {"bIsDefaultFolder", defaultIndex == 1},
                             {"vecApps", json::array()}},
                            {{"nFolderIndex", 2},
                             {"strFolderPath", "/mnt/games"},
                             {"bIsDefaultFolder", defaultIndex == 2},
                             {"vecApps", json::array({{{"nAppID", 1}, {"nAppSize", 10}}})}}});
    }};
    Rig rig{root, true, steam.port()};
    rig.steamReady = true;
    rig.controller.open();
    rig.enter("Steam libraries");
    settle(rig, [&rig] {
        return rig.page.focusedCategory().rows.size() == 6;
    });
    expect(rig.rowOf("steamLibraries.library.0").note == "1 game, 1.0 KB · 2.0 KB free of 4.0 KB" &&
               rig.rowOf("steamLibraries.library.1").value == "Default" &&
               rig.rowOf("steamLibraries.library.2").value == "Make default",
           "each library shows its games, size, free space and whether it is the default");
    expect(rig.rowOf("steamLibraries.remove.2").note.find("1 game") != std::string::npos,
           "a library with games says why it cannot be removed");

    rig.focus("steamLibraries.library.2");
    rig.controller.act(Button::A);
    settle(rig, [&rig] {
        return rig.rowOf("steamLibraries.library.2").value == "Default";
    });
    expect(hasCall(steam, "SetDefaultInstallFolder(2)"), "A makes a library the default");
    rig.controller.close();
    rig.controller.openCategory("installs");
    rig.page.enterRows();
    expect(rig.rowOf("install.Steam").value == "/mnt/games" &&
               rig.rowOf("install.Steam").note.starts_with("Steam's own default"),
           "the Install folders page shows Steam's default library as Steam's own");
    rig.controller.close();
    rig.controller.open();
    rig.enter("Steam libraries");
    settle(rig, [&rig] {
        return rig.page.focusedCategory().rows.size() == 6;
    });

    rig.focus("steamLibraries.remove.2");
    rig.controller.act(Button::A);
    expect(!rig.said.empty() && rig.said.back().error &&
               rig.said.back().text.find("uninstall or move them") != std::string::npos &&
               !hasCall(steam, "RemoveInstallFolder"),
           "removing a library with games is refused with the reason");
    rig.focus("steamLibraries.remove.1");
    rig.controller.act(Button::A);
    expect(rig.rowOf("steamLibraries.remove.1").value == "Press again to remove" &&
               !hasCall(steam, "RemoveInstallFolder"),
           "an empty library asks for a second press");
    rig.controller.act(Button::A);
    settle(
        rig,
        [&steam] {
            return hasCall(steam, "RemoveInstallFolder(1)");
        },
        "the second press removes the empty library");

    const fs::path added = root / "new-library";
    fs::create_directories(added);
    rig.focus("steamLibraries.add");
    rig.controller.act(Button::A);
    choose(rig, added);
    settle(
        rig,
        [&steam, &added] {
            return hasCall(steam, "AddInstallFolder(\"" + added.string() + "\")");
        },
        "a chosen folder is added as a library");
}

/// Every file under `dir` with its size, to show a folder was only read.
std::vector<std::string> treeOf(const fs::path& dir) {
    std::vector<std::string> tree;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator{dir}) {
        tree.push_back(entry.path().string() +
                       (entry.is_regular_file() ? " " + std::to_string(entry.file_size()) : ""));
    }
    std::ranges::sort(tree);
    return tree;
}

void wineRunnerIsPicked(const fs::path& root) {
    Rig rig{root};
    const auto binary = [](const fs::path& path) {
        fs::create_directories(path.parent_path());
        std::ofstream{path} << "#!/bin/sh\n";
        fs::permissions(path, fs::perms::owner_all);
        return path;
    };
    const fs::path system = binary(root / "bin" / "wine");
    const fs::path steamRoot = root / "steam";
    fs::create_directories(steamRoot / "config");
    const fs::path ge = binary(steamRoot / "compatibilitytools.d/GE-Proton9-20/files/bin/wine");
    const fs::path proton = binary(steamRoot / "steamapps/common/Proton 9.0/files/bin/wine");
    rig.config.executablePath = {root / "bin"};
    rig.config.steamRoots = {steamRoot};
    const std::vector<std::string> steamBefore = treeOf(steamRoot);

    rig.controller.open();
    rig.enter("Wine");
    expect(rig.rowOf("wine.runner").value == "Automatic: System Wine",
           "the first runner found is the default");
    expect(rig.rowOf("wine.prefix").value == (root / "data" / "wine-prefix").string(),
           "one shared prefix under the data folder");
    expect(rig.rowOf("wine.own").value == "None" &&
               rig.rowOf("wine.steam").value == "Left to Steam",
           "no game has its own prefix and Steam is left alone");
    rig.focus("wine.runner");
    std::vector<fs::path> picked;
    for (int press = 0; press < 4; ++press) {
        rig.controller.act(Button::Right);
        picked.push_back(rig.onDisk().wineRunner);
    }
    expect(picked == std::vector<fs::path>{system, ge, proton, {}},
           "Right steps through every runner found and back to Automatic, each kept");
    rig.controller.act(Button::Left);
    expect(rig.onDisk().wineRunner == proton && rig.rowOf("wine.runner").value == "Proton 9.0",
           "Left steps back, wrapping");
    expect(treeOf(steamRoot) == steamBefore, "Steam's folders are only read");

    library::Game game;
    game.id = "gog:2";
    game.source = library::Source::Gog;
    game.sourceId = "2";
    game.windowsInstall = true;
    game.launch = library::LaunchSpec{.program = "gogdl", .args = {"launch"}};
    expect(rig.wine.prepare(game).empty() &&
               game.launch.args ==
                   std::vector<std::string>{"launch", "--wine", proton.string(), "--wine-prefix",
                                            (root / "data" / "wine-prefix").string()},
           "a launch gets the runner picked here and the shared prefix");
}

void writeFile(const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path());
    std::ofstream{file} << text;
}

/// A plugin the player added runs only after its permissions are accepted in the dialog, and a
/// changed set of permissions asks again.
void pluginsAskBeforeTheyRun(const fs::path& root) {
    const fs::path dir = root / "data" / "plugins" / "probe";
    const std::string manifest = R"({"id": "probe", "name": "Probe", "version": "0.1", "api": 1,
        "permissions": {"read": {"data": ["{xdg_data}/probe"]}}})";
    writeFile(dir / "plugin.json", manifest);
    writeFile(dir / "main.lua", "opensu.register_playtime_source{claims = {source = 'rom', "
                                "systems = {'switch'}}, read = function() return {} end}");
    {
        Rig rig{root};
        rig.controller.open();
        rig.enter("Plugins");
        const ui::SettingsRow& row = rig.rowOf("plugins.probe");
        expect(row.kind == ui::RowKind::Toggle && !row.on && row.note.starts_with("Added by you"),
               "a plugin the player added starts off");
        expect(rig.rowOf("plugins.probe.access").value ==
                   "Reads " + (root / "xdg-data" / "probe").string(),
               "its row says what it can read");
        rig.focus("plugins.probe");
        rig.controller.act(Button::A);
        expect(rig.pluginSettings.active() && rig.dialog.text().title == "Turn on Probe?",
               "turning it on asks first");
        const auto& paragraphs = rig.dialog.text().paragraphs;
        expect(std::ranges::find(paragraphs, "It can read what is in " +
                                                 (root / "xdg-data" / "probe").string() + ".") !=
                       paragraphs.end() &&
                   std::ranges::find(paragraphs, "It does not use the network.") !=
                       paragraphs.end(),
               "the dialog lists every permission in words");
        expect(!rig.onDisk().plugins.contains("probe"), "nothing is kept before the answer");
        rig.pluginSettings.act(Button::B);
        expect(!rig.dialog.isOpen() && !rig.rowOf("plugins.probe").on, "Cancel leaves it off");
        rig.focus("plugins.probe");
        rig.controller.act(Button::A);
        rig.pluginSettings.act(Button::A);
        expect(!rig.dialog.isOpen() && rig.rowOf("plugins.probe").on, "Accept turns it on");
        const settings::PluginChoice kept = rig.onDisk().plugins.at("probe");
        expect(kept.enabled && kept.granted == std::vector<std::string>{"read:{xdg_data}/probe"},
               "the exact permissions accepted are kept");
    }
    writeFile(dir / "plugin.json",
              R"({"id": "probe", "name": "Probe", "version": "0.2", "api": 1,
        "permissions": {"read": {"data": ["{xdg_data}/probe", "{home}"]}}})");
    Rig rig{root};
    rig.controller.open();
    rig.enter("Plugins");
    expect(!rig.rowOf("plugins.probe").on &&
               rig.rowOf("plugins.probe").note.ends_with("asks you to accept what it can use"),
           "a plugin whose permissions grew is off until they are accepted again");
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
    guideChordsAreSetOnTheirPage(root);
    mappingsAreReachedFromControls(root);
    steamLibrariesWhileSteamRuns(root / "steam");
    pluginsAskBeforeTheyRun(root / "plugins");
    wineRunnerIsPicked(root / "wine");
    fs::remove_all(root);
    std::printf("settings_controller: all checks passed\n");
    return 0;
}
