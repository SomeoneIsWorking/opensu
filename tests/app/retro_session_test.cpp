// RetroSession against a fake runner: each launch gets its own control file, the port is read once
// the runner announces it, requests come back as done or as the runner's reason, and the load
// failure a runner leaves is handed over once and the file removed.
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <unistd.h>

#include "retro_session.hpp"
#include "support/fake_runner.hpp"
#include "support/scratch_dir.hpp"
#include "ui/check.hpp"

namespace {

using namespace opensu;
using test::expect;

std::filesystem::path fileOf(const std::string& entry) {
    return std::filesystem::path{entry.substr(entry.find('=') + 1)};
}

void eachLaunchHasItsOwnFile() {
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "retro_session_files"};
    app::RetroSession session{scratch.path() / "opensu"};
    const std::string first = session.begin();
    expect(first == "OPENSU_RETRO_CONTROL=" + (scratch.path() / "opensu").string() + "/retro-" +
                        std::to_string(getpid()) + "-0.json",
           "the environment entry names the runtime directory's control file");
    expect(session.end().empty(), "a launch that left nothing has no failure");
    const std::string second = session.begin();
    expect(second != first, "the next launch gets a new file");
    expect(!session.status(), "no status before a runner announces itself");
    expect(session.saveState() == "the game has no control channel", "nothing to ask yet");
    static_cast<void>(session.end());
}

void requestsReachTheRunner() {
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "retro_session_requests"};
    app::RetroSession session{scratch.path()};
    test::FakeRunner runner;
    const std::string entry = session.begin();
    runner.announce(entry);
    const std::optional<app::RetroSession::Status> before = session.status();
    expect(before && before->core == "fake" && before->rom == "/roms/game.gba" &&
               before->frame == 42 && !before->hasState,
           "the runner's status is read through the announced port");
    expect(session.loadState() == "no state is saved for this game",
           "a refusal comes back as the runner's reason");
    expect(session.saveState().empty(), "a save is done");
    const std::optional<app::RetroSession::Status> after = session.status();
    expect(after && after->hasState, "the runner has the state");
    expect(session.loadState().empty() && session.reset().empty(), "load and reset are done");
    const std::vector<app::RetroSession::CoreOption> options = session.options();
    expect(options.size() == 1 && options.front().definition.key == "mgba_color" &&
               options.front().value == "OFF" && options.front().definition.values.size() == 2,
           "the runner's core options are read");
    expect(session.setOption("mgba_color", "GBA").empty(), "an option is set");
    expect(session.options().front().value == "GBA", "the runner holds the new value");
    expect(runner.calls() == std::vector<std::string>{"/state/load", "/state/save", "/state/load",
                                                      "/reset", "/options", "/option", "/options"},
           "each request reached the runner once");
    runner.stop();
    expect(!session.status(), "a runner that is gone has no status");
    expect(session.saveState() == "the game does not answer", "nor does it save");
    expect(session.end().empty(), "a runner's announcement is no failure");
    expect(!std::filesystem::exists(fileOf(entry)), "the control file is removed at the end");
}

void loadFailureIsHandedOver() {
    const test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "retro_session_failure"};
    app::RetroSession session{scratch.path()};
    const std::filesystem::path file = fileOf(session.begin());
    std::ofstream{file} << R"({"error": "mGBA cannot load /roms/broken.gba"})";
    expect(!session.status(), "a failure announces no port");
    expect(session.end() == "mGBA cannot load /roms/broken.gba", "the runner's reason is read");
    expect(!std::filesystem::exists(file), "the shell removes the file it read");
    expect(session.end().empty(), "it is handed over once");
}

} // namespace

int main() {
    try {
        eachLaunchHasItsOwnFile();
        requestsReachTheRunner();
        loadFailureIsHandedOver();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: unhandled exception: %s\n", error.what());
        return 1;
    }
    std::printf("retro_session: all checks passed\n");
    return 0;
}
