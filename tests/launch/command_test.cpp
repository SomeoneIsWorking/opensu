// Short-lived children and executable lookup, against real processes.
#include "launch/command.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stop_token>
#include <string>
#include <thread>

#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using opensu::launch::resolveExecutable;
using opensu::launch::runCaptured;
using opensu::launch::runCommand;
using opensu::launch::runStreaming;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

} // namespace

int main() {
    const fs::path scratch =
        fs::path{OPENSU_TEST_SCRATCH} / ("opensu-command-test-" + std::to_string(getpid()));
    fs::create_directories(scratch);
    std::ofstream{scratch / "plain.txt"} << "not a program";

    const std::vector<fs::path> path{scratch, "/usr/bin", "/bin"};
    expect(!resolveExecutable("sh", path).empty(), "a program on the path is found");
    expect(resolveExecutable("sh", path).filename() == "sh", "the result names the program");
    expect(resolveExecutable("plain.txt", path).empty(), "a non-executable file is not found");
    expect(resolveExecutable("/bin/sh", {}) == fs::path{"/bin/sh"}, "an absolute path is kept");
    expect(resolveExecutable("no-such-program-opensu", path).empty(), "a missing program is empty");
    expect(resolveExecutable("", path).empty(), "an empty name is empty");

    const std::chrono::milliseconds generous{10000};
    expect(runCommand("/bin/sh", {"-c", "exit 7"}, generous) == 7, "the exit status is returned");
    expect(runCommand("/bin/sh", {"-c", "exit 0"}, generous) == 0, "success is zero");
    expect(runCommand("/bin/sh", {"-c", "kill -9 $$"}, generous) == 137,
           "a signal is 128 plus its number");
    expect(!runCommand("/no/such/program", {}, generous).has_value(), "a missing program fails");

    const Clock::time_point began = Clock::now();
    expect(!runCommand("/bin/sleep", {"30"}, std::chrono::milliseconds{300}).has_value(),
           "a child past its bound reports nothing");
    expect(Clock::now() - began < std::chrono::seconds{5}, "the bound holds");

    std::vector<std::string> lines;
    const auto collect = [&lines](std::string_view line) {
        lines.emplace_back(line);
    };
    const std::stop_token never;
    expect(runStreaming("/bin/sh", {"-c", "echo out; echo err >&2; printf tail; exit 3"}, collect,
                        never) == 3,
           "a streamed child's status is returned");
    expect(lines == std::vector<std::string>({"out", "err", "tail"}),
           "stdout and stderr arrive as lines, an unterminated last line too");
    expect(!runStreaming("/no/such/program", {}, collect, never).has_value(),
           "a streamed missing program fails");
    const auto captured =
        runCaptured("/bin/sh", {"-c", "echo out; echo err >&2; printf tail; exit 4"});
    expect(captured.has_value(), "a captured command runs");
    if (captured) {
        expect(captured->status == 4, "a captured command's status is returned");
        expect(captured->output == "out\ntail\n", "only stdout is captured");
    }
    const auto merged = runCaptured(
        "/bin/sh", {"-c", "echo out; echo err >&2; exit 2"}, opensu::launch::CaptureErrors::Merged);
    expect(merged && merged->status == 2 && merged->output == "out\nerr\n",
           "merged capture keeps stderr in the output");
    expect(!runCaptured("/no/such/program", {}).has_value(), "a captured missing program fails");
    lines.clear();
    expect(runStreaming("/bin/sh", {"-c", "read x; echo got:$x"}, collect, never) == 0 &&
               lines == std::vector<std::string>({"got:"}),
           "a streamed child's stdin is closed");

    // Stopping ends the child and what it started.
    std::jthread stopper;
    std::stop_source source;
    stopper = std::jthread{[&source] {
        std::this_thread::sleep_for(std::chrono::milliseconds{300});
        source.request_stop();
    }};
    const Clock::time_point streamed = Clock::now();
    expect(!runStreaming("/bin/sh", {"-c", "sleep 30 & sleep 30"}, collect, source.get_token())
                .has_value(),
           "a stopped stream reports nothing");
    expect(Clock::now() - streamed < std::chrono::seconds{8},
           "stopping does not wait for the child");

    fs::remove_all(scratch);
    std::printf("command: all checks passed\n");
    return 0;
}
