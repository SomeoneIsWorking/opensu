// Play on a game that cannot start yet, through PlayWait with a scripted library and downloads.
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/play_wait.hpp"
#include "library/romset_match.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::app::EmulatorDownloads;
using opensu::app::LaunchRequest;
using opensu::app::PlayWait;
using opensu::library::EmulatorOption;
using opensu::library::Game;
using opensu::library::RomsetFit;

int failures = 0;

void expect(bool condition, std::string_view what) {
    if (!condition) {
        std::printf("FAILED: %.*s\n", static_cast<int>(what.size()), what.data());
        ++failures;
    }
}

struct World {
    Game game;
    std::optional<EmulatorDownloads::Progress> running;
    std::optional<EmulatorDownloads::Finished> finished;
    std::vector<std::string> downloads;
    std::vector<std::string> cancelled;
    std::vector<std::string> played;
    std::vector<std::string> refused;
    std::string line;
    int shown{0};
    std::string downloadRefusal;

    PlayWait::Hooks hooks() {
        return PlayWait::Hooks{
            .find =
                [this](const std::string&) {
                    return std::optional<Game>{game};
                },
            .play =
                [this](const Game& started, const LaunchRequest&) {
                    played.push_back(started.title);
                },
            .show =
                [this](const Game&) {
                    ++shown;
                },
            .update =
                [this](std::string text, std::optional<double>) {
                    line = std::move(text);
                },
            .refuse =
                [this](const std::string& text) {
                    refused.push_back(text);
                },
            .progress =
                [this] {
                    return running;
                },
            .finished =
                [this] {
                    return finished;
                },
            .download =
                [this](const std::string& id) {
                    if (downloadRefusal.empty()) {
                        downloads.push_back(id);
                        running = EmulatorDownloads::Progress{id, 50, 100};
                    }
                    return downloadRefusal;
                },
            .cancelDownload =
                [this](const std::string& id) {
                    cancelled.push_back(id);
                },
        };
    }
};

Game romWithoutCore() {
    Game game;
    game.id = "rom";
    game.title = "Crash";
    game.source = opensu::library::Source::Rom;
    game.installed = true;
    EmulatorOption core;
    core.name = "PCSX";
    core.id = "pcsx";
    core.downloadable = true;
    game.emulatorOptions.push_back(core);
    return game;
}

void downloadsThenPlays() {
    World world;
    world.game = romWithoutCore();
    PlayWait wait{world.hooks()};
    expect(wait.begin(world.game, LaunchRequest{.profile = "p"}), "a missing core is waited for");
    expect(world.shown == 1 && world.downloads == std::vector<std::string>{"pcsx"},
           "the card is up and the core downloads");
    wait.service();
    expect(world.downloads.size() == 1, "a running download is not started again");
    expect(world.line == "Downloading PCSX…", "the card says what downloads");

    world.running.reset();
    world.finished = EmulatorDownloads::Finished{
        1, "pcsx", EmulatorDownloads::Finished::Outcome::Installed, "installed"};
    wait.service();
    expect(world.downloads.size() == 1 && wait.waiting(),
           "installed but not read yet: it waits for the library");

    world.game.emulatorOptions.front().installed = true;
    world.game.launch.program = "core";
    wait.service();
    expect(world.played == std::vector<std::string>{"Crash"} && !wait.waiting(),
           "the game starts once the library has the core");
}

void failedDownloadEnds() {
    World world;
    world.game = romWithoutCore();
    PlayWait wait{world.hooks()};
    expect(wait.begin(world.game, LaunchRequest{.profile = "p"}), "waits");
    world.running.reset();
    world.finished = EmulatorDownloads::Finished{
        1, "pcsx", EmulatorDownloads::Finished::Outcome::Failed, "no network"};
    wait.service();
    expect(!wait.waiting() && world.played.empty() && world.downloads.size() == 1,
           "a failed download ends the wait and is not retried");
}

void refusedDownloadSays() {
    World world;
    world.game = romWithoutCore();
    world.downloadRefusal = "no";
    PlayWait wait{world.hooks()};
    expect(wait.begin(world.game, LaunchRequest{.profile = "p"}), "waits");
    expect(!wait.waiting() && world.refused == std::vector<std::string>{"no"},
           "a refused download tells why");
}

void cancelStopsDownload() {
    World world;
    world.game = romWithoutCore();
    PlayWait wait{world.hooks()};
    expect(wait.begin(world.game, LaunchRequest{.profile = "p"}), "waits");
    wait.cancel();
    expect(!wait.waiting() && world.cancelled == std::vector<std::string>{"pcsx"},
           "Cancel stops the download the wait started");
}

void romsetCheckResolves() {
    World world;
    Game game;
    game.id = "rom";
    game.title = "Gunlock";
    game.source = opensu::library::Source::Rom;
    game.installed = true;
    game.unavailable = "No arcade core matches this romset";
    EmulatorOption core;
    core.name = "FBNeo";
    core.id = "fbneo";
    core.installed = true;
    core.romset = RomsetFit::Pending;
    game.emulatorOptions.push_back(core);
    world.game = game;
    PlayWait wait{world.hooks()};
    expect(wait.begin(game, LaunchRequest{.profile = "p"}), "a pending romset is waited for");
    expect(world.line == opensu::library::roms::checkingRomset, "the card says it checks");
    world.game.emulatorOptions.front().romset = RomsetFit::Mismatch;
    wait.service();
    expect(!wait.waiting() &&
               world.refused == std::vector<std::string>{"No arcade core matches this romset"},
           "the real verdict is told");
}

void nothingToWaitFor() {
    World world;
    Game game;
    game.id = "rom";
    PlayWait wait{world.hooks()};
    expect(!wait.begin(game, LaunchRequest{.profile = "p"}) && world.shown == 0,
           "nothing can help: no wait");
}

} // namespace

int main() try {
    downloadsThenPlays();
    failedDownloadEnds();
    refusedDownloadSays();
    cancelStopsDownload();
    romsetCheckResolves();
    nothingToWaitFor();
    return failures == 0 ? 0 : 1;
} catch (...) {
    return opensu::test::escaped();
}
