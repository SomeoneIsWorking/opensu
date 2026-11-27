// The Guide menu on the interface toolkit, driven as the shell drives it: its places follow where
// the shell is, each reaches its hook, a page staying over the menu and the rest closing it, it
// holds places and actions only, and every step that throws something away asks first.
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
#include "support/fake_interface_text.hpp"
#include "support/scratch_dir.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gpu2d::ui::Action;
using test::expect;

struct Rig {
    explicit Rig(const app::GuideContext& context = {}, bool machine = true)
        : scratch{OPENSU_TEST_SCRATCH, "guide-menu"}, retro{scratch.path() / "retro"},
          controller{ui.navigator,
                     {machine ? &power : nullptr, machine ? &session : nullptr, retro},
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
                         .openKeyboard =
                             [this] {
                                 calls.emplace_back("keyboard");
                             },
                         .keepScreenTurn =
                             [] {
                             },
                         .installSession =
                             [this] {
                                 calls.emplace_back("install");
                             },
                         .quit =
                             [this] {
                                 calls.emplace_back("quit");
                             },
                         .say =
                             [this](const std::string& text, bool error) {
                                 said.push_back((error ? "error: " : "") + text);
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
    void open() {
        controller.toggle();
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
    app::RetroSession retro;
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
    expect(rig.places() == std::vector<std::string>{"guide.home", "guide.library", "guide.search",
                                                    "guide.devices", "guide.downloads",
                                                    "guide.settings", "guide.power"},
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
    rig.controller.toggle();
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
    expect(rig.calls.back() == "gameSettings" && !rig.controller.isOpen(),
           "Game settings closes the menu and opens Quick settings on the game");
}

void keyboardTypesIntoTheGame() {
    Rig rig{app::GuideContext{.inGame = true, .title = "Roboquest", .typesIntoGame = true}};
    rig.open();
    expect(rig.places().size() > 1 && rig.places()[1] == "guide.keyboard",
           "where it can type into the game, Keyboard follows Resume");
    rig.choose("guide.keyboard");
    expect(rig.calls == std::vector<std::string>{"keyboard"} && !rig.controller.isOpen(),
           "Keyboard closes the menu and opens the keyboard over the game");
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

void theGuideHoldsPlacesAndActionsOnly() {
    Rig rig;
    rig.open();
    rig.press(Action::Right);
    expect(rig.ui.navigator.focused() == "guide.home",
           "beside the places there is nothing to move to: changes live in Quick settings");
    expect(std::ranges::all_of(rig.places(),
                               [](const std::string& id) {
                                   return id.starts_with("guide.");
                               }),
           "every row is a place or an action");
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
    const std::vector<ui::SettingsRow> loginPower = login.controller.power();
    expect(
        std::ranges::none_of(loginPower,
                             [](const ui::SettingsRow& row) {
                                 return row.id == "power.quit";
                             }),
        "as the login session there is no Quit openSU: the display manager would start it again");
}

void quitAsksFirst() {
    Rig rig;
    rig.open();
    rig.choose("guide.power");
    rig.choose("power.quit");
    expect(rig.calls.empty(), "Quit openSU asks before it closes anything");
    rig.confirm();
    expect(rig.calls == std::vector<std::string>{"quit"} && !rig.controller.isOpen(),
           "confirmed, Quit openSU closes the menu and quits");
}

void withoutPowerOrSessionPowerOnlyQuits() {
    Rig rig{{}, false};
    rig.open();
    const std::vector<ui::SettingsRow> power = rig.controller.power();
    expect(power.size() == 1 && power.front().id == "power.quit" &&
               std::ranges::count(rig.places(), "guide.power") == 1,
           "without power or session mode, Power holds Quit openSU alone");
}

} // namespace

int main() try {
    homePlacesReachTheirHooks();
    rowsAreReadAgainWhenAPageLeaves();
    gameStepsAskFirst();
    keyboardTypesIntoTheGame();
    togetherPagesInviteAndKick();
    theGuideHoldsPlacesAndActionsOnly();
    powerAsksAndSaysRefusals();
    sessionInstalledOffersTheSwitch();
    withoutPowerOrSessionPowerOnlyQuits();
    quitAsksFirst();
    std::printf("guide menu controller: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
