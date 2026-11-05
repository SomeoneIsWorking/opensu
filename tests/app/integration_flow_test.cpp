// The opt-in integrations through the shipping owners on a scratch settings file: all off on a
// fresh start with Library's Set up tiles for them, Settings > Integrations
// (switches, Steam's mode, no Normal in a login session, Isolated's controllers) and the switch
// that puts a change into effect (the catalog's stores, standalone emulators, Lua plugins, the
// Steam client's mode), with no Steam started.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "focus_follow.hpp"
#include "integration_settings.hpp"
#include "integration_switch.hpp"
#include "integrations_in_force.hpp"
#include "library_additions.hpp"
#include "pad_owner.hpp"
#include "plugin_service.hpp"
#include "profiles/profile_book.hpp"
#include "save_sharing.hpp"
#include "steam/client_modes.hpp"
#include "support/fake_interface_text.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
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

void libraryTilesSetUpWhatIsOff(const fs::path& root) {
    app::Preferences preferences = preferencesIn(root / "tiles");
    app::IntegrationsInForce inForce{preferences, false};
    expect(inForce.current() == settings::Integrations{}, "a fresh start has nothing in force");
    std::vector<std::string> pressed;
    const app::LibraryAdditions additions{
        inForce, app::LibraryAdditions::Hooks{.addGame =
                                                  [&pressed] {
                                                      pressed.emplace_back("add game");
                                                  },
                                              .addGameFolder =
                                                  [&pressed] {
                                                      pressed.emplace_back("add game folder");
                                                  },
                                              .addRomFolder =
                                                  [&pressed] {
                                                      pressed.emplace_back("add rom folder");
                                                  },
                                              .importFromLan =
                                                  [&pressed] {
                                                      pressed.emplace_back("lan import");
                                                  },
                                              .setUp =
                                                  [&pressed](const std::string& row) {
                                                      pressed.push_back(row);
                                                  }}};
    const auto ids = [&additions] {
        std::vector<std::string> out;
        for (const library::AddTile& tile : additions.tiles()) {
            out.push_back(tile.id);
        }
        return out;
    };
    expect(ids() == std::vector<std::string>{"add-game", "add-game-folder", "add-rom-folder",
                                             "set-up:steam", "set-up:epic", "set-up:gog",
                                             "set-up:emulators", "set-up:plugins", "set-up:lan"},
           "nothing chosen: Add a game, Add a game folder, Add a ROM folder and a tile per "
           "integration, no LAN import while LAN is off");
    for (const std::string id : {"add-game", "add-game-folder", "add-rom-folder", "set-up:gog"}) {
        additions.press(id);
    }
    expect(pressed == std::vector<std::string>{"add game", "add game folder", "add rom folder",
                                               "integrations.gog"},
           "each tile does its own thing; an integration's opens its switch");
    settings::IntegrationChoices chosen;
    chosen.steam = true;
    chosen.lan = true;
    inForce.choose(chosen);
    const std::vector<std::string> after = ids();
    expect(std::ranges::find(after, "set-up:steam") == after.end() &&
               std::ranges::find(after, "set-up:lan") == after.end() && after.size() == 8,
           "a tile goes once its integration is on");
    expect(after[3] == "lan-import", "LAN on: Import from a PC on your network follows Add a "
                                     "ROM folder");
    additions.press("lan-import");
    expect(pressed.back() == "lan import", "and opens the LAN's ROM import");
}

/// Steam's modes on a build that offers several: Linux's Normal, Managed and Isolated.
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

/// A build whose Steam has one mode (Normal) offers no choice; the switches still stage.
void oneModeStagesThenApplies(const fs::path& root) {
    app::Preferences preferences = preferencesIn(root / "one-mode");
    app::IntegrationsInForce inForce{preferences, false};
    int changes = 0;
    app::IntegrationSettings page{inForce, [&changes] {
                                      ++changes;
                                  }};
    page.run("integrations.steam", 0);
    expect(rowOf(page.page(), "integrations.steamMode") == nullptr,
           "Steam on: one mode has nothing to choose");
    page.run("integrations.epic", 0);
    expect(changes == 0 && inForce.current().steam == SteamUse::Off,
           "a switch is staged: nothing is in force until Apply");
    page.run("integrations.apply", 0);
    expect(changes == 1 && inForce.current().steam == SteamUse::Normal && inForce.current().epic,
           "Apply keeps every staged choice at once, Steam in its one mode");
}

/// A provider that only records that it was read.
class CountingProvider final : public library::Provider {
  public:
    explicit CountingProvider(library::Source source) : source_{source} {
    }
    [[nodiscard]] library::Source source() const override {
        return source_;
    }
    [[nodiscard]] std::vector<library::Game> list(const std::stop_token& /*stop*/) override {
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
    app::FocusFollow focus{
        app::FocusFollow::Hooks{.gamePids =
                                    [] {
                                        return std::vector<opensu::launch::ProcessId>{};
                                    },
                                .closeBigPicture =
                                    [] {
                                    },
                                .takeFocus =
                                    [] {
                                    }}};
    audio::SdlSoundPlayer sounds;
    test::InterfaceRig ui;
    profiles::ProfileBook profiles{dir / "profiles.json"};
    app::SaveSharing sharing{save_sync::SaveSync::Setup{.versionsFile = dir / "save-sync.json",
                                                        .backups = dir,
                                                        .staging = dir / "saves-incoming",
                                                        .runnerSaves = dir / "saves",
                                                        .inUse = {},
                                                        .changed = {},
                                                        .seatOf = {}},
                             save_sync::EpicCloud::Setup{.legendary = "legendary",
                                                         .recordFile = dir / "epic-saves.json",
                                                         .backups = dir,
                                                         .changed = {}},
                             secrets::SecretStore{dir / "secrets", false}, profiles, [] {
                             }};
    app::LanService lan{
        app::LanService::Places{.bookFile = dir / "lan.json",
                                .secrets = secrets::SecretStore{dir / "secrets", false},
                                .coverCache = dir / "lan-covers",
                                .port = 0,
                                .saves = sharing.engine(),
                                .profiles = profiles,
                                .roms = {}},
        preferences,
        ui.navigator,
        sounds,
        std::make_shared<lan::RemoteLibrary>(),
        inForce.current().lan,
        app::LanService::Hooks{.say =
                                   [](const std::string&, bool) {
                                   },
                               .remoteChanged =
                                   [] {
                                   },
                               .peersChanged =
                                   [] {
                                   },
                               .romsChanged = {},
                               .play = {},
                               .playStatus = {}}};
    int emulatorChanges = 0;
    app::IntegrationSwitch integrationSwitch{
        inForce,
        app::IntegrationSwitch::Targets{client, loader, plugins, lan, emulators, padOwner, *pads,
                                        focus},
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
    expect(!plugins.active() && !emulators.setup().standalone && lan.node() == nullptr,
           "all off: no plugins, no emulators, not on the LAN");

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

int main() try {
    const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "integration-flow";
    libraryTilesSetUpWhatIsOff(root);
    if (steam::clientModes().size() > 1) {
        settingsStagesThenApplies(root);
    } else {
        oneModeStagesThenApplies(root);
    }
    theSwitchAppliesAChange(root);
    fs::remove_all(root);
    std::printf("integration_flow: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
