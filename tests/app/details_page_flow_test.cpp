// The details page's buttons through the controller (the two-press Uninstall too), and the
// breadcrumb trail built from the shell's state.
#include "breadcrumb_trail.hpp"
#include "details_controller.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
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
    ui::DetailsPage page;
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
          controller{page,
                     sounds,
                     preferences,
                     gameSettings,
                     downloads,
                     uninstall,
                     update,
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
                         }}} {
    }
};

using gamepad::Button;

void buttonsDoWhatTheyName(const fs::path& dir) {
    Rig rig{dir};
    rig.controller.open();
    expect(rig.page.isOpen(), "A on a game's tile opens its details");
    rig.controller.act(Button::A);
    expect(rig.calls == std::vector<std::string>{"launch rom:/r/Kirby.nsp"} && rig.page.isOpen(),
           "Play launches the page's game and the page stays");
    rig.calls.clear();
    rig.controller.act(Button::Down);
    rig.controller.act(Button::A);
    expect(rig.calls == std::vector<std::string>{"emulator Ryujinx"},
           "A on Emulator picks the next");
    rig.calls.clear();
    rig.controller.act(Button::Left);
    expect(rig.calls == std::vector<std::string>{"emulator Ryujinx"},
           "Left on Emulator steps back, wrapping");
    rig.calls.clear();
    rig.controller.act(Button::Down);
    rig.controller.act(Button::A);
    expect(rig.calls == std::vector<std::string>{"hide"} && !rig.page.isOpen(),
           "Hide hides the game and closes the page");
}

void backClosesWithoutActing(const fs::path& dir) {
    Rig rig{dir};
    rig.controller.open();
    rig.controller.act(Button::B);
    expect(!rig.page.isOpen() && rig.calls.empty(), "B closes the page and does nothing else");
    rig.controller.open();
    rig.controller.act(Button::Down);
    rig.controller.act(Button::Down);
    rig.controller.act(Button::Down);
    rig.controller.act(Button::A);
    expect(rig.calls == std::vector<std::string>{"controls rom:/r/Kirby.nsp"} && rig.page.isOpen(),
           "Controls opens the game's mapping over the page");
    rig.calls.clear();
    rig.controller.act(Button::Down);
    rig.controller.act(Button::A);
    expect(!rig.page.isOpen() && rig.calls.empty(), "the Back button closes it too");
    rig.controller.act(Button::Select);
    expect(rig.calls.empty(), "Select does nothing on the page");
}

void uninstallTakesTwoPresses(const fs::path& dir) {
    Rig rig{dir};
    rig.game = library::Game{};
    rig.game.id = "epic:Fortnite";
    rig.game.title = "Fortnite";
    rig.game.source = library::Source::Epic;
    rig.game.sourceId = "Fortnite";
    rig.game.installed = true;
    rig.controller.open();
    rig.controller.act(Button::Down);
    rig.controller.act(Button::Down);
    expect(rig.page.selected() == ui::DetailsAction::Uninstall, "Uninstall follows Hide");
    rig.controller.act(Button::A);
    expect(rig.calls.empty() && rig.page.armed() &&
               rig.page.confirm() == "legendary deletes the Epic copy" &&
               rig.page.shown(rig.page.focus()) == "Press A again to uninstall",
           "the first press arms it with what goes and removes nothing");
    rig.controller.act(Button::Up);
    expect(!rig.page.armed(), "moving focus lets go of it");
    rig.controller.act(Button::Down);
    rig.controller.act(Button::A);
    rig.controller.act(Button::A);
    expect((rig.calls ==
            std::vector<std::string>{"uninstall epic:Fortnite", "say uninstalling Fortnite"}) &&
               !rig.page.armed() && rig.page.isOpen(),
           "the second press starts the uninstall and the page stays");
    rig.calls.clear();
    rig.refusal = "Fortnite is still installing";
    rig.controller.act(Button::A);
    expect(
        (rig.calls ==
         std::vector<std::string>{"say cannot uninstall Fortnite: Fortnite is still installing"}) &&
            !rig.page.armed(),
        "a refused first press says why and arms nothing");
}

void updateStartsWithOnePress(const fs::path& dir) {
    Rig rig{dir};
    rig.game = library::Game{};
    rig.game.id = "gog:2";
    rig.game.title = "Beta";
    rig.game.source = library::Source::Gog;
    rig.game.sourceId = "2";
    rig.game.installed = true;
    rig.controller.open();
    expect(std::ranges::none_of(rig.page.view().buttons,
                                [](const ui::DetailsButton& button) {
                                    return button.action == ui::DetailsAction::Update;
                                }),
           "a game with no update known has no Update");
    rig.controller.close();
    rig.game.updateAvailable = true;
    rig.controller.open();
    rig.controller.act(Button::Down);
    expect(rig.page.selected() == ui::DetailsAction::Update, "Update follows Play");
    expect(rig.page.view().rows.front().value == "Installed, update available",
           "the status says an update is there");
    rig.controller.act(Button::A);
    expect((rig.calls == std::vector<std::string>{"update gog:2", "say updating Beta"}) &&
               rig.page.isOpen(),
           "one press starts the update and the page stays");
    rig.calls.clear();
    rig.updateRefusal = "Beta is running";
    rig.controller.act(Button::A);
    expect((rig.calls ==
            std::vector<std::string>{"update gog:2", "say cannot update Beta: Beta is running"}),
           "a refused update says why");
}

void winePrefixIsChosenPerGame(const fs::path& dir) {
    Rig rig{dir};
    rig.game = library::Game{};
    rig.game.id = "epic:Fortnite";
    rig.game.title = "Fortnite";
    rig.game.source = library::Source::Epic;
    rig.game.sourceId = "Fortnite";
    rig.game.installed = true;
    rig.game.windowsInstall = true;
    rig.controller.open();
    rig.controller.act(Button::Down);
    expect(rig.page.selected() == ui::DetailsAction::WinePrefix &&
               rig.page.view().buttons[1].value == "Shared",
           "a Windows install follows Play with its prefix, shared");
    rig.controller.act(Button::A);
    expect(
        rig.page.view().buttons[1].value == "Own" &&
            settings::Store{dir / "settings.json"}.load().ownWinePrefixes.contains("epic:Fortnite"),
        "A gives the game its own prefix and keeps that");
    expect(rig.calls == std::vector<std::string>{"say Fortnite gets a Wine prefix of its own"},
           "the player is told");
    rig.controller.act(Button::A);
    expect(rig.page.view().buttons[1].value == "Shared" &&
               settings::Store{dir / "settings.json"}.load().ownWinePrefixes.ids().empty(),
           "a second A shares the prefix again");
}

void refreshFollowsTheGame(const fs::path& dir) {
    Rig rig{dir};
    rig.controller.open();
    rig.game.emulator = "Ryujinx";
    rig.controller.refresh();
    expect(rig.page.view().buttons[1].value == "Ryujinx", "a refresh shows the new emulator");
    rig.game.id = "rom:/gone";
    rig.controller.refresh();
    expect(!rig.page.isOpen(), "a game that left the shelf closes the page");
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
    uninstallTakesTwoPresses(dir);
    updateStartsWithOnePress(dir);
    winePrefixIsChosenPerGame(dir / "wine");
    trail();
    filters();
    std::puts("details_page_flow: ok");
    return 0;
}
