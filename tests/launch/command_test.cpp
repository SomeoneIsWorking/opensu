// Short-lived children and executable lookup, against real processes.
#include "launch/command.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using iideck::launch::resolveExecutable;
using iideck::launch::runCommand;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

} // namespace

int main() {
    const fs::path scratch =
        fs::path{IIDECK_TEST_SCRATCH} / ("iideck-command-test-" + std::to_string(getpid()));
    fs::create_directories(scratch);
    std::ofstream{scratch / "plain.txt"} << "not a program";

    const std::vector<fs::path> path{scratch, "/usr/bin", "/bin"};
    expect(!resolveExecutable("sh", path).empty(), "a program on the path is found");
    expect(resolveExecutable("sh", path).filename() == "sh", "the result names the program");
    expect(resolveExecutable("plain.txt", path).empty(), "a non-executable file is not found");
    expect(resolveExecutable("/bin/sh", {}) == fs::path{"/bin/sh"}, "an absolute path is kept");
    expect(resolveExecutable("no-such-program-iideck", path).empty(), "a missing program is empty");
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

    fs::remove_all(scratch);
    std::printf("command: all checks passed\n");
    return 0;
}
