// A built-in emulator's core downloaded from where the player meets its absence, through the
// shipping job, screens and ROM listing, against a fake buildbot: the details page's Download
// button, Play starting opensu-retro on the core, and Settings › Emulators showing the buildbot
// as its source, the update check, an update and the two-press Remove.
#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "details_controller.hpp"
#include "emulator_downloads.hpp"
#include "emulator_settings.hpp"
#include "library/roms.hpp"
#include "support/buildbot_server.hpp"
#include "support/scratch_dir.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using gamepad::Button;
using test::expect;

constexpr auto wait = std::chrono::seconds{10};
constexpr std::string_view gbInfo =
    "firmware_count = 1\nfirmware0_path = \"gb_bios.bin\"\n"
    "firmware0_desc = \"Game Boy BIOS\"\nfirmware0_opt = \"true\"\n";

struct Rig {
    explicit Rig(const fs::path& root)
        : root{prepared(root)},
          preferences{settings::Store{root / "settings.json"}, [](const std::string&) {
                      }} {
        buildbot.publish("mgba_libretro", "2026-10-01", "core one", std::nullopt,
                         std::string{gbInfo});
    }

    /// `root` with a Game Boy ROM and an opensu-retro to start.
    static fs::path prepared(const fs::path& root) {
        fs::create_directories(root / "ROM" / "gb");
        std::ofstream{root / "ROM" / "gb" / "Tetris.gb"} << "rom";
        fs::create_directories(root / "bin");
        std::ofstream{root / "bin" / "opensu-retro"} << "#!/bin/sh\n";
        fs::permissions(root / "bin" / "opensu-retro", fs::perms::owner_all);
        return root;
    }

    [[nodiscard]] library::roms::EmulatorSetup setup() const {
        library::roms::EmulatorEntry entry;
        entry.id = "gb-core";
        entry.name = "GB (built-in)";
        entry.systems = {"gb"};
        entry.core = "mgba_libretro";
        entry.withoutBios = library::roms::WithoutBios::Hle;
        library::roms::EmulatorSearch search;
        search.system = root / "bios";
        search.cores = library::roms::CoreSearch{root / "cores", {root / "system"}, {}};
        search.runner = root / "bin" / "opensu-retro";
        return library::roms::EmulatorSetup{
            library::roms::EmulatorCatalog{{entry}}, search, {}, buildbot.base()};
    }

    [[nodiscard]] library::Game game() const {
        library::roms::Provider provider{{root / "ROM"}, setup(), library::roms::NameDb{}, {}, {}};
        std::vector<library::Game> games = provider.list();
        preferences.values().emulators.apply(games);
        return games.at(0);
    }

    /// The core build openSU downloaded, or empty.
    [[nodiscard]] std::string version() const {
        return downloads.downloaded("gb-core").value_or(app::EmulatorDownloads::Build{}).version;
    }

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
    test::BuildbotServer buildbot;
    app::Preferences preferences;
    std::vector<std::pair<std::string, bool>> said;
    std::vector<library::LaunchSpec> launched;
    ui::DetailsPage page;
    audio::SdlSoundPlayer sounds;
    app::EmulatorDownloads downloads{
        setup(), app::EmulatorDownloads::Hooks{[this](const std::string& text, bool error) {
                                                   said.emplace_back(text, error);
                                               },
                                               [this] {
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
                                     [] {
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
                                          launched.push_back(game.launch);
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

void waitForCheck(Rig& rig) {
    const auto until = std::chrono::steady_clock::now() + wait;
    while (!rig.downloads.check().done && std::chrono::steady_clock::now() < until) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
}

void detailsDownloadsTheCore(const fs::path& root) {
    Rig rig{root};
    expect(rig.shown.launch.empty() && rig.shown.emulatorOptions.size() == 1 &&
               rig.shown.emulatorOptions[0].downloadable,
           "a Game Boy ROM lists the built-in emulator, not yet downloaded");
    rig.details.open();
    const auto& buttons = rig.page.view().buttons;
    expect(buttons.size() == 5 && buttons[1].action == ui::DetailsAction::DownloadEmulator &&
               buttons[1].label == "Download GB (built-in)",
           "the details page offers its core");
    rig.details.act(Button::Down);
    rig.details.act(Button::A);
    rig.settle();
    expect(!rig.said.back().second &&
               rig.said.back().first == "GB (built-in) 2026-10-01 is installed",
           "the core is installed with its build date");
    rig.details.refresh();
    rig.details.act(Button::A);
    const fs::path core = root / "cores" / "mgba_libretro.so";
    expect(rig.launched.size() == 1 &&
               rig.launched[0].program == (root / "bin" / "opensu-retro").string() &&
               rig.launched[0].args ==
                   std::vector<std::string>{"--core", core.string(),
                                            (root / "ROM" / "gb" / "Tetris.gb").string()},
           "Play starts opensu-retro on the downloaded core");
}

void settingsUpdateAndRemove(const fs::path& root) {
    Rig rig{root};
    ui::SettingsCategory page = rig.emulators.page();
    expect(rowOf(page, "emulators.run.gb-core").value == "Download",
           "Settings offers the core for download");
    rig.emulators.run("emulators.run.gb-core", 0);
    rig.settle();
    page = rig.emulators.page();
    expect(rowOf(page, "emulators.run.gb-core").note.starts_with("libretro buildbot 2026-10-01") &&
               hasRow(page, "emulators.remove.gb-core"),
           "once downloaded it names the buildbot and the build, and can be removed");

    rig.buildbot.publish("mgba_libretro", "2026-10-02", "core two");
    rig.emulators.run("emulators.check", 0);
    waitForCheck(rig);
    page = rig.emulators.page();
    expect(rowOf(page, "emulators.run.gb-core").value == "Update to 2026-10-02" &&
               rowOf(page, "emulators.check").value == "1 update",
           "the check finds the newer build and updates nothing");
    expect(rig.version() == "2026-10-01", "the old build stays");

    rig.buildbot.publish("mgba_libretro", "2026-10-02", "core two", 0xdeadbeef);
    rig.emulators.run("emulators.run.gb-core", 0);
    rig.settle();
    expect(rig.said.back().second && rig.said.back().first.find("CRC-32") != std::string::npos &&
               rig.version() == "2026-10-01",
           "an update whose CRC-32 does not match is refused and the old build stays");

    rig.buildbot.publish("mgba_libretro", "2026-10-02", "core two");
    rig.emulators.run("emulators.run.gb-core", 0);
    rig.settle();
    expect(rig.version() == "2026-10-02" && !rig.downloads.updateAvailable("gb-core"),
           "the verified update is switched to");

    rig.emulators.run("emulators.remove.gb-core", 0);
    expect(rig.downloads.downloaded("gb-core").has_value(), "Remove asks for a second press");
    rig.emulators.run("emulators.remove.gb-core", 0);
    expect(!rig.downloads.downloaded("gb-core") &&
               !fs::exists(root / "cores" / "mgba_libretro.so") &&
               rowOf(rig.emulators.page(), "emulators.run.gb-core").value == "Download",
           "the second press removes the core");
}

void systemCoreIsNotOurs(const fs::path& root) {
    Rig rig{root};
    fs::create_directories(root / "system");
    std::ofstream{root / "system" / "mgba_libretro.so"} << "core";
    rig.emulators.rescan();
    const ui::SettingsCategory page = rig.emulators.page();
    expect(rowOf(page, "emulators.run.gb-core")
                   .note.starts_with("Core in " + (root / "system").string()) &&
               rowOf(page, "emulators.run.gb-core").value == "Installed" &&
               !hasRow(page, "emulators.remove.gb-core"),
           "a core in a system folder shows the folder and offers no Remove");
    rig.emulators.run("emulators.check", 0);
    waitForCheck(rig);
    expect(rig.downloads.latest("gb-core").has_value() && rig.downloads.check().failures.empty(),
           "the check reads the buildbot's listing once for the cores");
    expect(rig.downloads.remove("gb-core") == "openSU did not download it",
           "a core openSU did not download is never removed");
}

void settingsShowTheBios(const fs::path& root) {
    Rig rig{root};
    expect(!hasRow(rig.emulators.page(), "emulators.bios.gb-core"),
           "a core not installed shows no BIOS");
    rig.emulators.run("emulators.run.gb-core", 0);
    rig.settle();
    ui::SettingsCategory page = rig.emulators.page();
    expect(rowOf(page, "emulators.bios.gb-core").value == "HLE built in" &&
               rowOf(page, "emulators.bios.gb-core").label == "Game Boy BIOS" &&
               rowOf(page, "emulators.bios.gb-core/gb_bios.bin").value == "Missing" &&
               rowOf(page, "emulators.bios.gb-core/gb_bios.bin").note == "Game Boy BIOS",
           "the downloaded core's info lists its BIOS, missing, so it runs its HLE");
    fs::create_directories(root / "bios");
    std::ofstream{root / "bios" / "gb_bios.bin"} << "bios";
    page = rig.emulators.page();
    expect(rowOf(page, "emulators.bios.gb-core").value == "Your BIOS file gb_bios.bin" &&
               rowOf(page, "emulators.bios.gb-core/gb_bios.bin").value == "In use",
           "a file in the system folder is the one in use");

    fs::remove(root / "cores" / "mgba_libretro.info");
    expect(rowOf(rig.emulators.page(), "emulators.bios.gb-core").value == "Not listed",
           "a core downloaded without its info says so");
    rig.emulators.run("emulators.check", 0);
    waitForCheck(rig);
    expect(rig.downloads.check().failures.empty() &&
               rowOf(rig.emulators.page(), "emulators.bios.gb-core/gb_bios.bin").value == "In use",
           "the update check fetches the missing info");
}

} // namespace

int main() {
    try {
        const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "core_flow"};
        detailsDownloadsTheCore(scratch.path() / "details");
        settingsUpdateAndRemove(scratch.path() / "settings");
        systemCoreIsNotOurs(scratch.path() / "system");
        settingsShowTheBios(scratch.path() / "bios");
        std::puts("core_flow: ok");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}
