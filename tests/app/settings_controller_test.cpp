// The Settings screen's controller and the folder editor under it, with the real preferences on a
// scratch file: rows built from the saved settings, every change kept on disk, and the install
// folders refused when they are missing or not writable.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
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
#include "settings_screen.hpp"
#include "shortcut_editor.hpp"
#include "support/fake_interface_text.hpp"
#include "support/fake_steam.hpp"
#include "support/fake_steam_gate.hpp"
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
          steamGridDbAccount{app::SteamGridDbAccount::Parts{
              host::SecretStore{config.configDir / "secrets", false}, steamGridDb, steamPort,
              steam::SteamGridDbSignIn::Timing{std::chrono::milliseconds{10},
                                               std::chrono::seconds{10}},
              "true",
              [this, steamPort]() -> launch::SteamGate* {
                  return steamPort == 1 ? nullptr : &steamGate;
              }}},
          artworkSettings{steamGridDbAccount, entry, sounds,
                          app::ArtworkSettings::Hooks{[this](const std::string& text, bool error) {
                                                          said.push_back({text, error});
                                                      },
                                                      [this] {
                                                          ++artworkRequests;
                                                      },
                                                      [this] {
                                                          controller.refresh();
                                                      }}},
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
          controller{screens.navigator,
                     chrome,
                     app::SettingsController::Services{
                         paths, shortcutEditor, volume, shortcuts, emulatorSettings, layerSettings,
                         steamLibrarySettings, pluginSettings, chordSettings, mappings, wine,
                         integrationSettings, artworkSettings, inForce},
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
                                  config, steamLibrarySettings.defaultLibrary(), inForce.current());
                          },
                      .say =
                          [this](const std::string& text, bool error) {
                              said.push_back({text, error});
                          }}} {
        screens.frame();
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

    /// A button on the screen, then the next frame.
    void act(Button button) {
        controller.act(button);
        screens.frame();
    }
    /// Focuses the tab of the category called `label`, which chooses it, and goes into its rows.
    void enter(const std::string& label) {
        std::string id;
        for (const ui::SettingsCategory& category : controller.categories()) {
            if (category.label == label) {
                id = category.id;
            }
        }
        expect(!id.empty(), "a category with that label exists");
        expect(screens.navigator.focus("categories/" + id), "its tab takes the focus");
        screens.frame();
        act(Button::Right);
    }
    /// Focuses the widget `id` on the top screen.
    void focus(const std::string& id) {
        expect(screens.navigator.focus(id), "a row with that id takes the focus");
        screens.frame();
    }
    /// The id of the top screen, or empty with none open.
    [[nodiscard]] std::string top() const {
        const gpu2d::ui::Screen* screen = screens.navigator.top();
        return screen == nullptr ? std::string{} : screen->id();
    }
    [[nodiscard]] std::string focused() const {
        return screens.navigator.focused();
    }
    /// The category or page the topmost Settings screen shows.
    [[nodiscard]] std::string pageId() const {
        const std::vector<const gpu2d::ui::Screen*> open = screens.navigator.screens();
        for (auto at = open.rbegin(); at != open.rend(); ++at) {
            if ((*at)->overlay()) {
                continue;
            }
            if (const auto* root = dynamic_cast<const app::SettingsScreen*>(*at)) {
                return root->tabId().substr(std::string_view{"categories/"}.size());
            }
            return (*at)->id();
        }
        return {};
    }
    /// The rows of the category or page shown.
    [[nodiscard]] std::vector<ui::SettingsRow> rows() {
        return controller.page(pageId()).rows;
    }
    [[nodiscard]] ui::SettingsRow rowOf(const std::string& id) {
        for (const ui::SettingsRow& row : controller.page(pageId()).rows) {
            if (row.id == id) {
                return row;
            }
        }
        test::fail("a row with that id exists");
    }
    /// Every integration on, as the pages these checks read need them.
    static app::Preferences& everyIntegration(app::Preferences& preferences) {
        preferences.values().integrations = settings::IntegrationChoices{.steam = true,
                                                                         .steamMode = std::nullopt,
                                                                         .epic = true,
                                                                         .gog = true,
                                                                         .emulators = true,
                                                                         .plugins = true};
        return preferences;
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
    app::IntegrationsInForce inForce{everyIntegration(preferences), false};
    app::IntegrationSettings integrationSettings{inForce, [this] {
                                                     controller.refresh();
                                                 }};
    test::InterfaceRig screens;
    ui::Chrome chrome;
    ui::FolderChooser folders;
    ui::OnScreenKeyboard entry;
    app::PathEditor paths;
    /// SteamGridDB with no API behind it: these checks keep and forget keys, never fetch.
    artwork::SteamGridDb steamGridDb{"http://127.0.0.1:1/api/v2"};
    /// The client Connect signs in with; a desktop Steam unless a test says otherwise.
    test::FakeSteamGate steamGate;
    app::SteamGridDbAccount steamGridDbAccount;
    app::ArtworkSettings artworkSettings;
    int artworkRequests{0};
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
    app::GameSettings gameSettings{
        settings::GameSettingsStore{file.parent_path() / settings::gameSettingsFile}, file,
        [this](const std::string& why) {
            failures.push_back(why);
        }};
    app::EmulatorSettings emulatorSettings{
        downloads, app::EmulatorSettings::Hooks{[this](const std::string& text, bool error) {
            said.push_back({text, error});
        }}};
    app::LayerSettings layerSettings{gameSettings,
                                     [this]() -> const library::roms::Emulators& {
                                         return emulatorSettings.found();
                                     },
                                     downloads.shaderPack(),
                                     app::LayerSettings::Hooks{[] {
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
        config, preferences, true,
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
    app::WineSettings wine{preferences, config, inForce.current()};
    app::SettingsController controller;
};

/// With an integration off its pages and rows are gone; turning it on in Settings > Integrations
/// brings them back at once.
void pagesFollowTheIntegrations(const fs::path& root) {
    Rig rig{root / "integrations"};
    rig.inForce.choose(settings::IntegrationChoices{});
    rig.controller.open();
    std::vector<std::string> names;
    for (const ui::SettingsCategory& category : rig.controller.categories()) {
        names.push_back(category.label);
    }
    expect(names == std::vector<std::string>{"Appearance", "Library", "Integrations", "Emulators",
                                             "Audio", "Controls", "Guide chords", "Install folders",
                                             "Artwork", "About"},
           "no Steam libraries, Wine or Plugins with their integrations off");
    rig.enter("Library");
    expect(std::ranges::none_of(rig.rows(),
                                [](const ui::SettingsRow& row) {
                                    return row.id.starts_with("steam.");
                                }),
           "no Steam folders on the Library page");
    rig.controller.showCategories();
    rig.enter("Install folders");
    expect(rig.rows().size() == 1, "only the default install folder");
    rig.controller.showCategories();
    rig.enter("Integrations");
    rig.focus("integrations.steam");
    rig.act(Button::A);
    const std::optional<settings::IntegrationChoices> staged = rig.onDisk().integrations;
    expect(staged.has_value() && !staged->steam, "a staged switch is not kept until Apply");
    expect(rig.rowOf("integrations.steamMode").value == "Normal", "its mode shows");
    rig.focus("integrations.apply");
    rig.act(Button::A);
    const std::optional<settings::IntegrationChoices> kept = rig.onDisk().integrations;
    expect(kept && kept->steam, "Steam on is kept once applied");
    expect(!rig.said.empty() && !rig.said.back().error &&
               rig.said.back().text == "Integrations: 1 change applied",
           "Apply says how many changes it put in effect");
    const auto& categories = rig.controller.categories();
    expect(std::ranges::any_of(categories,
                               [](const ui::SettingsCategory& category) {
                                   return category.label == "Steam libraries";
                               }),
           "Steam libraries is back");
}

bool saidExactly(const Rig& rig, const std::string& text) {
    return std::ranges::any_of(rig.said, [&text](const Said& said) {
        return !said.error && said.text == text;
    });
}

/// Leaving Settings with Integrations' changes staged asks whether to apply them.
void leavingAsksAboutStagedIntegrations(const fs::path& root) {
    Rig rig{root / "leave"};
    rig.inForce.choose(settings::IntegrationChoices{});
    rig.controller.open();
    rig.enter("Integrations");
    rig.focus("integrations.steam");
    rig.act(Button::A);
    expect(rig.integrationSettings.pendingCount() == 1, "a switch is staged");
    rig.act(Button::B);
    expect(rig.focused() == "categories/integrations", "Back goes to the tabs first");
    rig.act(Button::B);
    expect(rig.top() == "dialog" && rig.focused() == "dialog/0" && rig.controller.isOpen(),
           "Back on the tabs asks first, Apply focused");
    rig.focus("dialog/2");
    rig.act(Button::A);
    expect(rig.top() == "settings" && rig.integrationSettings.pendingCount() == 1,
           "Keep editing stays with the change staged");
    rig.act(Button::Start);
    expect(rig.top() == "dialog", "the menu button asks too");
    rig.act(Button::A);
    const std::optional<settings::IntegrationChoices> kept = rig.onDisk().integrations;
    expect(!rig.controller.isOpen() && kept && kept->steam &&
               rig.integrationSettings.pendingCount() == 0,
           "Apply puts the change in effect and closes Settings");
    expect(saidExactly(rig, "Integrations: 1 change applied"), "and says so");

    rig.controller.open();
    rig.enter("Integrations");
    rig.focus("integrations.steam");
    rig.act(Button::A);
    rig.act(Button::B);
    rig.act(Button::B);
    rig.focus("dialog/1");
    rig.act(Button::A);
    const std::optional<settings::IntegrationChoices> after = rig.onDisk().integrations;
    expect(!rig.controller.isOpen() && after && after->steam &&
               rig.integrationSettings.pendingCount() == 0,
           "Discard drops the staged change and closes Settings");

    rig.controller.open();
    rig.act(Button::B);
    expect(!rig.controller.isOpen(), "with nothing staged Back on the tabs just closes");
}

void categoriesFollowTheSavedSettings(const fs::path& root) {
    Rig rig{root / "categories"};
    rig.recorded = {library::steam::LibraryFolder{"/mnt/games/SteamLibrary", "", {{"10", 2048}}}};
    rig.controller.open();
    std::vector<std::string> names;
    for (const ui::SettingsCategory& category : rig.controller.categories()) {
        names.push_back(category.label);
    }
    expect(names == std::vector<std::string>{"Appearance", "Library", "Integrations", "Emulators",
                                             "Steam libraries", "Audio", "Controls", "Guide chords",
                                             "Install folders", "Artwork", "Wine", "Plugins",
                                             "About"},
           "the thirteen categories");
    expect(rig.controller.isOpen() && rig.top() == "settings" &&
               rig.controller.category() == "Appearance" &&
               rig.focused() == "categories/appearance",
           "opens on Appearance with its tab focused");
    rig.enter("Steam libraries");
    expect(rig.focused().starts_with("categories/"), "a page of facts has no row to focus");
    const auto& rows = rig.rows();
    expect(rows.size() == 2 && rows[0].label == "Steam is not running" &&
               rows[1].kind == ui::RowKind::Info && rows[1].label == "/mnt/games/SteamLibrary" &&
               rows[1].note == "1 game, 2.0 KB" && rows[1].value == "Not mounted",
           "without Steam each recorded library is a read-only row, the page says why");
    rig.controller.close();
    expect(!rig.controller.isOpen() && rig.top().empty() && rig.controller.category().empty(),
           "closed");
}

void togglesChoicesAndSliderPersist(const fs::path& root) {
    Rig rig{root / "persist"};
    rig.controller.open();
    rig.act(Button::A);
    expect(rig.focused() == "layout" && rig.applied == 0,
           "A on a tab goes into its rows and changes nothing");

    rig.focus("pinDock");
    const bool pinned = rig.preferences.values().pinLibraryDock;
    rig.act(Button::A);
    expect(rig.preferences.values().pinLibraryDock != pinned &&
               rig.onDisk().pinLibraryDock != pinned && rig.applied == 1,
           "a toggle flips, is saved and applied");

    rig.focus("iconSize");
    const int size = rig.preferences.values().iconSize;
    rig.act(Button::Right);
    rig.act(Button::Right);
    expect(rig.onDisk().iconSize == size + 2 && rig.rowOf("iconSize").level == size + 2,
           "right grows the icons, kept");
    rig.controller.setLevel("iconSize", 99);
    expect(rig.onDisk().iconSize == ui::maxIconLevel, "a click clamps to the top level");

    rig.focus("scrollMode");
    rig.act(Button::Right);
    expect(rig.onDisk().homeMode.has_value(), "choosing a scroll mode overrides the environment");
    const config::HomeMode first = need(rig.onDisk().homeMode, "a scroll mode");
    rig.act(Button::Right);
    expect(need(rig.onDisk().homeMode, "a scroll mode") != first, "and stepping changes it");

    rig.focus("layout");
    const library::LibraryMode mode = rig.preferences.values().libraryMode;
    const ui::SettingsRow layout = rig.rowOf("layout");
    rig.act(Button::A);
    expect(rig.top() == "choice" && rig.focused() == "choice/" + std::to_string(layout.chosen),
           "A on a choice lists its options, on the one chosen");
    rig.act(Button::B);
    expect(rig.top() == "settings" && rig.preferences.values().libraryMode == mode,
           "Back closes the list and keeps the choice");
    rig.act(Button::A);
    const std::size_t other = (layout.chosen + 1) % layout.choices.size();
    rig.focus("choice/" + std::to_string(other));
    rig.act(Button::A);
    expect(rig.top() == "settings" && rig.rowOf("layout").chosen == other &&
               rig.onDisk().libraryMode != mode,
           "an option picked on the list is chosen and kept");

    rig.act(Button::B);
    expect(rig.focused() == "categories/appearance", "Back from the rows focuses the tabs");
    rig.enter("Audio");
    rig.focus("uiSounds");
    rig.act(Button::A);
    expect(!rig.onDisk().uiSounds, "UI sounds off is kept");
    expect(rig.sounds.muted() || rig.applied > 0, "and applied");

    rig.act(Button::B);
    rig.enter("Library");
    rig.focus("installedOnly");
    rig.act(Button::A);
    expect(rig.onDisk().view.installedOnly, "a view filter is kept");
    rig.focus("source");
    rig.act(Button::Right);
    expect(rig.onDisk().view.source == "launcher:epic", "the source choice is kept");
    expect(rig.failures.empty(), "nothing failed to save");
}

void backAndStartLeave(const fs::path& root) {
    Rig rig{root / "back"};
    rig.controller.open();
    rig.act(Button::A);
    expect(!rig.focused().starts_with("categories/"), "A enters the rows");
    rig.act(Button::B);
    expect(rig.focused() == "categories/appearance" && rig.controller.isOpen(),
           "B leaves the rows for the tabs");
    rig.act(Button::Right);
    expect(!rig.focused().starts_with("categories/"), "Right enters them too");
    rig.controller.showCategories();
    rig.screens.frame();
    expect(rig.focused() == "categories/appearance", "the trail's Settings level goes back");
    rig.act(Button::B);
    expect(!rig.controller.isOpen(), "B on the tabs closes the screen");
    rig.controller.open();
    rig.act(Button::A);
    rig.act(Button::Start);
    expect(!rig.controller.isOpen(), "Start closes it from anywhere");
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
    rig.act(Button::A);
    expect(rig.paths.active(), "A on a folder row opens the chooser");
    choose(rig, library);
    expect(rig.onDisk().installFolders.defaultFolder() == library, "the default is kept");
    expect(rig.onDisk().installFolders.effective(library::Source::Gog) == library,
           "a store follows it");
    expect(rig.rowOf("install.Epic").value == library.string() &&
               rig.rowOf("install.Epic").note == "From the default install folder",
           "the row shows the folder and that it is the default");

    rig.focus("install.Epic");
    rig.act(Button::A);
    choose(rig, epic);
    expect(rig.onDisk().installFolders.effective(library::Source::Epic) == epic &&
               rig.onDisk().installFolders.effective(library::Source::Steam) == library,
           "an override beats the default for its own store only");

    rig.focus("install.Epic");
    rig.act(Button::A);
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
    rig.act(Button::A);

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
    rig.act(Button::A);
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
    rig.act(Button::A);
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
    rig.act(Button::A);
    choose(rig, roms);
    expect(rig.onDisk().romFolders == std::vector<fs::path>{roms}, "a ROM folder is kept");
    rig.focus("roms.add");
    rig.act(Button::A);
    choose(rig, roms);
    expect(rig.onDisk().romFolders.size() == 1, "the same folder is not added twice");
    rig.focus("roms.reset");
    rig.act(Button::A);
    expect(rig.onDisk().romFolders.empty(), "reset goes back to automatic");

    rig.focus("steam.add");
    rig.act(Button::A);
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
    rig.act(Button::A);
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
    rig.act(Button::Right);
    expect(rig.mixerState.percent == 45 && rig.rowOf("volume").level == 45,
           "right raises the volume five percent and the row follows");
    rig.act(Button::Left);
    rig.act(Button::Left);
    expect(rig.mixerState.percent == 35, "left lowers it");
    rig.controller.setLevel("volume", 80);
    expect(rig.mixerState.percent == 80, "a click on the track sets the level");
    rig.controller.setLevel("volume", 400);
    expect(rig.mixerState.percent == 100, "and clamps");
    rig.focus("volumeMute");
    rig.act(Button::A);
    expect(rig.mixerState.muted && rig.rowOf("volumeMute").on, "A mutes");
    expect(!rig.shown.empty() && rig.shown.back().muted, "every change was told to the display");
    const std::size_t told = rig.shown.size();
    rig.act(Button::A);
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
    rig.focus("uiScale");
    expect(rig.rowOf("uiScale").value == "100%", "100% by default");
    rig.act(Button::Right);
    expect(rig.onDisk().uiScale == 105 && rig.rowOf("uiScale").value == "105%", "right grows it");
    rig.act(Button::Left);
    rig.act(Button::Left);
    expect(rig.onDisk().uiScale == 95, "left shrinks it");
    for (int i = 0; i < 40; ++i) {
        rig.act(Button::Left);
    }
    expect(rig.preferences.values().uiScale >= settings::minUiScale &&
               rig.preferences.values().uiScale <= settings::maxUiScale,
           "it wraps inside its range");
    rig.act(Button::Right);
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
    rig.act(Button::A);
    expect(rig.shortcutEditor.capturing() &&
               rig.rowOf("shortcut.quit").value == "Press the new key",
           "A starts listening and the row says so");
    rig.act(Button::A);
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
    rig.act(Button::A);
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
    rig.act(Button::A);
    rig.shortcutEditor.capturePad(
        {event(Button::R2, false), event(Button::R2, true), event(Button::Y, true)});
    expect(rig.shortcutEditor.capturing() && rig.said.back().error, "a clashing chord is refused");
    rig.shortcutEditor.capturePad({event(Button::B, false), event(Button::Y, false),
                                   event(Button::R2, false), event(Button::B, true)});
    expect(rig.shortcutEditor.capturing(), "B may be the start of a chord");
    rig.shortcutEditor.capturePad({event(Button::B, false)});
    expect(!rig.shortcutEditor.capturing(), "B pressed and released alone cancels");

    rig.focus("shortcut.volumeDown");
    rig.act(Button::A);
    rig.shortcutEditor.capturePad({event(Button::Start, true), event(Button::B, true)});
    expect(!rig.shortcutEditor.capturing() && rig.shortcuts.padChord(input::Action::VolumeDown) ==
                                                  input::PadChord{Button::Start, Button::B},
           "any button can be held for a chord, Start too, and B can be its partner");

    rig.focus("shortcut.confirm");
    rig.act(Button::A);
    rig.shortcutEditor.capturePad({event(Button::L2, true), event(Button::Up, true)});
    expect(rig.shortcutEditor.capturing(), "an action that is a button takes no chord");
    rig.shortcutEditor.cancel();

    rig.focus("shortcuts.reset");
    rig.act(Button::A);
    expect(rig.top() == "dialog" && rig.focused() == "dialog/0" &&
               rig.shortcuts.primary(input::Action::Quit) == input::comboNamed("shift+f9"),
           "restoring asks first, Cancel focused, and nothing is restored yet");
    rig.act(Button::B);
    expect(rig.top() == "settings" && rig.focused() == "shortcuts.reset" &&
               rig.shortcuts.primary(input::Action::Quit) == input::comboNamed("shift+f9"),
           "Back cancels the question");
    rig.act(Button::A);
    rig.focus("dialog/1");
    rig.act(Button::A);
    expect(rig.top() == "settings" && rig.onDisk().shortcuts == input::ShortcutOverrides{} &&
               rig.shortcuts.primary(input::Action::Quit) == input::comboNamed("q"),
           "restore puts every shortcut back");
}

void mappingsAreReachedFromControls(const fs::path& root) {
    Rig rig{root / "mappings"};
    rig.controller.open();
    rig.enter("Controls");
    const std::vector<ui::SettingsRow> rows = rig.rows();
    expect(rows.size() > 4 && rows[0].kind == ui::RowKind::Heading &&
               rows[1].id == "mapping.global" && rows[2].id == "mapping.emulators" &&
               rows[2].kind == ui::RowKind::Page && rows[3].id == "mapping.steamInput",
           "the Controls page starts with the mappings and the Steam Input switch");
    expect(std::ranges::none_of(rows,
                                [](const ui::SettingsRow& row) {
                                    return row.id == "chords.open";
                                }),
           "Guide chords is its own category, not a row here");
    rig.focus("mapping.emulators");
    rig.act(Button::A);
    expect(rig.top() == "mapping.emulators" &&
               rig.controller.category() == "Controls › Emulator mappings",
           "A opens the page of emulators' mappings");
    expect(rig.rows().size() == 2 && rig.rowOf("mapping.emulator.mgba").label == "mGBA",
           "one row per emulator");
    rig.focus("mapping.emulator.mgba");
    rig.act(Button::A);
    expect(rig.mappings.active() && rig.mappingPage.title() == "Controls · mGBA",
           "A opens that emulator's mapping");
    rig.mappings.close();
    rig.act(Button::B);
    expect(rig.top() == "settings" && rig.controller.category() == "Controls",
           "Back returns to Controls");
    rig.focus("mapping.steamInput");
    rig.act(Button::A);
    expect(rig.rowOf("mapping.steamInput").on &&
               settings::MappingStore{rig.config.configDir / settings::mappingsFile}
                   .load()
                   .steamInputGlobal(),
           "A turns Steam Input on for every game and keeps it");
    rig.focus("mapping.global");
    rig.act(Button::A);
    expect(rig.mappings.active() && rig.mappingPage.title() == "Controls · every game",
           "A opens the global mapping");
}

void guideChordsAreSetOnTheirPage(const fs::path& root) {
    Rig rig{root / "chords"};
    rig.controller.open();
    rig.enter("Guide chords");
    expect(rig.controller.category() == "Guide chords" && !rig.focused().starts_with("categories/"),
           "Guide chords is a category of its own");

    std::vector<std::string> labels;
    for (const ui::SettingsRow& row : rig.rows()) {
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
    rig.act(Button::A);
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
    rig.act(Button::A);
    rig.shortcutEditor.capturePad({event(Button::Guide, true), event(Button::A, true)});
    expect(rig.shortcutEditor.capturing() &&
               rig.said.back().text.find("already Quick menu") != std::string::npos,
           "a chord another action has is refused");
    rig.shortcutEditor.cancel();

    expect(rig.shortcuts.rebind(input::Action::Quit, input::Combo{input::keys::F9}).empty(),
           "a key is remapped too");
    rig.focus("chords.reset");
    rig.act(Button::A);
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
        return rig.rows().size() == 6;
    });
    expect(rig.rowOf("steamLibraries.library.0").note == "1 game, 1.0 KB · 2.0 KB free of 4.0 KB" &&
               rig.rowOf("steamLibraries.library.1").value == "Default" &&
               rig.rowOf("steamLibraries.library.2").value == "Make default",
           "each library shows its games, size, free space and whether it is the default");
    expect(rig.rowOf("steamLibraries.remove.2").note.find("1 game") != std::string::npos,
           "a library with games says why it cannot be removed");

    rig.focus("steamLibraries.library.2");
    rig.act(Button::A);
    settle(rig, [&rig] {
        return rig.rowOf("steamLibraries.library.2").value == "Default";
    });
    expect(hasCall(steam, "SetDefaultInstallFolder(2)"), "A makes a library the default");
    rig.controller.close();
    rig.controller.openCategory("installs");
    rig.act(Button::Right);
    expect(rig.rowOf("install.Steam").value == "/mnt/games" &&
               rig.rowOf("install.Steam").note.starts_with("Steam's own default"),
           "the Install folders page shows Steam's default library as Steam's own");
    rig.controller.close();
    rig.controller.open();
    rig.enter("Steam libraries");
    settle(rig, [&rig] {
        return rig.rows().size() == 6;
    });

    rig.focus("steamLibraries.remove.2");
    rig.act(Button::A);
    expect(!rig.said.empty() && rig.said.back().error &&
               rig.said.back().text.find("uninstall or move them") != std::string::npos &&
               !hasCall(steam, "RemoveInstallFolder"),
           "removing a library with games is refused with the reason");
    rig.focus("steamLibraries.remove.1");
    rig.act(Button::A);
    expect(rig.top() == "dialog" && rig.focused() == "dialog/0" &&
               !hasCall(steam, "RemoveInstallFolder"),
           "removing an empty library asks first, Cancel focused");
    rig.focus("dialog/1");
    rig.act(Button::A);
    settle(
        rig,
        [&steam] {
            return hasCall(steam, "RemoveInstallFolder(1)");
        },
        "Remove on the question removes the empty library");

    const fs::path added = root / "new-library";
    fs::create_directories(added);
    rig.focus("steamLibraries.add");
    rig.act(Button::A);
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
        rig.act(Button::Right);
        picked.push_back(rig.onDisk().wineRunner);
    }
    expect(picked == std::vector<fs::path>{system, ge, proton, {}},
           "Right steps through every runner found and back to Automatic, each kept");
    rig.act(Button::Left);
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
        rig.act(Button::A);
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
        rig.act(Button::A);
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

/// Services the screen until `done` holds, as the loop does each pass; false after five seconds.
template <class Done> bool serviceUntil(Rig& rig, Done done) {
    for (int pass = 0; pass < 500; ++pass) {
        rig.controller.service();
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    return false;
}

bool saidSomething(const Rig& rig, const std::string& part, bool error) {
    return std::ranges::any_of(rig.said, [&](const Said& said) {
        return said.error == error && said.text.find(part) != std::string::npos;
    });
}

constexpr const char* steamGridDbKey = "0123456789abcdef0123456789abcdef";

/// Settings > Artwork: a key typed in is kept owner-only and handed to the source; Disconnect
/// forgets it; a typed line that is not a key is refused.
void steamGridDbKeyIsEnteredAndForgotten(const fs::path& root) {
    Rig rig{root};
    rig.controller.open();
    rig.enter("Artwork");
    expect(serviceUntil(rig,
                        [&] {
                            return rig.rowOf("artwork.steamgriddb").value == "Not connected";
                        }),
           "with no saved key SteamGridDB is not connected");
    rig.focus("artwork.steamgriddb.enter");
    rig.act(Button::A);
    expect(rig.artworkSettings.typing() && rig.entry.secret(),
           "Enter a key opens the keyboard, showing dots");
    rig.artworkSettings.typeText("not a key");
    rig.artworkSettings.confirm();
    expect(!rig.artworkSettings.typing(), "Done closes the keyboard");
    expect(serviceUntil(rig,
                        [&] {
                            return saidSomething(rig, "not a SteamGridDB key", true);
                        }),
           "a line that is not a key is refused");

    rig.focus("artwork.steamgriddb.enter");
    rig.act(Button::A);
    rig.artworkSettings.typeText(std::string{" "} + steamGridDbKey + "\n");
    rig.artworkSettings.confirm();
    expect(serviceUntil(rig,
                        [&] {
                            return rig.rowOf("artwork.steamgriddb").value == "Connected";
                        }),
           "a pasted key connects, its spaces dropped");
    expect(rig.rowOf("artwork.steamgriddb").note == "Key kept in a private file",
           "the row says where the key is");
    const fs::path kept = rig.config.configDir / "secrets" / "steamgriddb-api-key";
    expect(fs::status(kept).permissions() == (fs::perms::owner_read | fs::perms::owner_write),
           "the key file is the owner's alone");
    expect(rig.steamGridDb.connected(), "the source has the key");
    expect(rig.artworkRequests == 1 && saidSomething(rig, "SteamGridDB connected", false),
           "artwork is asked for again");
    for (const Said& said : rig.said) {
        expect(said.text.find(steamGridDbKey) == std::string::npos, "the key is never shown");
    }

    rig.focus("artwork.steamgriddb.disconnect");
    rig.act(Button::A);
    expect(rig.top() == "dialog" && rig.steamGridDb.connected(),
           "Disconnect asks first and keeps the key meanwhile");
    rig.focus("dialog/1");
    rig.act(Button::A);
    expect(serviceUntil(rig,
                        [&] {
                            return rig.rowOf("artwork.steamgriddb").value == "Not connected";
                        }),
           "Disconnect ends the connection");
    expect(!fs::exists(kept) && !rig.steamGridDb.connected(), "and forgets the key");
    expect(rig.artworkRequests == 2, "artwork is asked for again without it");
}

/// A key kept by an earlier run is read back at start.
void steamGridDbKeyIsReadBack(const fs::path& root) {
    {
        const host::SecretStore secrets{root / "config" / "secrets", false};
        std::string error;
        expect(secrets.save("steamgriddb-api-key", "key", steamGridDbKey, error).has_value(),
               "a key is kept");
    }
    Rig rig{root};
    rig.controller.open();
    rig.enter("Artwork");
    expect(serviceUntil(rig,
                        [&] {
                            return rig.rowOf("artwork.steamgriddb").value == "Connected";
                        }),
           "the kept key connects at start");
    expect(rig.steamGridDb.connected(), "and goes to the source");
}

/// Connect with no Steam to ask opens SteamGridDB's page in the browser and the keyboard.
void steamGridDbWithoutSteamAsksForTheKey(const fs::path& root) {
    Rig rig{root};
    rig.controller.open();
    rig.enter("Artwork");
    expect(serviceUntil(rig,
                        [&] {
                            return rig.rowOf("artwork.steamgriddb").value == "Not connected";
                        }),
           "not connected");
    rig.focus("artwork.steamgriddb.connect");
    rig.act(Button::A);
    expect(serviceUntil(rig,
                        [&] {
                            return rig.artworkSettings.typing();
                        }),
           "without Steam the keyboard opens for the key");
    expect(saidSomething(rig, "Steam is not available", false), "the player hears why");
    rig.artworkSettings.dismiss();
    expect(!rig.artworkSettings.typing(), "Escape closes it");
}

/// Connect through a running Steam: the key is read from SteamGridDB's page in Steam's browser.
void steamGridDbThroughSteam(const fs::path& root) {
    test::FakeSteam* self = nullptr;
    test::FakeSteam steam{[&self](const std::string& expression) {
        if (expression.find("ExecuteSteamURL") != std::string::npos) {
            self->putPage("VIEW", "https://www.steamgriddb.com/login");
        }
        return nlohmann::json(true);
    }};
    self = &steam;
    steam.answerPages([&steam](const std::string& page, const std::string&) -> nlohmann::json {
        if (steam.pageUrl(page) == "https://www.steamgriddb.com/profile/preferences/api") {
            return {{"state", "key"}, {"key", steamGridDbKey}};
        }
        steam.putPage(page, "https://www.steamgriddb.com/profile/preferences/api");
        return {{"state", "navigating"}};
    });
    Rig rig{root, true, steam.port()};
    // Normal: the desktop's Steam, whose DevTools answer.
    rig.steamGate.setOwnsClient(false);
    rig.controller.open();
    rig.enter("Artwork");
    expect(serviceUntil(rig,
                        [&] {
                            return rig.rowOf("artwork.steamgriddb").value == "Not connected";
                        }),
           "not connected");
    rig.focus("artwork.steamgriddb.connect");
    rig.act(Button::A);
    expect(rig.rows().size() == 2 && rig.rowOf("artwork.steamgriddb.cancel").label == "Cancel",
           "while Steam signs in the page offers Cancel");
    expect(serviceUntil(rig,
                        [&] {
                            return rig.rowOf("artwork.steamgriddb").value == "Connected";
                        }),
           "the key read through Steam connects");
    expect(rig.steamGridDb.connected() && steam.closed() == std::vector<std::string>{"VIEW"},
           "the key is handed on and Steam's view closed");
}

/// Connect with openSU's own Steam kept out by a desktop Steam: the reason, not the browser.
void steamGridDbBlockedSteamSaysWhy(const fs::path& root) {
    Rig rig{root, true, 9};
    rig.steamGate.set(launch::SteamState::Blocked);
    rig.controller.open();
    rig.enter("Artwork");
    expect(serviceUntil(rig,
                        [&] {
                            return rig.rowOf("artwork.steamgriddb").value == "Not connected";
                        }),
           "not connected");
    rig.focus("artwork.steamgriddb.connect");
    rig.act(Button::A);
    expect(serviceUntil(rig,
                        [&] {
                            return saidSomething(rig, "running on the desktop", true);
                        }),
           "the player hears that the desktop's Steam is in the way");
    expect(!rig.artworkSettings.typing() &&
               rig.rowOf("artwork.steamgriddb").value == "Not connected",
           "no key is asked for and the default browser stays shut");
}

int main() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "settings_controller";
    fs::remove_all(root);
    categoriesFollowTheSavedSettings(root);
    pagesFollowTheIntegrations(root);
    leavingAsksAboutStagedIntegrations(root);
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
    steamGridDbKeyIsEnteredAndForgotten(root / "steamgriddb");
    steamGridDbKeyIsReadBack(root / "steamgriddb-kept");
    steamGridDbWithoutSteamAsksForTheKey(root / "steamgriddb-no-steam");
    steamGridDbThroughSteam(root / "steamgriddb-steam");
    steamGridDbBlockedSteamSaysWhy(root / "steamgriddb-blocked");
    fs::remove_all(root);
    std::printf("settings_controller: all checks passed\n");
    return 0;
}
