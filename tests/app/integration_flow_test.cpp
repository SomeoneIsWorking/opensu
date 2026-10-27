// The opt-in integrations through the shipping owners on a scratch settings file: the first-run
// card (shown while nothing is chosen, all off, A toggles, Done keeps), Settings > Integrations
// (switches, Steam's mode, no Normal in a login session, Isolated's controllers) and the switch
// that puts a change into effect (the catalog's stores, standalone emulators, Lua plugins, the
// Steam client's mode), with no Steam started.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "audio/sdl_sound_player.hpp"
#include "first_run_chooser.hpp"
#include "focus_follow.hpp"
#include "integration_settings.hpp"
#include "integration_switch.hpp"
#include "integrations_in_force.hpp"
#include "pad_owner.hpp"
#include "plugin_service.hpp"
#include "ui/check.hpp"
#include "ui/integration_chooser.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using gamepad::Button;
using settings::IntegrationChoices;
using settings::SteamUse;
using test::expect;

app::Preferences preferencesIn(const fs::path& dir) {
    fs::remove_all(dir);
    return app::Preferences{settings::Store{dir / "settings.json"}, [](const std::string&) {
                            }};
}

const ui::SettingsRow* rowOf(const ui::SettingsCategory& page, const std::string& id) {
    for (const ui::SettingsRow& row : page.rows) {
        if (row.id == id) {
            return &row;
        }
    }
    return nullptr;
}

void theChooserAsksUntilDone(const fs::path& root) {
    app::Preferences preferences = preferencesIn(root / "chooser");
    app::IntegrationsInForce inForce{preferences, false};
    audio::SdlSoundPlayer sounds;
    ui::IntegrationChooser card;
    int chosen = 0;
    app::FirstRunChooser chooser{card, inForce, sounds, [&chosen] {
                                     ++chosen;
                                 }};
    chooser.openIfUnchosen();
    expect(chooser.active() && card.options().size() == 5, "no choices: the card lists five");
    for (const ui::IntegrationOption& option : card.options()) {
        expect(!option.on && !option.note.empty(), "each is off and says what it does");
    }
    chooser.act(Button::A);
    chooser.act(Button::Down);
    chooser.act(Button::Down);
    chooser.act(Button::A);
    expect(card.options()[0].on && !card.options()[1].on && card.options()[2].on,
           "A toggles the focused option");
    chooser.act(Button::B);
    expect(chooser.active(), "B does not dismiss it: Done is the way on");
    for (int i = 0; i < 5; ++i) {
        chooser.act(Button::Down);
    }
    expect(card.onDone(), "Done is after the options");
    chooser.act(Button::A);
    expect(!chooser.active() && chosen == 1, "Done keeps the choices and closes the card");
    const auto saved = settings::Store{root / "chooser" / "settings.json"}.load().integrations;
    expect(saved && saved->steam && !saved->epic && saved->gog && !saved->emulators &&
               !saved->plugins,
           "the file holds what was chosen");
    expect(inForce.current().steam == SteamUse::Normal && inForce.current().gog,
           "and it is in force");

    app::Preferences again{settings::Store{root / "chooser" / "settings.json"},
                           [](const std::string&) {
                           }};
    app::IntegrationsInForce later{again, false};
    ui::IntegrationChooser secondCard;
    app::FirstRunChooser second{secondCard, later, sounds, [] {
                                }};
    second.openIfUnchosen();
    expect(!second.active(), "once chosen, the card does not come back");
}

void allOffIsAChoice(const fs::path& root) {
    app::Preferences preferences = preferencesIn(root / "all-off");
    app::IntegrationsInForce inForce{preferences, false};
    audio::SdlSoundPlayer sounds;
    ui::IntegrationChooser card;
    app::FirstRunChooser chooser{card, inForce, sounds, [] {
                                 }};
    chooser.openIfUnchosen();
    card.focusItem(card.options().size());
    chooser.act(Button::A);
    expect(inForce.chosen(), "Done with everything off is kept as a choice");
    expect(inForce.current() == settings::Integrations{}, "and nothing is in force");
}

void settingsStagesThenApplies(const fs::path& root) {
    app::Preferences preferences = preferencesIn(root / "page");
    app::IntegrationsInForce inForce{preferences, false};
    int changes = 0;
    app::IntegrationSettings page{inForce, [&changes] {
                                      ++changes;
                                  }};
    ui::SettingsCategory shown = page.page();
    expect(shown.rows.front().value == "Always on", "ROMs and libretro are always on");
    expect(rowOf(shown, "integrations.steamMode") == nullptr, "no mode while Steam is off");
    page.run("integrations.steam", 0);
    shown = page.page();
    const ui::SettingsRow* mode = rowOf(shown, "integrations.steamMode");
    expect(changes == 0 && inForce.current().steam == SteamUse::Off,
           "a switch is staged: nothing is in force until Apply");
    expect(mode != nullptr && mode->kind == ui::RowKind::Choice && mode->value == "Normal",
           "Steam on: its mode is a choice, Normal by default on a desktop");
    expect(rowOf(shown, "integrations.steamPads") == nullptr, "controllers only for Isolated");
    page.run("integrations.steamMode", 1);
    page.run("integrations.steamMode", 1);
    expect(inForce.current().steam == SteamUse::Off, "Left and Right stage the mode");
    shown = page.page();
    const ui::SettingsRow* pads = rowOf(shown, "integrations.steamPads");
    expect(pads != nullptr && pads->kind == ui::RowKind::Choice &&
               pads->value == "openSU's virtual pads",
           "Isolated shows what Steam sees, openSU's virtual pads by default");
    page.run("integrations.steamPads", 1);
    page.run("integrations.epic", 0);
    expect(rowOf(page.page(), "integrations.apply")->value == "Apply",
           "the Apply row says there is something to apply");
    page.run("integrations.apply", 0);
    expect(changes == 1 && inForce.current().steam == SteamUse::Isolated &&
               inForce.current().steamPads == settings::SteamPads::All && inForce.current().epic,
           "Apply keeps every staged choice at once");
    expect(rowOf(page.page(), "integrations.apply")->value == "Saved",
           "and nothing is left to apply");
    page.run("integrations.epic", 1);
    expect(inForce.current().epic, "Left and Right do not flip a switch");
    page.run("integrations.epic", 0);
    page.run("integrations.apply", 0);
    expect(!inForce.current().epic, "A stages the flip, applied on Apply");

    app::Preferences session = preferencesIn(root / "session");
    session.values().integrations = IntegrationChoices{.steam = true,
                                                       .steamMode = settings::SteamMode::Normal,
                                                       .epic = false,
                                                       .gog = false,
                                                       .emulators = false,
                                                       .plugins = false};
    app::IntegrationsInForce sessionForce{session, true};
    app::IntegrationSettings sessionPage{sessionForce, [] {
                                         }};
    const ui::SettingsCategory sessionShown = sessionPage.page();
    const ui::SettingsRow* sessionMode = rowOf(sessionShown, "integrations.steamMode");
    expect(sessionMode != nullptr && sessionMode->value == "Managed" &&
               sessionForce.current().steam == SteamUse::Managed,
           "a login session runs Managed in place of Normal");
    sessionPage.run("integrations.steamMode", 1);
    expect(rowOf(sessionPage.page(), "integrations.steamMode")->value == "Isolated",
           "and offers Managed and Isolated alone");
    sessionPage.run("integrations.steamMode", 1);
    expect(rowOf(sessionPage.page(), "integrations.steamMode")->value == "Managed",
           "wrapping within the offered modes");
    sessionPage.sync();
    expect(rowOf(sessionPage.page(), "integrations.apply")->value == "Saved",
           "sync drops a staged edit that was never applied");
}

/// A provider that only records that it was read.
class CountingProvider final : public library::Provider {
  public:
    explicit CountingProvider(library::Source source) : source_{source} {
    }
    [[nodiscard]] library::Source source() const override {
        return source_;
    }
    [[nodiscard]] std::vector<library::Game> list() override {
        return {};
    }

  private:
    library::Source source_;
};

bool waitFor(const std::function<bool()>& holds) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (std::chrono::steady_clock::now() < until) {
        if (holds()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    return holds();
}

void theSwitchAppliesAChange(const fs::path& root) {
    const fs::path dir = root / "switch";
    app::Preferences preferences = preferencesIn(dir);
    preferences.values().integrations = IntegrationChoices{};
    app::IntegrationsInForce inForce{preferences, false};
    config::Config config;
    config.home = dir / "home";
    config.dataDir = dir / "data";
    config.assetsDir = dir / "assets";
    steam::Client client{steam::Client::Options{.home = config.home,
                                                .executablePath = {},
                                                .session = "opensu-integration-test",
                                                .steamRoots = {},
                                                .mayStart = false,
                                                .devToolsPort = 1}};
    std::vector<library::CatalogStores> built;
    const auto catalogOf = [&built](const library::CatalogStores& stores) {
        built.push_back(stores);
        library::Catalog catalog;
        if (stores.steam) {
            catalog.add(std::make_unique<CountingProvider>(library::Source::Steam));
        }
        if (stores.epic) {
            catalog.add(std::make_unique<CountingProvider>(library::Source::Epic));
        }
        catalog.add(std::make_unique<CountingProvider>(library::Source::Rom));
        return catalog;
    };
    library::CatalogLoader loader{catalogOf(inForce.catalogStores())};
    app::PluginService plugins{config, preferences, inForce.current().plugins,
                               app::PluginService::Hooks{[](const std::string&, bool) {
                                                         },
                                                         [] {
                                                         },
                                                         [] {
                                                         }}};
    library::roms::EmulatorSetup setup;
    setup.standalone = inForce.current().emulators;
    app::EmulatorDownloads emulators{setup,
                                     app::EmulatorDownloads::Hooks{[](const std::string&, bool) {
                                                                   },
                                                                   [] {
                                                                   },
                                                                   [] {
                                                                   }}};
    const input::Shortcuts shortcuts;
    input::ChordSet chords{shortcuts};
    const std::unique_ptr<gamepad::PadSource> pads =
        gamepad::openPadSource(app::PadOwner::padsFor(OPENSU_TEST_SCRATCH, true));
    app::PadOwner padOwner{*pads, chords};
    app::FocusFollow focus{app::FocusFollow::Hooks{.gamePids =
                                                       [] {
                                                           return std::vector<pid_t>{};
                                                       },
                                                   .closeBigPicture =
                                                       [] {
                                                       },
                                                   .takeFocus =
                                                       [] {
                                                       }}};
    int emulatorChanges = 0;
    app::IntegrationSwitch integrationSwitch{
        inForce,
        app::IntegrationSwitch::Targets{client, loader, plugins, emulators, padOwner, *pads, focus},
        app::IntegrationSwitch::Hooks{.catalog = catalogOf,
                                      .steamInstalled =
                                          [] {
                                              return true;
                                          },
                                      .emulatorsChanged =
                                          [&emulatorChanges] {
                                              ++emulatorChanges;
                                          },
                                      .hideSteamWindows =
                                          [](bool) {
                                          },
                                      .steamWindowsWanted =
                                          [] {
                                              return false;
                                          }}};
    integrationSwitch.begin();
    expect(!plugins.active() && !emulators.setup().standalone, "all off: no plugins, no emulators");

    inForce.choose(IntegrationChoices{.steam = true,
                                      .steamMode = settings::SteamMode::Normal,
                                      .epic = true,
                                      .gog = false,
                                      .emulators = true,
                                      .plugins = true});
    integrationSwitch.apply();
    expect(built.size() == 2 && built.back().steam && built.back().epic && !built.back().gog &&
               built.back().standaloneEmulators,
           "the catalog is rebuilt for the stores now on");
    expect(plugins.active() && emulators.setup().standalone && emulatorChanges == 1,
           "plugins run and standalone emulators count");
    expect(waitFor([&client] {
               return client.mode() == steam::Client::Mode::Normal;
           }),
           "the client was moved to Normal");
    expect(client.state() == launch::SteamState::Stopped, "a hidden run attaches to no Steam");

    integrationSwitch.apply();
    expect(built.size() == 2, "applying again with nothing changed rebuilds nothing");

    inForce.choose(IntegrationChoices{});
    integrationSwitch.apply();
    expect(built.size() == 3 && !built.back().steam && !built.back().epic, "everything off again");
    expect(!plugins.active() && !emulators.setup().standalone, "and stopped");
    const auto snapshot = loader.take();
    expect(snapshot && snapshot->sources.size() == 1 &&
               snapshot->sources.front().source == library::Source::Rom,
           "only the ROM library is listed");
}

} // namespace

int main() {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "integration-flow";
    theChooserAsksUntilDone(root);
    allOffIsAChoice(root);
    settingsStagesThenApplies(root);
    theSwitchAppliesAChange(root);
    fs::remove_all(root);
    std::printf("integration_flow: all checks passed\n");
    return 0;
}
