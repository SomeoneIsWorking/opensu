// The details page's buttons through the controller, and the breadcrumb trail built from the
// shell's state.
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
        library::EmulatorOption{"Eden", true, library::LaunchSpec{"eden", {}}},
        library::EmulatorOption{"Ryujinx", true, library::LaunchSpec{"ryujinx", {}}}};
    game.emulator = "Eden";
    return game;
}

struct Rig {
    ui::DetailsPage page;
    audio::SoundPlayer sounds;
    app::Preferences preferences;
    library::Game game;
    std::vector<std::string> calls;
    app::DetailsController controller;

    explicit Rig(const fs::path& dir)
        : preferences{settings::Store{dir / "settings.json"},
                      [](const std::string&) {
                      }},
          game{rom("rom:/r/Kirby.nsp")},
          controller{page, sounds, preferences,
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
    expect(!rig.page.isOpen() && rig.calls.empty(), "the Back button closes it too");
    rig.controller.act(Button::Select);
    expect(rig.calls.empty(), "Select does nothing on the page");
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
    trail();
    filters();
    std::puts("details_page_flow: ok");
    return 0;
}
