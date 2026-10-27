// A built-in emulator's core downloaded from where the player meets its absence, through the
// shipping job, screens and ROM listing, against a fake buildbot: the details page's Download
// row, Play starting opensu-retro on the core, and Settings › Emulators showing the buildbot
// as its source, the update check, an update and Remove, asked first.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "details_controller.hpp"
#include "emulator_downloads.hpp"
#include "emulator_settings.hpp"
#include "library/emulator_choice.hpp"
#include "library/roms.hpp"
#include "support/buildbot_server.hpp"
#include "support/fake_interface_text.hpp"
#include "support/scratch_dir.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using gpu2d::ui::Action;
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
        library::applyEmulatorPicks(gameSettings.layers(), games);
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
    app::GameSettings gameSettings{settings::GameSettingsStore{root / "game-settings.json"},
                                   root / "settings.json", [](const std::string&) {
                                   }};
    std::vector<std::pair<std::string, bool>> said;
    std::vector<library::LaunchSpec> launched;
    test::InterfaceRig face;
    ui::ScreenImages images;
    ui::Chrome chrome;
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
        downloads, app::EmulatorSettings::Hooks{[this](const std::string& text, bool error) {
            said.emplace_back(text, error);
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
        app::DetailsController::Place{face.navigator, chrome, images}, sounds,
        app::DetailsController::Services{preferences, gameSettings, downloads, uninstall, update},
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
                                      },
                                      [](const library::Game&) {
                                      },
                                      [](const library::Game&) {
                                          return std::optional<std::size_t>{};
                                      },
                                      [](const library::Game&) {
                                      },
                                      [](const library::Game&) {
                                      },
                                      [](const library::Game&) {
                                      },
                                      [](const library::Game&) {
                                      }}};

    /// Each action as a press, the stack settled after it.
    void press(std::initializer_list<Action> actions) {
        for (const Action action : actions) {
            face.navigator.act(action);
            face.frame(1.0);
        }
    }
};

const ui::SettingsRow& rowOf(const ui::SettingsCategory& page, const std::string& id) {
    for (const ui::SettingsRow& row : page.rows) {
        if (row.id == id) {
            return row;
        }
    }
    test::fail("a row with that id exists");
}

/// The Emulators category's page `id`: its Downloads or its BIOS files.
ui::SettingsCategory pageOf(const app::EmulatorSettings& emulators, std::string_view id) {
    return test::need(emulators.subPage(id), "the Emulators category's page");
}

ui::SettingsCategory downloadsOf(const app::EmulatorSettings& emulators) {
    return pageOf(emulators, "emulators.page.downloads");
}

ui::SettingsCategory biosOf(const app::EmulatorSettings& emulators) {
    return pageOf(emulators, "emulators.page.bios");
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
    rig.face.frame(1.0);
    const ui::SettingsRow* download = ui::actionOf(*rig.details.details(), ui::detailsDownloadId);
    expect(download != nullptr && download->label == "Download GB (built-in)",
           "the details page offers its core");
    rig.press({Action::Down, Action::Accept});
    rig.settle();
    expect(!rig.said.back().second &&
               rig.said.back().first == "GB (built-in) 2026-10-01 is installed",
           "the core is installed with its build date");
    rig.details.refresh();
    rig.face.frame(1.0);
    rig.press({Action::Accept});
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
    ui::SettingsCategory page = downloadsOf(rig.emulators);
    expect(rowOf(page, "emulators.run.gb-core").value == "Download",
           "Settings offers the core for download");
    rig.emulators.run("emulators.run.gb-core", 0);
    rig.settle();
    page = downloadsOf(rig.emulators);
    expect(rowOf(page, "emulators.run.gb-core").note.starts_with("libretro buildbot 2026-10-01") &&
               hasRow(page, "emulators.remove.gb-core"),
           "once downloaded it names the buildbot and the build, and can be removed");

    rig.buildbot.publish("mgba_libretro", "2026-10-02", "core two");
    rig.emulators.run("emulators.check", 0);
    waitForCheck(rig);
    page = downloadsOf(rig.emulators);
    expect(rowOf(page, "emulators.run.gb-core").value == "Update to 2026-10-02" &&
               rowOf(rig.emulators.page(), "emulators.check").value == "1 update",
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

    expect(!rowOf(downloadsOf(rig.emulators), "emulators.remove.gb-core").confirm.empty(),
           "Remove carries the question the screen asks before it runs");
    rig.emulators.run("emulators.remove.gb-core", 0);
    expect(!rig.downloads.downloaded("gb-core") &&
               !fs::exists(root / "cores" / "mgba_libretro.so") &&
               rowOf(downloadsOf(rig.emulators), "emulators.run.gb-core").value == "Download",
           "once confirmed, Remove removes the core");
}

void systemCoreIsNotOurs(const fs::path& root) {
    Rig rig{root};
    fs::create_directories(root / "system");
    std::ofstream{root / "system" / "mgba_libretro.so"} << "core";
    rig.emulators.rescan();
    const ui::SettingsCategory page = downloadsOf(rig.emulators);
    expect(rowOf(page, "emulators.run.gb-core").note.starts_with("Core · ") &&
               rowOf(page, "emulators.run.gb-core").value == "Installed" &&
               !hasRow(page, "emulators.remove.gb-core"),
           "a core in a system folder reads as a core found here and offers no Remove");
    rig.emulators.run("emulators.check", 0);
    waitForCheck(rig);
    expect(rig.downloads.latest("gb-core").has_value() && rig.downloads.check().failures.empty(),
           "the check reads the buildbot's listing once for the cores");
    expect(rig.downloads.remove("gb-core") == "openSU did not download it",
           "a core openSU did not download is never removed");
}

void settingsShowTheBios(const fs::path& root) {
    Rig rig{root};
    expect(!hasRow(biosOf(rig.emulators), "emulators.bios.gb-core"),
           "a core not installed shows no BIOS");
    rig.emulators.run("emulators.run.gb-core", 0);
    rig.settle();
    ui::SettingsCategory page = biosOf(rig.emulators);
    expect(
        rowOf(page, "emulators.bios.gb-core").value == "HLE built in" &&
            rowOf(page, "emulators.bios.gb-core").label == "GB (built-in)" &&
            std::ranges::any_of(page.rows,
                                [](const ui::SettingsRow& row) {
                                    return row.kind == ui::RowKind::Heading &&
                                           row.label == "Game Boy";
                                }) &&
            rowOf(page, "emulators.bios.gb-core/gb_bios.bin").value == "Missing" &&
            rowOf(page, "emulators.bios.gb-core/gb_bios.bin").note == "Game Boy BIOS",
        "the downloaded core's info lists its BIOS under its console, missing, so it runs its HLE");
    fs::create_directories(root / "bios");
    std::ofstream{root / "bios" / "gb_bios.bin"} << "bios";
    page = biosOf(rig.emulators);
    expect(rowOf(page, "emulators.bios.gb-core").value == "Your BIOS file gb_bios.bin" &&
               rowOf(page, "emulators.bios.gb-core/gb_bios.bin").value == "In use",
           "a file in the system folder is the one in use");

    fs::remove(root / "cores" / "mgba_libretro.info");
    expect(rowOf(biosOf(rig.emulators), "emulators.bios.gb-core").value == "Not listed",
           "a core downloaded without its info says so");
    rig.emulators.run("emulators.check", 0);
    waitForCheck(rig);
    expect(rig.downloads.check().failures.empty() &&
               rowOf(biosOf(rig.emulators), "emulators.bios.gb-core/gb_bios.bin").value == "In use",
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
