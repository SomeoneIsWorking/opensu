// The Downloads page's controller through the shipping owners: an Epic install against a stub
// legendary, an emulator download against a fake release host, and Steam's queue against a fake
// Steam's DevTools. Each button reaches the owner of its row.
#include "downloads_controller.hpp"

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "support/fake_steam.hpp"
#include "support/release_server.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using app::DownloadRow;
using app::DownloadState;
using gamepad::Button;
using test::expect;
using Clock = std::chrono::steady_clock;

constexpr auto wait = std::chrono::seconds{20};

fs::path writeStub(const fs::path& path, const std::string& body) {
    fs::create_directories(path.parent_path());
    std::ofstream{path} << "#!/bin/sh\n" << body;
    fs::permissions(path, fs::perms::owner_all);
    return path;
}

/// The shipping owners, wired as the shell wires them, with fakes only at the edges.
struct Rig {
    explicit Rig(const fs::path& root)
        : root{root},
          legendary{writeStub(
              root / "bin" / "legendary",
              R"(echo "[DLManager] INFO: = Progress: 42.00% (42/100), Running for 00:00:10, ETA: 00:00:14" >&2
sleep 60
)")} {
        server.publish("emu", "v1", 1, "Emu-1.AppImage",
                       std::string(std::size_t{32} * 1024 * 1024, 'x'));
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

    /// Runs the loop until `done`, failing with `what` when it never is.
    template <typename Done> void until(Done done, const char* what) {
        const Clock::time_point stop = Clock::now() + wait;
        while (!done() && Clock::now() < stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
            emulators.service();
            controller.service();
        }
        expect(done(), what);
    }

    /// Runs the loop until the Steam call is answered.
    void answered() {
        until(
            [this] {
                return !controller.answering();
            },
            "Steam answers");
    }

    /// Focuses `key`'s row and button `label`, and presses A.
    void press(const std::string& key, const std::string& label) {
        const std::vector<ui::DownloadsEntry>& entries = page.entries();
        for (std::size_t row = 0; row < entries.size(); ++row) {
            if (entries[row].key != key) {
                continue;
            }
            for (std::size_t action = 0; action < entries[row].actions.size(); ++action) {
                if (entries[row].actions[action] == label) {
                    page.focusAction(row, action);
                    controller.act(Button::A);
                    return;
                }
            }
        }
        test::fail("the row shows the button");
    }

    [[nodiscard]] DownloadState stateOf(const std::string& key) const {
        const DownloadRow* row = controller.board().find(key);
        if (row == nullptr) {
            test::fail("the row is listed");
        }
        return row->state;
    }

    fs::path root;
    fs::path legendary;
    test::ReleaseServer server;
    test::FakeSteam fakeSteam{[](const std::string&) {
        return nlohmann::json{};
    }};
    steam::DevTools devTools{fakeSteam.port()};
    steam::DownloadControl steamControl{devTools};
    std::vector<steam::Download> queue;
    bool steamReady{false};
    steam::Client steamClient{steam::Client::Options{.home = root / "home",
                                                     .executablePath = {},
                                                     .session = "downloads-test",
                                                     .steamRoots = {}}};
    app::Installs installs{steamClient, legendary.string(),
                           app::GogInstallJob::Options{.dataDir = root / "data"},
                           [](library::Source) {
                               return std::optional<fs::path>{};
                           },
                           [](const library::Game&) {
                               return false;
                           }};
    std::vector<std::pair<std::string, bool>> said;
    app::EmulatorDownloads emulators{
        setup(), app::EmulatorDownloads::Hooks{[this](const std::string& text, bool error) {
                                                   said.emplace_back(text, error);
                                               },
                                               [] {
                                               },
                                               [] {
                                               }}};
    ui::DownloadsPage page;
    audio::SdlSoundPlayer sounds;
    app::DownloadsController controller{
        page,
        app::DownloadsController::Sources{
            .installs = installs,
            .emulators = emulators,
            .steam = steamControl,
            .steamReady =
                [this] {
                    return steamReady;
                },
            .steamQueue =
                [this] {
                    return queue;
                },
            .titles = [](library::Source store, const std::string& id) -> std::string {
                return store == library::Source::Steam && id == "480" ? "Spacewar" : "";
            }},
        sounds, app::DownloadsController::Hooks{[this](const std::string& text, bool error) {
            said.emplace_back(text, error);
        }}};
};

library::Game epicGame() {
    library::Game game;
    game.source = library::Source::Epic;
    game.sourceId = "Fortnite";
    game.title = "Fortnite";
    return game;
}

void cancelReachesEachOwner(const fs::path& root) {
    Rig rig{root};
    expect(rig.installs.start(epicGame()), "an Epic install starts");
    rig.controller.open();
    expect(rig.page.isOpen(), "the page opens");
    rig.until(
        [&rig] {
            const DownloadRow* row = rig.controller.board().find("epic:Fortnite");
            return row != nullptr && row->state == DownloadState::Downloading;
        },
        "the Epic install shows as downloading once legendary reports progress");
    expect(rig.page.entries().size() == 1, "the install is on the page");
    const ui::DownloadsEntry& epic = rig.page.entries()[0];
    expect(epic.title == "Fortnite" && epic.source == "Epic" && epic.state == "Downloading" &&
               epic.figures == "42%" && epic.actions == std::vector<std::string>{"Cancel"},
           "the Epic row shows its source, state, percent and a cancel");

    rig.press("epic:Fortnite", "Cancel");
    rig.until(
        [&rig] {
            // The shell's panel flow takes the reports; the install is over once the last is.
            (void)rig.installs.take();
            return !rig.installs.running();
        },
        "the install ends");
    rig.until(
        [&rig] {
            return rig.stateOf("epic:Fortnite") == DownloadState::Cancelled;
        },
        "cancel on the Epic row cancels the store install");

    expect(rig.emulators.start("emu").empty(), "an emulator download starts");
    rig.controller.service();
    expect(rig.stateOf("emulator:emu") == DownloadState::Downloading &&
               rig.stateOf("epic:Fortnite") == DownloadState::Cancelled,
           "it is listed above the cancelled install");
    rig.press("emulator:emu", "Cancel");
    rig.until(
        [&rig] {
            return rig.stateOf("emulator:emu") == DownloadState::Cancelled;
        },
        "cancel on the emulator row cancels the emulator download");
    const std::optional<app::Installs::Status> install = rig.installs.status();
    expect(install && install->job.phase == app::InstallJob::Phase::Cancelled &&
               !rig.emulators.downloaded("emu"),
           "nothing was installed");
    expect(rig.page.entries().size() == 2 && rig.page.entries()[0].ended &&
               rig.page.entries()[0].actions.empty() && rig.page.entries()[0].state == "Cancelled",
           "ended downloads stay on the page without buttons");

    const std::vector<app::DownloadLine> lines = rig.controller.lines();
    expect(lines.size() == 2 && lines[0].key == "emulator:emu" && lines[0].source == "Emulator" &&
               lines[0].state == "cancelled" && lines[1].key == "epic:Fortnite" &&
               lines[1].percent == 42,
           "the control channel sees the same rows");
    rig.controller.act(Button::B);
    expect(!rig.page.isOpen(), "B closes the page");
}

void steamButtonsCallSteam(const fs::path& root) {
    Rig rig{root};
    rig.steamReady = true;
    rig.queue = {steam::Download{.appId = "480",
                                 .progress = 0.25,
                                 .active = true,
                                 .queueIndex = 0,
                                 .bytesDone = 1'000'000'000,
                                 .bytesTotal = 4'000'000'000,
                                 .bytesPerSecond = 12'300'000,
                                 .updateState = "Downloading"},
                 steam::Download{.appId = "70", .queueIndex = 1},
                 steam::Download{.appId = "71", .queueIndex = 2}};
    rig.controller.open();
    const std::vector<ui::DownloadsEntry>& entries = rig.page.entries();
    expect(entries.size() == 3 && entries[0].key == "steam:480" && entries[1].key == "steam:70" &&
               entries[2].key == "steam:71",
           "Steam's queue is listed in its order");
    expect(entries[0].title == "Spacewar" && entries[0].source == "Steam" &&
               entries[0].figures == "25%  ·  1.0 GB of 4.0 GB  ·  12.3 MB/s" &&
               entries[0].actions == std::vector<std::string>{"Pause"},
           "the active download shows its bytes and rate and pauses");
    expect(entries[1].actions == std::vector<std::string>{"Download now", "Move down", "Remove"},
           "the next in line can start now, move down or go");

    rig.press("steam:480", "Pause");
    rig.answered();
    expect(rig.fakeSteam.calls().back() == "SteamClient.Downloads.EnableAllDownloads(false, \"0\")",
           "Pause pauses Steam's queue");

    rig.press("steam:70", "Move down");
    rig.answered();
    expect(rig.fakeSteam.calls().back() == "SteamClient.Downloads.SetQueueIndex(70, 2, \"0\")",
           "Move down moves it one place down");

    rig.press("steam:71", "Download now");
    rig.answered();
    expect(rig.fakeSteam.calls().back() == "SteamClient.Downloads.ResumeAppUpdate(71, \"0\")",
           "Download now starts it ahead of the rest");

    rig.press("steam:71", "Remove");
    rig.answered();
    expect(rig.fakeSteam.calls().back() ==
               "SteamClient.Downloads.RemoveFromDownloadList(71, \"0\")",
           "Remove takes it off Steam's list");
    expect(rig.controller.board().find("steam:71") == nullptr && rig.page.entries().size() == 2,
           "and off the page while Steam still lists it");
    expect(rig.said.empty(), "nothing was refused");

    rig.queue = {steam::Download{.appId = "480", .paused = true, .queueIndex = 0},
                 steam::Download{.appId = "70", .queueIndex = 1}};
    rig.controller.service();
    expect(rig.page.entries()[0].state == "Paused" &&
               rig.page.entries()[0].actions == std::vector<std::string>{"Resume", "Remove"},
           "the paused head resumes or goes");
    rig.press("steam:480", "Resume");
    rig.answered();
    expect(rig.fakeSteam.calls().back() == "SteamClient.Downloads.EnableAllDownloads(true, \"0\")",
           "Resume resumes Steam's queue");
}

void steamRefusalIsSaid(const fs::path& root) {
    Rig rig{root};
    std::uint16_t port = 0;
    {
        const test::FakeSteam gone{[](const std::string&) {
            return nlohmann::json{};
        }};
        port = gone.port();
    }
    steam::DevTools closed{port};
    steam::DownloadControl control{closed};
    app::DownloadsController controller{
        rig.page,
        app::DownloadsController::Sources{
            .installs = rig.installs,
            .emulators = rig.emulators,
            .steam = control,
            .steamReady =
                [] {
                    return true;
                },
            .steamQueue =
                [] {
                    return std::vector{
                        steam::Download{.appId = "480", .active = true, .queueIndex = 0}};
                },
            .titles =
                [](library::Source, const std::string&) {
                    return std::string{};
                }},
        rig.sounds, app::DownloadsController::Hooks{[&rig](const std::string& text, bool error) {
            rig.said.emplace_back(text, error);
        }}};
    controller.open();
    controller.act(Button::A);
    const Clock::time_point stop = Clock::now() + wait;
    while (controller.answering() && Clock::now() < stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
        controller.service();
    }
    expect(rig.said.size() == 1 && rig.said[0].second &&
               rig.said[0].first.starts_with("Steam would not pause downloads"),
           "a call Steam does not take is told to the player");
    expect(controller.board().find("steam:480")->title == "Steam app 480",
           "a game the library does not know is named by its id");
}

void figures() {
    DownloadRow row{.key = "gog:1",
                    .store = library::Source::Gog,
                    .title = "Gwent",
                    .state = DownloadState::Downloading,
                    .fraction = 0.429,
                    .bytesDone = 900,
                    .bytesTotal = 2'500'000,
                    .secondsLeft = 3 * 3600 + 125};
    expect(app::entryOf(row).figures == "42%  ·  900 B of 2.5 MB  ·  3 h 2 min left",
           "percent, bytes and time left, in that order");
    expect(app::entryOf(row).state == "Downloading", "a first install downloads");
    row.update = true;
    expect(app::entryOf(row).state == "Updating", "an update that moves bytes reads Updating");
    row.update = false;
    row.state = DownloadState::Queued;
    row.fraction.reset();
    row.bytesTotal = 0;
    row.secondsLeft.reset();
    row.detail = "Waiting for legendary";
    expect(app::entryOf(row).figures == "Waiting for legendary",
           "an unmeasured install shows its owner's line");
    row.state = DownloadState::Failed;
    row.fraction = 0.5;
    row.detail = "Not enough disk space";
    const ui::DownloadsEntry failed = app::entryOf(row);
    expect(failed.failed && failed.ended && failed.figures == "Not enough disk space" &&
               !failed.fraction && failed.actions.empty(),
           "a failed download says why, with no bar or buttons");
    row.kind = app::DownloadKind::Core;
    row.store.reset();
    expect(app::entryOf(row).source == "Core", "a core is its own source");

    const DownloadRow removing{.key = "gog:2",
                               .store = library::Source::Gog,
                               .title = "Gwent",
                               .state = DownloadState::Uninstalling,
                               .detail = "legendary is removing it"};
    const ui::DownloadsEntry entry = app::entryOf(removing);
    expect(entry.state == "Uninstalling" && entry.figures == "legendary is removing it" &&
               !entry.ended && !entry.fraction && entry.actions.empty(),
           "an uninstall reads as uninstalling, with no bar and no buttons");
}

} // namespace

int main() {
    try {
        const fs::path root = fs::path{OPENSU_TEST_SCRATCH} / "downloads_controller";
        std::error_code ec;
        fs::remove_all(root, ec);
        cancelReachesEachOwner(root / "cancel");
        steamButtonsCallSteam(root / "steam");
        steamRefusalIsSaid(root / "refusal");
        figures();
        fs::remove_all(root, ec);
        std::puts("downloads_controller: ok");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
}
