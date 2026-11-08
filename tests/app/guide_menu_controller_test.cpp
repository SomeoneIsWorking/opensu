// The Guide menu on the interface toolkit, driven as the shell drives it: its places follow where
// the shell is, each reaches its hook, a page staying over the menu and the rest closing it, Right
// from a place reaches Volume, the changes made in place go to their owners, every step that throws
// something away asks first, and over Home or Library the rows that shape that page show, kept at
// once.
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/screen.hpp"

#include "guide_menu_controller.hpp"
#include "host_fakes.hpp"
#include "profiles/profile_book.hpp"
#include "settings/settings.hpp"
#include "support/fake_interface_text.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gpu2d::ui::Action;
using Landing = app::GuideMenuController::Landing;
using test::expect;

struct Rig {
    explicit Rig(const app::GuideContext& context = {}, bool mixer = true, bool machine = true)
        : scratch{OPENSU_TEST_SCRATCH, "guide-menu"},
          preferences{settings::Store{scratch.path() / "settings.json"},
                      [](const std::string&) {
                      }},
          volume{mixer ? std::make_unique<test::FakeMixer>() : nullptr, {}}, brightness{&light},
          retro{scratch.path() / "retro"},
          shelf{preferences,
                {[this] {
                     calls.emplace_back("layout");
                 },
                 [this] {
                     calls.emplace_back("reshow");
                 },
                 [] {
                     return std::vector<library::SourceChoice>{
                         {"", "All sources"}, {"launcher:epic", "Epic"}, {"gc", "GameCube"}};
                 }}},
          controller{ui.navigator,
                     {machine ? &power : nullptr, machine ? &session : nullptr, volume, brightness,
                      retro, preferences, shelf, profiles},
                     sounds,
                     app::GuideMenuController::Hooks{
                         .context =
                             [this] {
                                 return current;
                             },
                         .showSection =
                             [this](library::Section section) {
                                 calls.emplace_back(section == library::Section::Home ? "home"
                                                                                      : "library");
                             },
                         .devices = page("devices"),
                         .downloads = page("downloads"),
                         .settings = page("settings"),
                         .openSearch =
                             [this] {
                                 calls.emplace_back("search");
                             },
                         .closeGame =
                             [this] {
                                 calls.emplace_back("closeGame");
                             },
                         .openGameSettings =
                             [this] {
                                 calls.emplace_back("gameSettings");
                             },
                         .keepScreenTurn =
                             [] {
                             },
                         .applyLayout =
                             [this] {
                                 calls.emplace_back("layout");
                             },
                         .quit =
                             [this] {
                                 calls.emplace_back("quit");
                             },
                         .installSession =
                             [this] {
                                 calls.emplace_back("install");
                             },
                         .say =
                             [this](const std::string& text, bool error) {
                                 said.push_back((error ? "error: " : "") + text);
                             },
                         .playAs =
                             [this](const std::string& id) {
                                 calls.push_back("playAs " + id);
                             },
                         .invite =
                             [this](const lan::Peer& peer) {
                                 calls.push_back("invite " + peer.name);
                             },
                         .kick =
                             [this](const std::string& peerId) {
                                 calls.push_back("kick " + peerId);
                             }}},
          current{context} {
    }

    /// `name` is a literal: a closure copied from it cannot throw.
    app::GuideMenuController::Page page(std::string_view name) {
        return {.isOpen =
                    [this, name] {
                        return shown == name;
                    },
                .open =
                    [this, name] {
                        calls.emplace_back(name);
                        shown = name;
                    },
                .close =
                    [this, name] {
                        calls.push_back("close " + std::string{name});
                        shown.clear();
                    }};
    }
    void open(Landing landing = Landing::Places) {
        controller.toggle(landing);
        ui.frame(1.0);
    }
    void press(Action action) {
        ui.navigator.act(action);
        ui.frame(1.0);
    }
    /// Moves the focus down to `id` and presses A.
    void choose(const std::string& id) {
        for (int i = 0; i < 20 && ui.navigator.focused() != id; ++i) {
            press(Action::Down);
        }
        expect(ui.navigator.focused() == id, ("the menu has " + id).c_str());
        press(Action::Accept);
    }
    /// Focuses row `id` of the card.
    void at(std::string_view id) {
        ui.navigator.focus(id);
        ui.frame(1.0);
    }
    /// Confirms the question on top with A: its action is focused.
    void confirm() {
        expect(ui.navigator.top() != nullptr && ui.navigator.top()->id() == "dialog",
               "the step asks first");
        expect(ui.navigator.focused() == "dialog/1", "with its action focused");
        press(Action::Accept);
    }
    [[nodiscard]] std::vector<std::string> places() const {
        std::vector<std::string> ids;
        for (const ui::SettingsRow& row : controller.places()) {
            if (row.kind != ui::RowKind::Heading) {
                ids.push_back(row.id);
            }
        }
        return ids;
    }

    test::ScratchDir scratch;
    test::InterfaceRig ui;
    test::FakePower power;
    test::FakeSessionMode session;
    test::FakeBacklight light;
    app::Preferences preferences;
    app::VolumeControl volume;
    app::BrightnessControl brightness;
    app::RetroSession retro;
    app::ShelfOptions shelf;
    profiles::ProfileBook profiles{scratch.path() / "profiles.json"};
    audio::SdlSoundPlayer sounds;
    app::GuideMenuController controller;
    std::vector<std::string> calls;
    std::vector<std::string> said;
    /// What the hooks say the menu is over now.
    app::GuideContext current;
    /// The page open, over or under the menu.
    std::string shown;
};

/// A page covering the menu.
class CoverPage final : public gpu2d::ui::Screen {
  public:
    CoverPage() : Screen{"cover", "Cover"} {
    }
    [[nodiscard]] std::unique_ptr<gpu2d::ui::Widget>
    build(const gpu2d::ui::Theme& /*theme*/) override {
        return std::make_unique<gpu2d::ui::Column>();
    }
};

void rowsAreReadAgainWhenAPageLeaves() {
    Rig rig{app::GuideContext{.inGame = true, .title = "Roboquest"}};
    const auto hasPlayers = [&rig] {
        const std::vector<std::string> ids = rig.places();
        return std::ranges::find(ids, "guide.players") != ids.end();
    };
    rig.open();
    rig.ui.navigator.push(std::make_unique<CoverPage>());
    rig.ui.frame(1.0);
    rig.controller.service();
    expect(!rig.controller.inFront(), "a page covers the menu");
    rig.current.guests.push_back(lan::Peer{.id = "ada", .name = "Ada"});
    rig.controller.service();
    expect(!hasPlayers(), "under a page the rows are not read");
    rig.ui.navigator.pop();
    rig.ui.frame(1.0);
    rig.controller.service();
    expect(hasPlayers(), "the page gone, they show the guest who joined");
}

void homePlacesReachTheirHooks() {
    Rig rig;
    rig.open();
    expect(rig.controller.isOpen() && rig.ui.navigator.focused() == "guide.home",
           "the menu opens on its first place");
    expect(rig.places() == std::vector<std::string>{"guide.home", "guide.library", "guide.devices",
                                                    "guide.downloads", "guide.settings",
                                                    "guide.power"},
           "over the home screen: the sections, the pages, then Power");
    rig.choose("guide.downloads");
    expect(rig.calls == std::vector<std::string>{"downloads"} && rig.controller.isOpen(),
           "a page opens over the menu");
    rig.controller.close();
    rig.ui.frame(1.0);
    expect(rig.calls.back() == "close downloads" && !rig.controller.isOpen(),
           "closing the menu takes the page it opened with it");
    rig.shown = "settings";
    rig.calls.clear();
    rig.open();
    rig.choose("guide.settings");
    expect(rig.calls.empty() && !rig.controller.isOpen() && rig.shown == "settings",
           "a page open under the menu shows when the menu closes");
    rig.shown.clear();
    rig.open();
    rig.press(Action::Back);
    expect(!rig.controller.isOpen(), "Back closes the menu");
    rig.open();
    rig.controller.toggle(Landing::Places);
    rig.ui.frame(1.0);
    expect(!rig.controller.isOpen(), "Guide again closes it");
}

void gameStepsAskFirst() {
    Rig rig{app::GuideContext{.inGame = true, .title = "Roboquest", .gameSettings = true}};
    rig.open();
    expect(rig.controller.title() == "Roboquest", "the game's title heads the menu");
    expect(rig.places() == std::vector<std::string>{"guide.resume", "guide.closeGame",
                                                    "guide.gameSettings", "guide.devices",
                                                    "guide.downloads", "guide.settings",
                                                    "guide.power"},
           "over a game: Resume, Close game, its settings, then the pages");
    rig.choose("guide.closeGame");
    rig.press(Action::Back);
    expect(rig.calls.empty() && rig.controller.isOpen(), "Back keeps the game");
    rig.press(Action::Accept);
    rig.confirm();
    expect(rig.calls == std::vector<std::string>{"closeGame"} && !rig.controller.isOpen(),
           "confirmed, the game closes with the menu");
    rig.open();
    rig.choose("guide.gameSettings");
    expect(rig.calls.back() == "gameSettings" && rig.controller.isOpen(),
           "the game's settings open over the menu, which Back returns to");
}

void togetherPagesInviteAndKick() {
    Rig rig{app::GuideContext{.inGame = true,
                              .title = "Roboquest",
                              .takesGuests = true,
                              .invitable = {lan::Peer{"attic", "Attic"}},
                              .guests = {lan::Peer{"loft", "Loft"}}}};
    rig.open();
    expect(rig.places() == std::vector<std::string>{"guide.resume", "guide.closeGame",
                                                    "guide.invite", "guide.players",
                                                    "guide.devices", "guide.downloads",
                                                    "guide.settings", "guide.power"},
           "over a game that takes guests: Invite to play, and Players with guests in it");
    rig.choose("guide.invite");
    rig.choose("guide.invite/attic");
    expect(rig.calls == std::vector<std::string>{"invite Attic"} && rig.controller.isOpen(),
           "picking a peer invites it and goes back to the menu");
    rig.choose("guide.players");
    rig.choose("guide.kick/loft");
    expect(rig.calls.size() == 1, "Kick asks first");
    rig.confirm();
    expect(rig.calls.back() == "kick loft" && !rig.controller.isOpen(),
           "confirmed, the guest goes and the menu closes");
}

void quickChangesSitBesideThePlaces() {
    Rig rig;
    rig.open();
    rig.press(Action::Right);
    expect(rig.ui.navigator.focused() == "guide.volume", "Right from a place reaches Volume");
    rig.press(Action::Left);
    expect(rig.ui.navigator.focused() == "guide.home" &&
               test::need(rig.volume.system().state(), "a volume").percent == 40,
           "Left on Volume goes back to the places, the volume left alone");
    rig.press(Action::Right);
    rig.press(Action::Accept);
    rig.press(Action::Right);
    expect(test::need(rig.volume.system().state(), "a volume").percent == 40 + audio::volumeStep,
           "A holds Volume, then Right turns it up a step");
    rig.press(Action::Back);
    expect(test::need(rig.volume.system().state(), "a volume").percent == 40 &&
               rig.ui.navigator.focused() == "guide.volume",
           "Back lets Volume go, putting back the level it had");
    rig.choose("guide.mute");
    expect(test::need(rig.volume.system().state(), "a volume").muted, "A on Mute mutes");
    rig.choose("guide.scale");
    expect(rig.ui.navigator.top() != nullptr && rig.ui.navigator.top()->id() == "choice",
           "A on the interface size lists the sizes");
    rig.press(Action::Back);
    rig.press(Action::Right);
    expect(rig.preferences.values().uiScale == settings::defaultUiScale + settings::uiScaleStep &&
               rig.calls == std::vector<std::string>{"layout"},
           "Right on the interface size steps it and the screen follows");
    rig.press(Action::Back);
    rig.open(Landing::Volume);
    expect(rig.ui.navigator.focused() == "guide.volume", "the quick menu shortcut lands on Volume");
}

void profileRowPlaysAsTheNextProfile() {
    Rig rig;
    std::string error;
    const profiles::Profile ana = test::need(rig.profiles.add("Ana", error), "a second profile");
    rig.open(Landing::Volume);
    for (int i = 0; i < 20 && rig.ui.navigator.focused() != "guide.profile"; ++i) {
        rig.press(Action::Down);
    }
    expect(rig.ui.navigator.focused() == "guide.profile", "the quick card has the profile");
    rig.press(Action::Right);
    expect(rig.calls == std::vector<std::string>{"playAs " + ana.id},
           "Right on Profile plays as the next profile");
}

void pageRowsFollowThePage() {
    const auto ids = [](const Rig& rig) {
        std::vector<std::string> found;
        for (const ui::SettingsRow& row : rig.controller.quick()) {
            if (row.id.starts_with("guide.") || row.kind == ui::RowKind::Heading) {
                continue;
            }
            found.push_back(row.id);
        }
        return found;
    };
    const std::vector<std::string> onHome{"iconSize", "sort", "source", "installedOnly",
                                          "hiddenOnly"};
    const std::vector<std::string> onLibrary{"layout", "iconSize",      "pinDock",   "sort",
                                             "source", "installedOnly", "hiddenOnly"};
    const auto search = [](const Rig& rig) {
        const std::vector<std::string> places = rig.places();
        return std::ranges::find(places, "guide.search") != places.end();
    };

    Rig home{app::GuideContext{.page = library::Section::Home}};
    home.open();
    expect(ids(home) == onHome && search(home), "Home has what does something there, and Search");

    Rig library{app::GuideContext{.page = library::Section::Library}};
    library.open();
    expect(ids(library) == onLibrary && search(library), "Library adds the layout and the pin");

    Rig game{app::GuideContext{.inGame = true, .title = "Game"}};
    game.open();
    expect(ids(game).empty() && !search(game), "over a game none of them show");

    Rig page;
    page.open();
    expect(ids(page).empty() && !search(page), "nor over another screen");

    library.controller.close();
    library.ui.frame(1.0);
    library.open();
    library.choose("guide.search");
    expect(!library.controller.isOpen() && library.calls.back() == "search",
           "Search closes the menu and opens the search");
}

void pageRowsChangeAndKeep() {
    Rig rig{app::GuideContext{.page = library::Section::Library}};
    const app::Preferences& kept = rig.preferences;
    const auto onDisk = [&rig] {
        return settings::Store{rig.scratch.path() / "settings.json"}.load();
    };
    rig.open();
    rig.calls.clear();

    rig.at(app::optionLayoutId);
    rig.press(Action::Right);
    expect(kept.values().libraryMode == library::LibraryMode::Xmb &&
               onDisk().libraryMode == library::LibraryMode::Xmb &&
               rig.calls == std::vector<std::string>{"layout", "reshow"},
           "Right steps the layout, kept and shown at once, the menu still up");
    expect(rig.controller.isOpen(), "the menu stays over the page");

    rig.at(app::optionIconSizeId);
    rig.press(Action::Accept);
    rig.press(Action::Right);
    rig.press(Action::Right);
    expect(onDisk().iconSize == settings::defaultIconSize + 2, "Right grows the icons, kept");
    rig.controller.setLevel(std::string{app::optionIconSizeId}, 99);
    expect(kept.values().iconSize == settings::maxIconSize, "the slider clamps");
    rig.press(Action::Accept);

    rig.at(app::optionPinId);
    rig.press(Action::Accept);
    expect(!onDisk().pinLibraryDock, "A toggles the pin, kept");

    rig.at(app::optionSortId);
    rig.press(Action::Right);
    expect(onDisk().view.sort == library::SortKey::Name, "Right steps the sort, kept");
    rig.press(Action::Left);
    expect(onDisk().view.sort == library::SortKey::Recent, "Left steps back");

    rig.at(app::optionSourceId);
    rig.press(Action::Right);
    expect(onDisk().view.source == "launcher:epic", "Right moves to the next source");
    rig.press(Action::Left);
    rig.press(Action::Left);
    expect(onDisk().view.source == "gc", "Left wraps to the last source");

    rig.at(app::optionInstalledId);
    rig.press(Action::Accept);
    rig.at(app::optionHiddenId);
    rig.press(Action::Accept);
    expect(onDisk().view.installedOnly && !onDisk().view.hiddenOnly,
           "the filters are kept, the hidden filter is not");
    expect(kept.values().view.hiddenOnly, "the hidden filter applies now");
}

void withoutAMixerVolumeSaysWhy() {
    Rig rig{{}, false};
    const std::vector<ui::SettingsRow> quick = rig.controller.quick();
    expect(!quick.empty() && quick.front().id == "guide.volumeMissing" &&
               quick.front().kind == ui::RowKind::Info && !quick.front().note.empty(),
           "an unavailable mixer is a fact saying why");
}

void powerAsksAndSaysRefusals() {
    Rig rig;
    rig.open();
    rig.choose("guide.power");
    expect(rig.ui.navigator.top() != nullptr && rig.ui.navigator.top()->id() == "guide.power",
           "Power opens its list over the menu");
    rig.choose("power.restart");
    rig.power.refusal = "not allowed";
    rig.confirm();
    expect(rig.power.performed == std::vector<host::PowerAction>{host::PowerAction::Restart},
           "confirmed, Restart goes to power");
    expect(!rig.controller.isOpen() && rig.said == std::vector<std::string>{"error: Restart: "
                                                                            "not allowed"},
           "the menu closes and a refusal is said with what was refused");
    rig.open();
    rig.choose("guide.power");
    rig.choose("power.installSession");
    expect(rig.calls == std::vector<std::string>{"install"},
           "Install session mode opens its dialog");
}

void sessionInstalledOffersTheSwitch() {
    Rig rig{app::GuideContext{.inGame = false,
                              .title = {},
                              .gameSettings = false,
                              .loginSession = false,
                              .sessionInstalled = true}};
    rig.open();
    rig.choose("guide.power");
    rig.choose("power.toSession");
    rig.confirm();
    expect(rig.session.calls == std::vector<std::string>{"session"},
           "the switch to session mode asks first, then switches");
    Rig login{app::GuideContext{
        .inGame = false, .title = {}, .gameSettings = false, .loginSession = true}};
    login.open();
    login.choose("guide.power");
    login.choose("power.toDesktop");
    expect(login.session.calls == std::vector<std::string>{"desktop"},
           "as the login session, Switch to desktop ends it");
}

void withoutPowerOrSessionOnlyQuitIsLeft() {
    Rig rig{{}, true, false};
    std::vector<std::string> ids;
    for (const ui::SettingsRow& row : rig.controller.power()) {
        ids.push_back(row.id);
    }
    expect(ids == std::vector<std::string>{"power.quit"},
           "without power or session mode, Power holds Quit alone");
}

} // namespace

int main() try {
    homePlacesReachTheirHooks();
    rowsAreReadAgainWhenAPageLeaves();
    gameStepsAskFirst();
    togetherPagesInviteAndKick();
    quickChangesSitBesideThePlaces();
    profileRowPlaysAsTheNextProfile();
    pageRowsFollowThePage();
    pageRowsChangeAndKeep();
    withoutAMixerVolumeSaysWhy();
    powerAsksAndSaysRefusals();
    sessionInstalledOffersTheSwitch();
    withoutPowerOrSessionOnlyQuitIsLeft();
    std::printf("guide menu controller: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
