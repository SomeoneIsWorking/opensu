// The opt-in integrations through the shipping owners on a scratch settings file: all off on a
// fresh start, listed on Library's Add page and added through its wizard, Steam's rows in Settings
// (its mode, no Normal in a login session, Isolated's controllers), Remove, and the switch
// that puts a change into effect (the catalog's stores, standalone emulators, Lua plugins, the
// Steam client's mode), with no Steam started.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "gpu2d/ui/navigator.hpp"

#include "audio/sdl_sound_player.hpp"
#include "focus_follow.hpp"
#include "fumarole_sign_in.hpp"
#include "gpu2d/image_loader.hpp"
#include "integration_settings.hpp"
#include "integration_switch.hpp"
#include "integration_wizard.hpp"
#include "integrations_in_force.hpp"
#include "lan/peer_book.hpp"
#include "library_additions.hpp"
#include "pad_owner.hpp"
#include "plugin_service.hpp"
#include "profiles/profile_book.hpp"
#include "save_sharing.hpp"
#include "steam/client_modes.hpp"
#include "support/fake_interface_text.hpp"
#include "support/test_main.hpp"
#include "ui/check.hpp"
#include "ui/screen_images.hpp"
#include "ui/screen_stack.hpp"
#include "ui/setting_row.hpp"

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

void libraryAddListsWhatIsOff(const fs::path& root) {
    app::Preferences preferences = preferencesIn(root / "tiles");
    app::IntegrationsInForce inForce{preferences, false};
    expect(inForce.current() == settings::Integrations{}, "a fresh start has nothing in force");
    test::InterfaceRig screens;
    const ui::Chrome chrome{.width = 1280.0f};
    std::vector<std::string> pressed;
    app::LibraryAdditions additions{
        inForce, screens.navigator, chrome,
        app::LibraryAdditions::Hooks{.addGame =
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
                                     .addIntegration =
                                         [&pressed](std::string_view entry) {
                                             pressed.emplace_back(entry);
                                         }}};
    expect(additions.tiles().size() == 1 && additions.tiles().front().id == "add",
           "Library has one Add tile");
    const auto ids = [&additions] {
        std::vector<std::string> out;
        for (const ui::SettingsRow& row : additions.page().rows) {
            out.push_back(row.kind == ui::RowKind::Heading ? "-" : row.id);
        }
        return out;
    };
    expect(ids() == std::vector<std::string>{"set-up:steam", "set-up:epic", "set-up:gog",
                                             "set-up:emulators", "set-up:plugins", "set-up:lan",
                                             "-", "add-game", "add-game-folder", "add-rom-folder"},
           "nothing chosen: every integration, then Add a game, Add a game folder and Add a ROM "
           "folder under Games, no LAN import while LAN is off");
    for (const std::string id : {"add-game", "add-game-folder", "add-rom-folder", "set-up:gog"}) {
        additions.press("add");
        expect(screens.navigator.top() != nullptr && screens.navigator.top()->id() == "library-add",
               "Add opens the Add to library page");
        additions.run(id, 0);
        expect(screens.navigator.top() == nullptr, "a pick closes the page");
    }
    expect(pressed ==
               std::vector<std::string>{"add game", "add game folder", "add rom folder", "gog"},
           "each row does its own thing; an integration's adds it");
    settings::IntegrationChoices chosen;
    chosen.steam = true;
    chosen.lan = true;
    inForce.choose(chosen);
    const std::vector<std::string> after = ids();
    expect(std::ranges::find(after, "set-up:steam") == after.end() &&
               std::ranges::find(after, "set-up:lan") == after.end() && after.size() == 9,
           "an integration leaves the page once it is on");
    expect(after.back() == "lan-import", "LAN on: Import from a PC on your network is last");
    additions.press("add");
    additions.run("lan-import", 0);
    expect(pressed.back() == "lan import", "and opens the LAN's ROM import");
}

bool waitFor(const std::function<bool()>& holds);

/// A stand-in fumarole whose `auth --qr <png>` writes the picture and signs in, and whose
/// `auth --import` keeps what it reads in `imported` beside it, refusing what has no token.
fs::path fakeFumarole(const fs::path& dir) {
    fs::create_directories(dir);
    const fs::path program = dir / "fumarole";
    const std::string imported = (dir / "imported").string();
    {
        std::ofstream out{program};
        out << "#!/bin/sh\n[ \"$2\" = --qr ] && printf qr > \"$3\"\n"
            << "if [ \"$2\" = --import ]; then cat > '" << imported << "'; grep -q refresh_token '"
            << imported << "' || { echo 'logon refused'; exit 1; }; fi\nexit 0\n";
    }
    fs::permissions(program, fs::perms::owner_all);
    return program;
}

std::string topId(const test::InterfaceRig& screens) {
    const gpu2d::ui::Screen* top = screens.navigator.top();
    return top == nullptr ? std::string{} : top->id();
}

/// The wizard Library's Add page starts: Steam through fumarole or the client, in its mode, and
/// the other integrations added at once.
void wizardAddsIntegrations(const fs::path& root, bool loginSession) {
    app::Preferences preferences = preferencesIn(root / "wizard");
    app::IntegrationsInForce inForce{preferences, loginSession};
    test::InterfaceRig screens;
    gpu2d::ImageLoader loader;
    ui::IconAtlas icons{{}, loader};
    ui::ScreenImages images{loader, icons};
    const ui::Chrome chrome{.width = 1280.0f};
    app::FumaroleSignIn fumarole{fakeFumarole(root / "wizard-bin"), root / "wizard-qr" / "qr.png"};
    std::vector<std::string> toasts;
    std::vector<app::Store> signIns;
    app::IntegrationWizard wizard{
        inForce,
        screens.navigator,
        chrome,
        images,
        fumarole,
        app::IntegrationWizard::Hooks{.add =
                                          [&inForce](const IntegrationChoices& choices) {
                                              inForce.choose(choices);
                                          },
                                      .signIn =
                                          [&signIns](app::Store store) {
                                              signIns.push_back(store);
                                          },
                                      .toast =
                                          [&toasts](const std::string& text, bool) {
                                              toasts.push_back(text);
                                          },
                                      .peers = {},
                                      .askLogin = {}}};
    const bool client = !inForce.modesOffered().empty();

    wizard.start("steam");
    if (client) {
        expect(topId(screens) == "wizard/steam-kind", "Steam asks how its games come");
        wizard.run("wizard.kind.fumarole", 0);
    }
    expect(topId(screens) == "wizard/steam-login", "fumarole asks how to sign in");
    expect(!inForce.choices().steam, "nothing is added before the last step");
    wizard.run("wizard.login.later", 0);
    screens.frame();
    expect(topId(screens).empty() && inForce.current().steam == SteamUse::Fumarole,
           "Sign in later adds Steam through fumarole and closes the steps");
    expect(!inForce.catalogStores().steam && inForce.catalogStores().fumarole,
           "the catalog lists fumarole's games, not the Steam client's");

    inForce.choose({});
    wizard.start("steam");
    if (client) {
        wizard.run("wizard.kind.fumarole", 0);
    }
    wizard.run("wizard.login.qr", 0);
    expect(topId(screens) == "wizard/fumarole-qr", "the QR code shows while fumarole waits");
    expect(waitFor([&] {
               wizard.service();
               screens.frame();
               return inForce.choices().steam;
           }),
           "a signed-in fumarole adds Steam");
    expect(topId(screens).empty() && toasts.back() == "Steam added. Signed in to Steam",
           "and closes the steps");

    if (client) {
        inForce.choose({});
        wizard.start("steam");
        wizard.run("wizard.kind.client", 0);
        if (loginSession || inForce.modesOffered().size() < 2) {
            expect(inForce.choices().steamKind == settings::SteamKind::Client &&
                       inForce.current().steam != SteamUse::Off,
                   "with no mode to choose, the client is added in the default mode");
        } else {
            expect(topId(screens) == "wizard/steam-mode", "the client asks for its mode");
            const ui::SettingsCategory modes = wizard.page(app::IntegrationWizard::Step::Mode);
            expect(rowOf(modes, "wizard.mode.normal") != nullptr &&
                       !rowOf(modes, "wizard.mode.normal")->note.empty(),
                   "each mode says what it means");
            wizard.run("wizard.mode.isolated", 0);
            expect(inForce.current().steam == SteamUse::Isolated && inForce.catalogStores().steam,
                   "a picked mode is in force and the client's library listed");
        }
        screens.frame();
        expect(topId(screens).empty(), "the client's steps close once added");
    }

    wizard.start("lan");
    expect(inForce.choices().lan && toasts.back() == "LAN added", "LAN is added at once");
    wizard.start("epic");
    expect(inForce.choices().epic && signIns == std::vector<app::Store>{app::Store::Epic},
           "Epic is added and its sign-in opens");
}

/// Steam's sign-in copied from a paired openSU: its row on the Login page, the page a copy waits
/// on, a refusal and Back leaving the Login page as it was, and an allowed copy piped into
/// fumarole's `auth --import`, which adds Steam. The LAN is a stand-in that answers as told.
void wizardCopiesASignIn(const fs::path& root) {
    app::Preferences preferences = preferencesIn(root / "copy");
    app::IntegrationsInForce inForce{preferences, false};
    test::InterfaceRig screens;
    gpu2d::ImageLoader loader;
    ui::IconAtlas icons{{}, loader};
    ui::ScreenImages images{loader, icons};
    const ui::Chrome chrome{.width = 1280.0f};
    const fs::path bin = root / "copy-bin";
    fs::remove_all(bin);
    app::FumaroleSignIn fumarole{fakeFumarole(bin), root / "copy-qr" / "qr.png"};
    std::vector<std::string> toasts;
    struct Ask {
        std::string peer;
        std::stop_token cancel;
        app::IntegrationWizard::LoginAnswer answer;
    };
    std::optional<Ask> asked;
    app::IntegrationWizard wizard{
        inForce,
        screens.navigator,
        chrome,
        images,
        fumarole,
        app::IntegrationWizard::Hooks{
            .add =
                [&inForce](const IntegrationChoices& choices) {
                    inForce.choose(choices);
                },
            .signIn =
                [](app::Store) {
                },
            .toast =
                [&toasts](const std::string& text, bool) {
                    toasts.push_back(text);
                },
            .peers =
                [] {
                    return std::vector<lan::Peer>{lan::Peer{"den-id", "Den"}};
                },
            .askLogin =
                [&asked](std::string peer, std::stop_token cancel,
                         app::IntegrationWizard::LoginAnswer answer) {
                    asked = Ask{std::move(peer), std::move(cancel), std::move(answer)};
                    return true;
                }}};
    const auto lastAsk = [&asked]() -> Ask& {
        if (!asked) {
            throw std::runtime_error{"the wizard asked no peer"};
        }
        return *asked;
    };
    const auto toLogin = [&] {
        wizard.start("steam");
        if (!inForce.modesOffered().empty()) {
            wizard.run("wizard.kind.fumarole", 0);
        }
        screens.frame();
    };
    toLogin();
    const ui::SettingsCategory login = wizard.page(app::IntegrationWizard::Step::Login);
    const ui::SettingsRow* copy = rowOf(login, "wizard.login.copy:den-id");
    expect(copy != nullptr && copy->label == "Copy from Den" &&
               login.rows.back().id == "wizard.login.later",
           "the Login page offers a copy from each paired openSU, before Sign in later");

    wizard.run("wizard.login.copy:den-id", 0);
    screens.frame();
    expect(asked && asked->peer == "den-id" && topId(screens) == "wizard/steam-copy",
           "picking it asks Den and waits on a page of its own");
    expect(wizard.page(app::IntegrationWizard::Step::Copy).rows.front().label == "Waiting for Den",
           "which says what it waits for");
    lastAsk().answer(std::nullopt, "its player declined");
    wizard.service();
    screens.frame();
    expect(topId(screens) == "wizard/steam-login" &&
               toasts.back() == "No Steam sign-in from Den: its player declined" &&
               !inForce.choices().steam,
           "a refusal says why and stays on the Login page, adding nothing");

    asked.reset();
    wizard.run("wizard.login.copy:den-id", 0);
    screens.frame();
    expect(asked && !asked->cancel.stop_requested(), "asked again");
    screens.navigator.act(gpu2d::ui::Action::Back);
    screens.frame(1.0);
    expect(lastAsk().cancel.stop_requested() && topId(screens) == "wizard/steam-login",
           "Back stops waiting and goes back to the Login page");

    wizard.run("wizard.login.copy:den-id", 0);
    screens.frame();
    const std::string signIn =
        R"({"account_name":"stand-in","steam_id":"1","refresh_token":"not-a-token"})";
    lastAsk().answer(signIn, {});
    expect(waitFor([&] {
               wizard.service();
               screens.frame();
               return inForce.choices().steam;
           }),
           "an allowed copy signs fumarole in and adds Steam");
    std::ifstream in{bin / "imported"};
    std::string imported;
    std::getline(in, imported);
    expect(imported == signIn, "fumarole's auth --import read the sign-in on its stdin");
    expect(topId(screens).empty() && toasts.back() == "Steam added with Den's sign-in" &&
               inForce.current().steam == SteamUse::Fumarole,
           "and the steps close");

    inForce.choose({});
    toLogin();
    wizard.run("wizard.login.copy:den-id", 0);
    lastAsk().answer(std::string{"not a sign-in"}, {});
    expect(waitFor([&] {
               wizard.service();
               screens.frame();
               return topId(screens) == "wizard/steam-login";
           }),
           "a copy fumarole refuses goes back to the Login page");
    expect(toasts.back() == "Steam sign-in failed: logon refused" && !inForce.choices().steam,
           "saying why, and adds nothing");
}

std::optional<ui::SettingsRow> rowIn(const std::vector<ui::SettingsRow>& rows,
                                     const std::string& id) {
    const auto found = std::ranges::find(rows, id, &ui::SettingsRow::id);
    return found == rows.end() ? std::nullopt : std::optional{*found};
}

/// Steam's rows on a build that offers several modes (Linux's Normal, Managed and Isolated): a
/// pick takes effect at once; Remove asks first.
void steamRowsTakeEffect(const fs::path& root) {
    app::Preferences preferences = preferencesIn(root / "page");
    app::IntegrationsInForce inForce{preferences, false};
    test::InterfaceRig screens;
    int changes = 0;
    app::IntegrationSettings rows{inForce, screens.navigator, [&changes] {
                                      ++changes;
                                  }};
    expect(rows.steamRows().empty(), "no Steam rows while Steam is off");
    inForce.choose(IntegrationChoices{.steam = true, .steamKind = settings::SteamKind::Fumarole});
    expect(rows.steamRows().empty(), "none through fumarole: there is no client to run");
    inForce.choose(IntegrationChoices{.steam = true});
    const std::optional<ui::SettingsRow> mode = rowIn(rows.steamRows(), "integrations.steamMode");
    expect(
        mode && mode->kind == ui::RowKind::Choice && mode->value == "Normal" && !mode->note.empty(),
        "the client: its mode is a choice, Normal by default on a desktop, saying what it means");
    expect(!rowIn(rows.steamRows(), "integrations.steamPads"), "controllers only for Isolated");
    rows.run("integrations.steamMode", 2);
    expect(changes == 1 && inForce.current().steam == SteamUse::Isolated,
           "a picked mode is in force at once");
    const std::optional<ui::SettingsRow> pads = rowIn(rows.steamRows(), "integrations.steamPads");
    expect(pads && pads->value == "openSU's virtual pads",
           "Isolated shows what Steam sees, openSU's virtual pads by default");
    rows.run("integrations.steamPads", 1);
    expect(changes == 2 && inForce.current().steamPads == settings::SteamPads::All,
           "and its pick is in force at once");

    inForce.choose(IntegrationChoices{.steam = true, .emulators = true});
    const ui::SettingsRow remove = app::IntegrationSettings::removeRow("emulators");
    expect(remove.label == "Remove Standalone emulators", "Remove names the integration");
    rows.run(remove.id, 0);
    expect(inForce.current().emulators && screens.navigator.top() != nullptr,
           "Remove asks before it turns anything off");
    screens.frame();
    screens.navigator.act(gpu2d::ui::Action::Accept);
    screens.frame();
    expect(!inForce.current().emulators && inForce.current().steam == SteamUse::Normal,
           "confirmed, only that integration is off");

    app::Preferences session = preferencesIn(root / "session");
    session.values().integrations =
        IntegrationChoices{.steam = true, .steamMode = settings::SteamMode::Normal};
    app::IntegrationsInForce sessionForce{session, true};
    app::IntegrationSettings sessionRows{sessionForce, screens.navigator, [] {
                                         }};
    const std::optional<ui::SettingsRow> sessionMode =
        rowIn(sessionRows.steamRows(), "integrations.steamMode");
    expect(sessionMode && sessionMode->value == "Managed" &&
               sessionForce.current().steam == SteamUse::Managed,
           "a login session runs Managed in place of Normal");
    sessionRows.run("integrations.steamMode", 1);
    expect(sessionForce.current().steam == SteamUse::Isolated,
           "and offers Managed and Isolated alone");
}

/// A build whose Steam has one mode (Normal) offers no choice.
void oneModeHasNoChoice(const fs::path& root) {
    app::Preferences preferences = preferencesIn(root / "one-mode");
    app::IntegrationsInForce inForce{preferences, false};
    test::InterfaceRig screens;
    app::IntegrationSettings rows{inForce, screens.navigator, [] {
                                  }};
    inForce.choose(IntegrationChoices{.steam = true});
    expect(!rowIn(rows.steamRows(), "integrations.steamMode") &&
               inForce.current().steam == SteamUse::Normal,
           "Steam on: one mode has nothing to choose");
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
    steam::Client client{
        steam::Client::Options{.home = config.home,
                               .executablePath = {},
                               .session = "opensu-integration-test",
                               .steamRoots = {},
                               .mayStart = false,
                               .devToolsPort = 1,
                               .containment = opensu::launch::chooseContainment()}};
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
                                                                   },
                                                                   [] {
                                                                       return false;
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
                               .playStatus = {},
                               .close = {},
                               .invited = {},
                               .joinAsked = {},
                               .joinAnswered = {},
                               .frameGenModules = {},
                               .steamLogin = {}}};
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
    libraryAddListsWhatIsOff(root);
    wizardAddsIntegrations(root, false);
    wizardAddsIntegrations(root, true);
    wizardCopiesASignIn(root);
    if (steam::clientModes().size() > 1) {
        steamRowsTakeEffect(root);
    } else {
        oneModeHasNoChoice(root);
    }
    theSwitchAppliesAChange(root);
    fs::remove_all(root);
    std::printf("integration_flow: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
