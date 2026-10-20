// Downloading an emulator from where the player meets its absence, through the shipping job,
// screens and ROM listing, against a fake release host: the details page's Download button with
// its progress, Play working once it is installed, and the Settings screen's Emulators category
// (update check, update, a refused update, two-press Remove, each system's default).
#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#include "details_controller.hpp"
#include "emulator_downloads.hpp"
#include "emulator_settings.hpp"
#include "library/roms.hpp"
#include "support/release_server.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using gamepad::Button;
using test::expect;

constexpr auto wait = std::chrono::seconds{10};

struct Rig {
    explicit Rig(const fs::path& root)
        : root{withRom(root)},
          preferences{settings::Store{root / "settings.json"}, [](const std::string&) {
                      }} {
        server.publish("emu", "v1", 1, "Emu-1.AppImage", "build one");
    }

    /// `root` with a Game Boy Advance ROM in its ROM folder.
    static fs::path withRom(const fs::path& root) {
        fs::create_directories(root / "ROM" / "gba");
        std::ofstream{root / "ROM" / "gba" / "Kirby.gba"} << "rom";
        return root;
    }

    library::roms::EmulatorSetup setup() const {
        library::roms::EmulatorSearch search;
        search.downloads = root / "emulators";
        return library::roms::EmulatorSetup{
            library::roms::EmulatorCatalog{{server.entry({.id = "emu",
                                                          .name = "Emu",
                                                          .system = "gba",
                                                          .asset = "^Emu-[0-9]+\\.AppImage$"})}},
            search,
            {}};
    }

    /// The game as the library lists it now.
    library::Game game() const {
        library::roms::Provider provider{{root / "ROM"}, setup(), library::roms::NameDb{}, {}, {}};
        std::vector<library::Game> games = provider.list();
        preferences.values().emulators.apply(games);
        return games.at(0);
    }

    /// The version of `id` that openSU downloaded, or empty.
    std::string versionInUse(std::string_view id) const {
        const std::optional<app::EmulatorDownloads::Build> record = downloads.downloaded(id);
        return record ? record->version : std::string{};
    }

    /// Hands the loop the job's news until a download ends.
    void settle() {
        const auto until = std::chrono::steady_clock::now() + wait;
        const std::size_t before = said.size();
        while (said.size() == before && std::chrono::steady_clock::now() < until) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            downloads.service();
        }
        expect(said.size() > before, "the download ends");
    }

    fs::path root;
    test::ReleaseServer server;
    app::Preferences preferences;
    std::vector<std::pair<std::string, bool>> said;
    std::vector<std::string> launched;
    int changes{0};
    ui::DetailsPage page;
    audio::SdlSoundPlayer sounds;
    app::EmulatorDownloads downloads{
        setup(), app::EmulatorDownloads::Hooks{[this](const std::string& text, bool error) {
                                                   said.emplace_back(text, error);
                                               },
                                               [this] {
                                                   ++changes;
                                                   emulators.rescan();
                                               },
                                               [this] {
                                                   details.refresh();
                                               }}};
    app::EmulatorSettings emulators{
        downloads, preferences,
        app::EmulatorSettings::Hooks{[this](const std::string& text, bool error) {
                                         said.emplace_back(text, error);
                                     },
                                     [this] {
                                         ++changes;
                                     }}};
    /// ROMs are never uninstalled; the details page here offers no Uninstall.
    app::UninstallPrompt uninstall{app::UninstallPrompt::Hooks{[](const library::Game&) {
                                                                   return app::Removal{};
                                                               },
                                                               [](const library::Game&) {
                                                                   return std::string{};
                                                               },
                                                               [](const std::string&, bool) {
                                                               }}};
    /// ROMs are never updated; the details page here offers no Update.
    app::UpdateRequest update{app::UpdateRequest::Hooks{[](const library::Game&) {
                                                            return std::string{};
                                                        },
                                                        [](const std::string&, bool) {
                                                        }}};
    library::Game shown = game();
    app::DetailsController details{
        page,
        sounds,
        preferences,
        downloads,
        uninstall,
        update,
        app::DetailsController::Hooks{[this] {
                                          return &shown;
                                      },
                                      [this](const std::string&) {
                                          shown = game();
                                          return std::optional{shown};
                                      },
                                      [this](const library::Game& game) {
                                          launched.push_back(game.launch.program);
                                      },
                                      [](const library::Game&, bool) {
                                      },
                                      [](const library::Game&, const std::string&) {
                                      },
                                      [this](const std::string& text, bool error) {
                                          said.emplace_back(text, error);
                                      },
                                      [](const library::Game&) {
                                      }}};
};

const ui::SettingsRow& rowOf(const ui::SettingsCategory& page, const std::string& id) {
    for (const ui::SettingsRow& row : page.rows) {
        if (row.id == id) {
            return row;
        }
    }
    test::fail("a row with that id exists");
}

bool hasRow(const ui::SettingsCategory& page, const std::string& id) {
    for (const ui::SettingsRow& row : page.rows) {
        if (row.id == id) {
            return true;
        }
    }
    return false;
}

void detailsDownloadsTheMissingEmulator(const fs::path& root) {
    Rig rig{root};
    expect(rig.shown.launch.empty() && rig.shown.unavailable.find("Emu") != std::string::npos,
           "a ROM without its emulator cannot start");
    rig.details.open();
    const auto& buttons = rig.page.view().buttons;
    expect(buttons.size() == 5 && buttons[1].action == ui::DetailsAction::DownloadEmulator &&
               buttons[1].label == "Download Emu",
           "the details page offers the missing emulator right there");
    rig.details.act(Button::Down);
    rig.details.act(Button::A);
    expect(rig.downloads.progress().has_value(), "A starts the download in the background");
    const auto until = std::chrono::steady_clock::now() + wait;
    while (!rig.downloads.downloaded("emu") && std::chrono::steady_clock::now() < until) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    // Past the install, so the worker has ended before anything reports it.
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    expect(rig.downloads.progress().has_value(),
           "a finished download still runs until service() reports its end");
    rig.settle();
    expect(!rig.said.back().second && rig.said.back().first == "Emu v1 is installed" &&
               rig.changes == 1,
           "it is installed and the library is asked to read again");
    rig.details.refresh();
    expect(rig.page.view().buttons.size() == 4 && rig.page.selected() == ui::DetailsAction::Launch,
           "the Download button goes and focus is on Play");
    rig.details.act(Button::A);
    expect(rig.launched.size() == 1 && rig.launched[0].ends_with("Emu-1.AppImage"),
           "Play starts the game on the downloaded AppImage without leaving the page");
}

void detailsCancels(const fs::path& root) {
    Rig rig{root};
    rig.server.publish("emu", "v1", 1, "Emu-1.AppImage",
                       std::string(std::size_t{16} * 1024 * 1024, 'x'));
    rig.details.open();
    rig.details.act(Button::Down);
    rig.details.act(Button::A);
    rig.details.act(Button::A);
    rig.settle();
    expect(rig.said.back().first == "Emu download cancelled" && rig.changes == 0 &&
               !rig.downloads.downloaded("emu"),
           "A again cancels, and nothing is installed");
}

void settingsUpdateRemoveAndDefault(const fs::path& root) {
    Rig rig{root};
    ui::SettingsCategory page = rig.emulators.page();
    expect(rowOf(page, "emulators.run.emu").value == "Download" &&
               rowOf(page, "emulators.run.emu").note.starts_with("Not installed"),
           "an emulator that is not here offers Download");
    expect(!hasRow(page, "emulators.remove.emu"), "and nothing to remove");
    rig.emulators.run("emulators.run.emu", 0);
    rig.settle();
    page = rig.emulators.page();
    expect(rowOf(page, "emulators.run.emu").note.starts_with("AppImage (openSU) v1") &&
               hasRow(page, "emulators.remove.emu"),
           "once downloaded it shows its source and version, and can be removed");

    rig.server.publish("emu", "v2", 2, "Emu-2.AppImage", "build two");
    rig.emulators.run("emulators.check", 0);
    const auto until = std::chrono::steady_clock::now() + wait;
    while (!rig.downloads.check().done && std::chrono::steady_clock::now() < until) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    page = rig.emulators.page();
    expect(rowOf(page, "emulators.run.emu").value == "Update to v2" &&
               rowOf(page, "emulators.check").value == "1 update",
           "the check shows the available version and updates nothing");
    expect(rig.versionInUse("emu") == "v1", "v1 is still in use");

    rig.server.publish("emu", "v2", 2, "Emu-2.AppImage", "build two", std::string(64, 'f'));
    rig.emulators.run("emulators.run.emu", 0);
    rig.settle();
    expect(rig.said.back().second && rig.said.back().first.find("SHA-256") != std::string::npos &&
               rig.versionInUse("emu") == "v1",
           "an update whose digest does not match is refused and v1 stays");

    rig.server.publish("emu", "v2", 2, "Emu-2.AppImage", "build two");
    rig.emulators.run("emulators.run.emu", 0);
    rig.settle();
    expect(rig.versionInUse("emu") == "v2" && !fs::exists(root / "emulators" / "emu" / "1"),
           "a verified update is switched to and the old version goes");

    rig.emulators.run("emulators.remove.emu", 0);
    expect(rowOf(rig.emulators.page(), "emulators.remove.emu").value == "Press again to remove" &&
               rig.downloads.downloaded("emu"),
           "Remove asks for a second press");
    rig.emulators.disarm();
    rig.emulators.run("emulators.remove.emu", 0);
    expect(rig.downloads.downloaded("emu").has_value(), "moving away forgets the first press");
    rig.emulators.run("emulators.remove.emu", 0);
    expect(!rig.downloads.downloaded("emu") &&
               !hasRow(rig.emulators.page(), "emulators.remove.emu"),
           "the second press removes it");

    rig.emulators.run("emulators.system.gba", 1);
    expect(rig.preferences.values().emulators.systems().at("gba") == "Emu" &&
               settings::Store{root / "settings.json"}.load().emulators.systems().at("gba") ==
                   "Emu",
           "a system's emulator is picked and kept");
    expect(rig.game().unavailable == "Emu is not installed", "the system's pick reaches its games");
    rig.emulators.run("emulators.system.gba", 1);
    expect(rig.preferences.values().emulators.systems().empty(),
           "stepping on goes back to Automatic");
}

} // namespace

int main() {
    try {
        const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "emulator_flow";
        std::error_code ec;
        fs::remove_all(root, ec);
        detailsDownloadsTheMissingEmulator(root / "details");
        detailsCancels(root / "cancel");
        settingsUpdateRemoveAndDefault(root / "settings");
        fs::remove_all(root, ec);
        std::puts("emulator_flow: ok");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}
