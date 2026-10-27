// The details page on the interface toolkit, driven through the navigator as the shell drives it:
// its launch button and rows reach their hooks, Uninstall asks first, the page follows the game,
// and the breadcrumb trail built from the shell's state.
#include "breadcrumb_trail.hpp"
#include "details_controller.hpp"

#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "support/fake_interface_text.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using gpu2d::ui::Action;
using opensu::test::expect;

library::Game rom(const std::string& id) {
    library::Game game;
    game.id = id;
    game.title = "Kirby";
    game.source = library::Source::Rom;
    game.sourceId = "switch";
    game.installed = true;
    game.emulatorOptions = {
        library::EmulatorOption{
            "Eden", true, library::LaunchSpec{"eden", {}}, "eden", false, {}, {}, {}},
        library::EmulatorOption{
            "Ryujinx", true, library::LaunchSpec{"ryujinx", {}}, "ryujinx", false, {}, {}, {}}};
    game.emulator = "Eden";
    return game;
}

struct Rig {
    test::InterfaceRig ui;
    ui::ScreenImages images;
    ui::Chrome chrome;
    audio::SdlSoundPlayer sounds;
    app::Preferences preferences;
    app::GameSettings gameSettings;
    library::Game game;
    std::vector<std::string> calls;
    /// Why `Installs` would refuse the uninstall; empty when it would not.
    std::string refusal;
    app::EmulatorDownloads downloads;
    app::UninstallPrompt uninstall;
    /// Why `Installs` would refuse the update; empty when it would not.
    std::string updateRefusal;
    app::UpdateRequest update;
    app::DetailsController controller;

    explicit Rig(const fs::path& dir)
        : preferences{settings::Store{dir / "settings.json"},
                      [](const std::string&) {
                      }},
          gameSettings{settings::GameSettingsStore{dir / settings::gameSettingsFile},
                       dir / settings::settingsFile,
                       [](const std::string&) {
                       }},
          game{rom("rom:/r/Kirby.nsp")},
          downloads{library::roms::EmulatorSetup{},
                    app::EmulatorDownloads::Hooks{[](const std::string&, bool) {
                                                  },
                                                  [] {
                                                  },
                                                  [] {
                                                  }}},
          uninstall{app::UninstallPrompt::Hooks{[this](const library::Game&) {
                                                    return app::Removal{
                                                        .refusal = refusal,
                                                        .what = "legendary deletes the Epic copy"};
                                                },
                                                [this](const library::Game& removed) {
                                                    calls.push_back("uninstall " + removed.id);
                                                    return std::string{};
                                                },
                                                [this](const std::string& text, bool) {
                                                    calls.push_back("say " + text);
                                                }}},
          update{app::UpdateRequest::Hooks{[this](const library::Game& updated) {
                                               calls.push_back("update " + updated.id);
                                               return updateRefusal;
                                           },
                                           [this](const std::string& text, bool) {
                                               calls.push_back("say " + text);
                                           }}},
          controller{app::DetailsController::Place{ui.navigator, chrome, images}, sounds,
                     app::DetailsController::Services{preferences, gameSettings, downloads,
                                                      uninstall, update},
                     app::DetailsController::Hooks{
                         [this] {
                             return &game;
                         },
                         [this](const std::string& id) {
                             return id == game.id ? std::optional{game} : std::nullopt;
                         },
                         [this](const library::Game& launched) {
                             calls.push_back("launch " + launched.id);
                         },
                         [this](const library::Game&, bool hidden) {
                             calls.push_back(hidden ? "hide" : "show");
                         },
                         [this](const library::Game&, const std::string& name) {
                             calls.push_back("emulator " + name);
                         },
                         [this](const std::string& text, bool) {
                             calls.push_back("say " + text);
                         },
                         [this](const library::Game& opened) {
                             calls.push_back("controls " + opened.id);
                         },
                         [this](const library::Game& opened) {
                             calls.push_back("settings " + opened.id);
                         },
                         [](const library::Game&) {
                             return std::optional<std::size_t>{};
                         },
                         [this](const library::Game& opened) {
                             calls.push_back("artwork " + opened.id);
                         },
                         [this](const library::Game& opened) {
                             calls.push_back("rename " + opened.id);
                         },
                         [this](const library::Game& opened) {
                             calls.push_back("change file " + opened.id);
                         },
                         [this](const library::Game& opened) {
                             calls.push_back("remove " + opened.id);
                         }}} {
    }

    void open() {
        controller.open();
        ui.frame(1.0);
    }
    /// Each action as a press, the stack settled after it.
    void press(std::initializer_list<Action> actions) {
        for (const Action action : actions) {
            ui.navigator.act(action);
            ui.frame(1.0);
        }
    }
    [[nodiscard]] std::string top() const {
        const gpu2d::ui::Screen* screen = ui.navigator.top();
        return screen == nullptr ? std::string{} : screen->id();
    }
    [[nodiscard]] std::string focused() const {
        return ui.navigator.focused();
    }
    [[nodiscard]] const ui::SettingsRow& row(std::string_view id) const {
        const ui::SettingsRow* found = ui::actionOf(*controller.details(), id);
        expect(found != nullptr, "the row is on the page");
        return *found;
    }
};

void buttonsDoWhatTheyName(const fs::path& dir) {
    Rig rig{dir};
    rig.open();
    expect(rig.controller.isOpen() && rig.top() == "details" &&
               rig.focused() == ui::detailsLaunchId,
           "A on a game's tile opens its details on Play");
    rig.press({Action::Accept});
    expect(rig.calls == std::vector<std::string>{"launch rom:/r/Kirby.nsp"} &&
               rig.controller.isOpen(),
           "Play launches the page's game and the page stays");
    rig.calls.clear();
    rig.press({Action::Down});
    expect(rig.focused() == ui::detailsEmulatorId, "the emulator follows Play");
    rig.press({Action::Accept, Action::Down, Action::Accept});
    expect(rig.calls == std::vector<std::string>{"emulator Ryujinx"},
           "A on Emulator lists them, and the one picked is the game's");
    rig.calls.clear();
    rig.press({Action::Left});
    expect(rig.calls == std::vector<std::string>{"emulator Ryujinx"},
           "Left on Emulator steps back, wrapping");
    rig.calls.clear();
    rig.press({Action::Down, Action::Accept});
    expect(rig.calls == std::vector<std::string>{"hide"} && !rig.controller.isOpen(),
           "Hide hides the game and closes the page");
}

void backClosesWithoutActing(const fs::path& dir) {
    Rig rig{dir};
    rig.open();
    rig.press({Action::Back});
    expect(!rig.controller.isOpen() && rig.calls.empty(),
           "B closes the page and does nothing else");
    rig.open();
    rig.press({Action::Down, Action::Down, Action::Down, Action::Accept});
    expect(rig.calls == std::vector<std::string>{"artwork rom:/r/Kirby.nsp"} &&
               rig.controller.isOpen(),
           "Choose artwork opens the cover picker over the page");
    rig.calls.clear();
    rig.press({Action::Down, Action::Accept});
    expect(rig.calls == std::vector<std::string>{"controls rom:/r/Kirby.nsp"} &&
               rig.controller.isOpen(),
           "Controls opens the game's mapping over the page");
    rig.calls.clear();
    rig.press({Action::Down, Action::Accept});
    expect(!rig.controller.isOpen() && rig.calls.empty(), "the Back row closes it too");
}

library::Game fortnite() {
    library::Game game;
    game.id = "epic:Fortnite";
    game.title = "Fortnite";
    game.source = library::Source::Epic;
    game.sourceId = "Fortnite";
    game.installed = true;
    return game;
}

void uninstallAsksFirst(const fs::path& dir) {
    Rig rig{dir};
    rig.game = fortnite();
    rig.open();
    rig.press({Action::Down, Action::Down});
    expect(rig.focused() == ui::detailsUninstallId, "Uninstall follows Hide");
    rig.press({Action::Accept});
    expect(rig.calls.empty() && rig.top() == "dialog" &&
               rig.ui.navigator.top()->title() == "Uninstall Fortnite?" &&
               rig.focused() == "dialog/1",
           "A asks first, its Uninstall focused, and removes nothing");
    rig.press({Action::Back});
    expect(rig.calls.empty() && rig.top() == "details" && rig.focused() == ui::detailsUninstallId,
           "Back keeps the game and the page");
    rig.press({Action::Accept, Action::Accept});
    expect((rig.calls ==
            std::vector<std::string>{"uninstall epic:Fortnite", "say uninstalling Fortnite"}) &&
               rig.top() == "details",
           "A, A starts the uninstall and the page stays");
    rig.calls.clear();
    rig.refusal = "Fortnite is still installing";
    rig.press({Action::Accept});
    expect(
        (rig.calls ==
         std::vector<std::string>{"say cannot uninstall Fortnite: Fortnite is still installing"}) &&
            rig.top() == "details",
        "a refused Uninstall says why and asks nothing");
}

void updateStartsWithOnePress(const fs::path& dir) {
    Rig rig{dir};
    rig.game = library::Game{};
    rig.game.id = "gog:2";
    rig.game.title = "Beta";
    rig.game.source = library::Source::Gog;
    rig.game.sourceId = "2";
    rig.game.installed = true;
    rig.open();
    expect(ui::actionOf(*rig.controller.details(), ui::detailsUpdateId) == nullptr,
           "a game with no update known has no Update");
    rig.controller.close();
    rig.ui.frame(1.0);
    rig.game.updateAvailable = true;
    rig.open();
    rig.press({Action::Down});
    expect(rig.focused() == ui::detailsUpdateId, "Update follows Play");
    rig.press({Action::Accept});
    expect((rig.calls == std::vector<std::string>{"update gog:2", "say updating Beta"}) &&
               rig.controller.isOpen(),
           "one press starts the update and the page stays");
    rig.calls.clear();
    rig.updateRefusal = "Beta is running";
    rig.press({Action::Accept});
    expect((rig.calls ==
            std::vector<std::string>{"update gog:2", "say cannot update Beta: Beta is running"}),
           "a refused update says why");
}

void winePrefixIsChosenPerGame(const fs::path& dir) {
    Rig rig{dir};
    rig.game = fortnite();
    rig.game.windowsInstall = true;
    rig.open();
    rig.press({Action::Down});
    expect(rig.focused() == ui::detailsWinePrefixId &&
               rig.row(ui::detailsWinePrefixId).value == ui::sharedPrefix,
           "a Windows install follows Play with its prefix, shared");
    rig.press({Action::Right});
    expect(
        rig.row(ui::detailsWinePrefixId).value == ui::ownPrefix &&
            settings::Store{dir / "settings.json"}.load().ownWinePrefixes.contains("epic:Fortnite"),
        "Right gives the game its own prefix and keeps that");
    expect(rig.calls == std::vector<std::string>{"say Fortnite gets a Wine prefix of its own"},
           "the player is told");
    rig.press({Action::Right});
    expect(rig.row(ui::detailsWinePrefixId).value == ui::sharedPrefix &&
               settings::Store{dir / "settings.json"}.load().ownWinePrefixes.ids().empty(),
           "again shares the prefix again");
}

void refreshFollowsTheGame(const fs::path& dir) {
    Rig rig{dir};
    rig.open();
    rig.game.emulator = "Ryujinx";
    rig.controller.refresh();
    expect(rig.row(ui::detailsEmulatorId).value == "Ryujinx", "a refresh shows the new emulator");
    expect(rig.controller.details()->title == "Kirby", "the trail reads the page's game");
    rig.game.id = "rom:/gone";
    rig.controller.refresh();
    rig.ui.frame(1.0);
    expect(!rig.controller.isOpen() && rig.controller.details() == nullptr,
           "a game that left the shelf closes the page");
}

void trail() {
    app::TrailState state;
    expect(app::trailOf(state) ==
               ui::Trail{{"Home", ui::CrumbKind::Section, library::Section::Home}},
           "Home is one level");
    state.section = library::Section::Library;
    state.folder = library::Launcher{library::Source::Gog, 3, true, false};
    state.search = "had";
    state.filters = {"Installed"};
    state.game = "Hades";
    const ui::Trail full = app::trailOf(state);
    expect(full.size() == 5 && full[0].label == "Library" && full[1].label == "GOG" &&
               full[2].label == "Search \"had\"" && full[3].label == "Installed" &&
               full[4].label == "Hades",
           "Library, the folder, the search, the filters and the game, in that order");
    expect(full[1].kind == ui::CrumbKind::Folder && full[4].kind == ui::CrumbKind::Game,
           "each level knows what it is");
    state.folder = library::AllGames{10};
    expect(app::trailOf(state)[1].label == "All games", "All games is a folder of Library");
}

void filters() {
    library::ViewOptions options;
    expect(app::filtersOf(options, {}).empty(), "no filter, none named");
    options.installedOnly = true;
    options.hiddenOnly = true;
    options.source = "launcher:epic";
    const std::vector<library::SourceChoice> sources{{"", "All sources"},
                                                     {"launcher:epic", "Epic"}};
    expect((app::filtersOf(options, sources) ==
            std::vector<std::string>{"Epic", "Installed", "Hidden"}),
           "a source is named by its label, then the other filters");
}

} // namespace

int main() {
    const fs::path dir = fs::path{OPENSU_TEST_SCRATCH} / "details-flow";
    fs::remove_all(dir);
    fs::create_directories(dir);
    buttonsDoWhatTheyName(dir);
    backClosesWithoutActing(dir);
    refreshFollowsTheGame(dir);
    uninstallAsksFirst(dir);
    updateStartsWithOnePress(dir);
    winePrefixIsChosenPerGame(dir / "wine");
    trail();
    filters();
    std::puts("details_page_flow: ok");
    return 0;
}
