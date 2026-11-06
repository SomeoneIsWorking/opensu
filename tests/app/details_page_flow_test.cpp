// The details page on the interface toolkit, driven through the navigator as the shell drives it:
// its launch button and rows reach their hooks, Uninstall asks first, the page follows the game,
// and the breadcrumb trail built from the shell's state.
#include "breadcrumb_trail.hpp"
#include "details_controller.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/fake_interface_text.hpp"
#include "support/test_main.hpp"
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
            "Eden", true, library::LaunchSpec{"eden", {}}, "eden", false, {}, {}, {}, {}, false},
        library::EmulatorOption{"Ryujinx",
                                true,
                                library::LaunchSpec{"ryujinx", {}},
                                "ryujinx",
                                false,
                                {},
                                {},
                                {},
                                {},
                                false}};
    game.emulator = "Eden";
    return game;
}

struct Rig {
    test::InterfaceRig ui;
    gpu2d::ImageLoader loader;
    ui::ScreenImages images{loader};
    ui::Chrome chrome;
    audio::SdlSoundPlayer sounds;
    app::Preferences preferences;
    app::GameSettings gameSettings;
    library::Game game;
    std::vector<std::string> calls;
    bool onHome{false};
    /// Whether the game's cover is on its way.
    bool artComing{false};
    /// What the Copy saves row reads; empty for no row.
    std::string savesOffer;
    app::SharedSaves::View sharedSaves;
    /// What the last Game settings page opened holds besides its settings.
    std::function<std::vector<ui::SettingsRow>()> settingsRows;
    /// The copy on its way.
    std::string installingId;
    /// The store copies with rows of their own.
    std::vector<library::Game> copies;
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
                             return id == game.id || !copies.empty() ? std::optional{game}
                                                                     : std::nullopt;
                         },
                         [this](const library::Game& launched) {
                             calls.push_back("launch " + launched.id);
                         },
                         [this](const library::Game&, bool hidden) {
                             calls.push_back(hidden ? "hide" : "show");
                         },
                         [this](const library::Game&) {
                             return onHome;
                         },
                         [](const library::Game&) {
                             return false;
                         },
                         [this](const library::Game&, bool on) {
                             calls.push_back(on ? "add home" : "remove home");
                         },
                         [this](const std::string& text, bool) {
                             calls.push_back("say " + text);
                         },
                         [this](const library::Game& opened) {
                             calls.push_back("controls " + opened.id);
                         },
                         [this](const library::Game& opened,
                                std::function<std::vector<ui::SettingsRow>()> rows) {
                             calls.push_back("settings " + opened.id);
                             settingsRows = std::move(rows);
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
                         },
                         [this](const std::string& id) {
                             return id == game.id && artComing;
                         },
                         [this](const library::Game& opened) {
                             calls.push_back("match " + opened.id);
                         },
                         [this](const library::Game& read) {
                             calls.push_back("read " + read.id);
                         },
                         [this](const library::Game&) {
                             return savesOffer;
                         },
                         [this](const library::Game& opened) {
                             calls.push_back("saves " + opened.id);
                         },
                         [this](const library::Game&) {
                             return sharedSaves;
                         },
                         [this](const library::Game& synced) {
                             calls.push_back("sync saves " + synced.id);
                         },
                         [](const library::Game&) {
                             return std::optional<retro::ResumeHeader>{};
                         },
                         [](const library::Game&) {
                             return std::string{};
                         },
                         [] {
                             return std::vector<ui::GameSettingsView::Partner>{};
                         },
                         [](const library::Game&, const std::string&) {
                         },
                         [](const library::Game&) {
                         },
                         [this](const library::Game&) {
                             return copies;
                         },
                         [this](const library::Game& asked) {
                             return asked.id == installingId;
                         }}} {
    }

    /// Opens the page; the saves read it asks for is checked by `copySavesOpens`.
    void open() {
        controller.open();
        ui.frame(1.0);
        std::erase_if(calls, [](const std::string& call) {
            return call.starts_with("read ");
        });
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
        const ui::SettingsRow* found = ui::settingsRowOf(*controller.details(), game.id, id);
        expect(found != nullptr, "the row is on Game settings");
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
    expect(rig.focused() == ui::detailsHomeId, "Add to Home follows Play");
    rig.press({Action::Accept});
    expect(rig.calls == std::vector<std::string>{"add home"} && rig.controller.isOpen(),
           "Add to Home puts the game on Home and the page stays");
    rig.calls.clear();
    rig.press({Action::Down, Action::Accept});
    expect(rig.calls == std::vector<std::string>{"hide"} && !rig.controller.isOpen(),
           "Hide hides the game and closes the page");
}

library::Game storeCopy(library::Source source, const std::string& id, bool installed) {
    library::Game copy;
    copy.id = id;
    copy.title = "Hades";
    copy.source = source;
    copy.installed = installed;
    copy.severalStores = true;
    return copy;
}

void aRowPerStoreCopy(const fs::path& dir) {
    Rig rig{dir};
    rig.copies = {storeCopy(library::Source::Steam, "steam:1", true),
                  storeCopy(library::Source::Epic, "epic:h", true)};
    rig.game = rig.copies.front();
    rig.controller.open();
    rig.ui.frame(1.0);
    expect((rig.calls == std::vector<std::string>{"read steam:1", "read epic:h"}),
           "the page reads each copy's saves");
    rig.calls.clear();
    expect(rig.focused() == ui::detailsLaunchId, "Play is focused");
    rig.press({Action::Accept});
    expect(rig.calls == std::vector<std::string>{"launch steam:1"}, "Play is the top copy's");
    rig.calls.clear();

    rig.press({Action::Down, Action::Down, Action::Down});
    expect(rig.focused() == ui::detailsSettingsId &&
               ui::actionOf(*rig.controller.details(), ui::detailsSettingsId)->label ==
                   "Game settings",
           "one Game settings for the title");
    rig.press({Action::Accept});
    expect(rig.calls == std::vector<std::string>{"settings steam:1"}, "opened on the top copy");
    rig.calls.clear();
    rig.press({Action::Down});
    expect(rig.focused() == ui::detailsMoreId, "More follows it");
    rig.press({Action::Accept});
    expect(rig.top() == "details.more" &&
               rig.focused() == ui::copyRowId("epic:h", ui::detailsLaunchId),
           "More leads with the other copy's Play");
    rig.press({Action::Accept});
    expect(rig.calls == std::vector<std::string>{"launch epic:h"}, "which plays that copy");
    rig.calls.clear();
    rig.press({Action::Down, Action::Down});
    expect(rig.focused() == ui::copyRowId("epic:h", ui::detailsUninstallId),
           "Uninstall of each copy ends More");
    rig.press({Action::Accept});
    expect(rig.ui.navigator.top()->title() == "Uninstall the Epic copy of Hades?",
           "the confirmation names the copy");
    rig.press({Action::Accept});
    expect(rig.calls.front() == "uninstall epic:h", "the row acts on its copy, not the top one");
    rig.calls.clear();
    rig.press({Action::Back});
    expect(rig.top() == "details" && rig.focused() == ui::detailsMoreId,
           "Back returns to the More row");

    rig.game = rig.copies.back();
    rig.copies = {rig.copies.back(), rig.copies.front()};
    rig.controller.refresh();
    expect(rig.controller.details()->launch.gameId == "steam:1",
           "a new top copy moves no row while the page is open");
    rig.press({Action::Back});
    rig.open();
    expect(rig.controller.details()->launch.gameId == "epic:h",
           "it leads when the page opens again");
}

void nothingInstalledLeadsWithInstall(const fs::path& dir) {
    Rig rig{dir};
    rig.copies = {storeCopy(library::Source::Steam, "steam:1", false),
                  storeCopy(library::Source::Epic, "epic:h", false)};
    rig.game = rig.copies.front();
    rig.open();
    expect(rig.focused() == ui::detailsLaunchId &&
               rig.controller.details()->launch.label == "Install (Steam)",
           "the top copy's Install is the primary row");
    rig.press({Action::Accept});
    expect(rig.calls.back() == "launch steam:1", "and behaves as Play does");
    rig.controller.openPage(std::string{ui::detailsMoreId});
    rig.ui.frame(1.0);
    expect(rig.focused() == ui::copyRowId("epic:h", ui::detailsInstallId) &&
               rig.controller.details()->more.size() == 1,
           "More has the other copy's Install, not the top one again");
}

void aStoreSectionFollowsItsInstall(const fs::path& dir) {
    Rig rig{dir};
    rig.copies = {storeCopy(library::Source::Steam, "steam:1", true),
                  storeCopy(library::Source::Epic, "epic:h", false)};
    rig.copies[0].windowsInstall = true;
    rig.copies[1].windowsInstall = true;
    rig.game = rig.copies.front();
    rig.open();
    const auto heading = [&rig](const char* store) {
        const std::vector<ui::SettingsRow>& rows = rig.controller.details()->settingsRows;
        return std::ranges::any_of(rows, [store](const ui::SettingsRow& row) {
            return row.kind == ui::RowKind::Heading && row.label == store;
        });
    };
    expect(heading("Steam") && !heading("Epic"), "only what is installed has a section");
    rig.installingId = "epic:h";
    rig.controller.refresh();
    expect(heading("Epic"), "a copy on its way has one too");
    rig.installingId.clear();
    rig.controller.refresh();
    expect(!heading("Epic"), "and it goes when the install does");
}

void backClosesWithoutActing(const fs::path& dir) {
    Rig rig{dir};
    rig.open();
    rig.press({Action::Back});
    expect(!rig.controller.isOpen() && rig.calls.empty(),
           "B closes the page and does nothing else");
    rig.open();
    rig.controller.openPage(ui::copyRowId(rig.game.id, ui::detailsArtworkId));
    expect(rig.calls == std::vector<std::string>{"artwork rom:/r/Kirby.nsp"} &&
               rig.controller.isOpen(),
           "Choose artwork opens the cover picker over the page");
    rig.calls.clear();
    rig.controller.openPage(ui::copyRowId(rig.game.id, ui::detailsControlsId));
    expect(rig.calls == std::vector<std::string>{"controls rom:/r/Kirby.nsp"} &&
               rig.controller.isOpen(),
           "Controls opens the game's mapping over the page");
    rig.calls.clear();
    rig.press({Action::Down, Action::Down, Action::Down, Action::Down, Action::Accept});
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
    rig.press({Action::Down, Action::Down, Action::Down, Action::Down, Action::Accept});
    expect(rig.top() == "details.more" && rig.focused() == ui::detailsUninstallId,
           "a one-store game's Uninstall is on More too");
    rig.press({Action::Accept});
    expect(rig.calls.empty() && rig.top() == "dialog" &&
               rig.ui.navigator.top()->title() == "Uninstall Fortnite?" &&
               rig.focused() == "dialog/1",
           "A asks first, its Uninstall focused, and removes nothing");
    rig.press({Action::Back});
    expect(rig.calls.empty() && rig.top() == "details.more" &&
               rig.focused() == ui::detailsUninstallId,
           "Back keeps the game and the page");
    rig.press({Action::Accept, Action::Accept});
    expect((rig.calls ==
            std::vector<std::string>{"uninstall epic:Fortnite", "say uninstalling Fortnite"}) &&
               rig.top() == "details.more",
           "A, A starts the uninstall and the page stays");
    rig.calls.clear();
    rig.refusal = "Fortnite is still installing";
    rig.press({Action::Accept});
    expect(
        (rig.calls ==
         std::vector<std::string>{"say cannot uninstall Fortnite: Fortnite is still installing"}) &&
            rig.top() == "details.more",
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
    const std::string row = ui::copyRowId(rig.game.id, ui::detailsWinePrefixId);
    rig.controller.openPage(ui::actionOf(*rig.controller.details(), ui::detailsSettingsId)->id);
    expect(rig.row(ui::detailsWinePrefixId).value == ui::sharedPrefix,
           "Game settings holds a Windows install's prefix, shared");
    rig.calls.clear();
    rig.controller.run(row, 1);
    expect(
        rig.row(ui::detailsWinePrefixId).value == ui::ownPrefix &&
            settings::Store{dir / "settings.json"}.load().ownWinePrefixes.contains("epic:Fortnite"),
        "stepping gives the game its own prefix and keeps that");
    expect(rig.calls == std::vector<std::string>{"say Fortnite gets a Wine prefix of its own"},
           "the player is told");
    rig.controller.run(row, 1);
    expect(rig.row(ui::detailsWinePrefixId).value == ui::sharedPrefix &&
               settings::Store{dir / "settings.json"}.load().ownWinePrefixes.ids().empty(),
           "again shares the prefix again");
}

void refreshFollowsTheGame(const fs::path& dir) {
    Rig rig{dir};
    rig.open();
    rig.game.emulator = "Ryujinx";
    rig.controller.refresh();
    expect(ui::factOf(*rig.controller.details(), "Emulator")->value.starts_with("Ryujinx"),
           "a refresh shows the new emulator");
    expect(rig.controller.details()->title == "Kirby", "the trail reads the page's game");
    rig.game.id = "rom:/gone";
    rig.controller.refresh();
    rig.ui.frame(1.0);
    expect(!rig.controller.isOpen() && rig.controller.details() == nullptr,
           "a game that left the shelf closes the page");
}

void coverSpinnerFollowsTheGame(const fs::path& dir) {
    Rig rig{dir};
    rig.open();
    expect(!rig.controller.artDownloading(), "a cover not on its way shows no spinner");
    rig.artComing = true;
    rig.ui.frame(1.0);
    expect(rig.controller.artDownloading(), "a cover on its way shows the spinner");
    rig.controller.close();
    rig.ui.frame(2.0);
    expect(!rig.controller.artDownloading(), "a closed page waits for nothing");
}

void copySavesOpens(const fs::path& dir) {
    Rig rig{dir};
    rig.controller.open();
    expect(rig.calls == std::vector<std::string>{"read rom:/r/Kirby.nsp"},
           "opening the page reads the game's saves again");
    rig.calls.clear();
    expect(ui::settingsRowOf(*rig.controller.details(), rig.game.id, ui::detailsSavesId) == nullptr,
           "no Copy saves row while the read offers no copy");
    rig.savesOffer = "Eden, Ryujinx";
    rig.controller.refresh();
    const ui::SettingsRow& row = rig.row(ui::detailsSavesId);
    expect(row.label == "Copy saves" && row.value == "Eden, Ryujinx",
           "the row names the emulators keeping a save");
    rig.controller.openPage(row.id);
    expect(rig.calls == std::vector<std::string>{"saves rom:/r/Kirby.nsp"},
           "it opens Copy saves for the page's game");
}

void sharedSavesShow(const fs::path& dir) {
    Rig rig{dir};
    rig.open();
    expect(ui::factOf(*rig.controller.details(), "Saves") == nullptr &&
               ui::settingsRowOf(*rig.controller.details(), rig.game.id, ui::detailsSyncSavesId) ==
                   nullptr,
           "no Saves fact or row while no openSU shares the game's saves");
    rig.sharedSaves = app::SharedSaves::View{"Conflict with Desk", "Pick which save to keep"};
    rig.controller.refresh();
    const ui::SettingsRow* fact = ui::factOf(*rig.controller.details(), "Saves");
    expect(fact != nullptr && fact->value == "Conflict with Desk",
           "the Saves fact says how they stand");
    expect(rig.row(ui::detailsSyncSavesId).label == "Pick which save to keep",
           "and the row what it does");
    rig.controller.run(rig.row(ui::detailsSyncSavesId).id, 0);
    expect(rig.calls == std::vector<std::string>{"sync saves rom:/r/Kirby.nsp"},
           "the row acts on the page's game");
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

int main() try {
    const fs::path dir = fs::path{OPENSU_TEST_SCRATCH} / "details-flow";
    fs::remove_all(dir);
    fs::create_directories(dir);
    buttonsDoWhatTheyName(dir);
    aRowPerStoreCopy(dir / "store");
    nothingInstalledLeadsWithInstall(dir / "nothing-installed");
    aStoreSectionFollowsItsInstall(dir / "store-section");
    backClosesWithoutActing(dir);
    refreshFollowsTheGame(dir);
    coverSpinnerFollowsTheGame(dir);
    copySavesOpens(dir);
    sharedSavesShow(dir);
    uninstallAsksFirst(dir);
    updateStartsWithOnePress(dir);
    winePrefixIsChosenPerGame(dir / "wine");
    trail();
    filters();
    std::puts("details_page_flow: ok");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
