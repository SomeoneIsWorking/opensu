// The Guide menu on the interface toolkit, driven as the shell drives it: its places follow where
// the shell is, each reaches its hook and closes the menu, Right from a place reaches Volume, the
// changes made in place go to their owners, and every step that throws something away asks first.
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "guide_menu_controller.hpp"
#include "host_fakes.hpp"
#include "support/fake_interface_text.hpp"
#include "support/scratch_dir.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using gpu2d::ui::Action;
using Landing = app::GuideMenuController::Landing;
using test::expect;

struct Rig {
    explicit Rig(const app::GuideContext& context = {}, bool mixer = true)
        : scratch{OPENSU_TEST_SCRATCH, "guide-menu"},
          preferences{settings::Store{scratch.path() / "settings.json"},
                      [](const std::string&) {
                      }},
          volume{mixer ? std::make_unique<test::FakeMixer>() : nullptr, {}}, brightness{&light},
          retro{scratch.path() / "retro"},
          controller{ui.navigator,
                     {power, session, volume, brightness, retro, preferences},
                     sounds,
                     app::GuideMenuController::Hooks{
                         .context =
                             [context] {
                                 return context;
                             },
                         .showSection =
                             [this](library::Section section) {
                                 calls.emplace_back(section == library::Section::Home ? "home"
                                                                                      : "library");
                             },
                         .openDevices =
                             [this] {
                                 calls.emplace_back("devices");
                             },
                         .openDownloads =
                             [this] {
                                 calls.emplace_back("downloads");
                             },
                         .openSettings =
                             [this] {
                                 calls.emplace_back("settings");
                             },
                         .closeGame =
                             [this] {
                                 calls.emplace_back("closeGame");
                             },
                         .openGameSettings =
                             [this] {
                                 calls.emplace_back("gameSettings");
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
                             }}} {
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
    /// Confirms the question on top: its action is the second button.
    void confirm() {
        expect(ui.navigator.top() != nullptr && ui.navigator.top()->id() == "dialog",
               "the step asks first");
        expect(ui.navigator.focused() == "dialog/0", "with Cancel focused");
        press(Action::Right);
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
    audio::SdlSoundPlayer sounds;
    app::GuideMenuController controller;
    std::vector<std::string> calls;
    std::vector<std::string> said;
};

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
    expect(rig.calls == std::vector<std::string>{"downloads"} && !rig.controller.isOpen(),
           "a place closes the menu and opens");
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
    rig.press(Action::Accept);
    expect(rig.calls.empty() && rig.controller.isOpen(), "Cancel keeps the game");
    rig.press(Action::Accept);
    rig.confirm();
    expect(rig.calls == std::vector<std::string>{"closeGame"} && !rig.controller.isOpen(),
           "confirmed, the game closes with the menu");
    rig.open();
    rig.choose("guide.gameSettings");
    expect(rig.calls.back() == "gameSettings" && rig.controller.isOpen(),
           "the game's settings open over the menu, which Back returns to");
}

void quickChangesSitBesideThePlaces() {
    Rig rig;
    rig.open();
    rig.press(Action::Right);
    expect(rig.ui.navigator.focused() == "guide.volume", "Right from a place reaches Volume");
    rig.press(Action::Right);
    expect(test::need(rig.volume.system().state(), "a volume").percent == 40 + audio::volumeStep,
           "Right on Volume turns it up a step");
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

} // namespace

int main() {
    homePlacesReachTheirHooks();
    gameStepsAskFirst();
    quickChangesSitBesideThePlaces();
    withoutAMixerVolumeSaysWhy();
    powerAsksAndSaysRefusals();
    sessionInstalledOffersTheSwitch();
    std::printf("guide menu controller: all checks passed\n");
    return 0;
}
