// The control file: announced while the channel is open and removed with it, and a failure
// reported in it kept for the shell after the channel is gone.
#include "control_server.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "check.hpp"
#include "scratch_dir.hpp"
#include "support/test_main.hpp"

namespace {

using opensu::retro::ControlServer;
using opensu::test::expect;
using opensu::test::ScratchDir;

std::string read(const std::filesystem::path& file) {
    std::ifstream in{file};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void announcedThenRemoved(const std::filesystem::path& dir) {
    const std::filesystem::path file = dir / "closed.json";
    {
        ControlServer control{file, "Fake", "game.gba"};
        expect(control.start(), "the channel starts");
        expect(read(file).find(R"("port":)") != std::string::npos, "the file announces the port");
    }
    expect(!std::filesystem::exists(file), "the file goes with the channel");
}

void failureOutlivesTheChannel(const std::filesystem::path& dir) {
    const std::filesystem::path file = dir / "failed.json";
    {
        ControlServer control{file, "Fake", "game.gba"};
        expect(control.start(), "the channel starts");
        control.reportFailure("the game stopped drawing: vkQueueSubmit failed");
    }
    expect(std::filesystem::exists(file), "a failure keeps the file for the shell");
    expect(read(file) == R"({"error":"the game stopped drawing: vkQueueSubmit failed"})",
           "the file holds the failure");
}

void failureBeforeAnyChannel(const std::filesystem::path& dir) {
    const std::filesystem::path file = dir / "unloaded.json";
    ControlServer::reportFailure(file, "mGBA cannot load game.gba");
    expect(read(file) == R"({"error":"mGBA cannot load game.gba"})",
           "a load failure is left in the file");
}

} // namespace

int main() try {
    const ScratchDir scratch{OPENSU_TEST_SCRATCH, "control_server"};
    announcedThenRemoved(scratch.path());
    failureOutlivesTheChannel(scratch.path());
    failureBeforeAnyChannel(scratch.path());
    std::printf("control_server: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
