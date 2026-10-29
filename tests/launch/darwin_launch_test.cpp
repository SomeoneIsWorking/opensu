// The macOS launch backends with real processes: a process-group launch with its environment,
// folder, output and exit status, a grandchild ended with the group, SIGKILL for a group that
// ignores SIGTERM, the libproc process table, and the window server's windows.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <unistd.h>

#include "launch/desktop_windows.hpp"
#include "launch/process_launcher.hpp"
#include "launch/process_table.hpp"
#include "support/scratch_dir.hpp"

namespace {

namespace fs = std::filesystem;
namespace launch = opensu::launch;
using Clock = std::chrono::steady_clock;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

bool waitUntil(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const Clock::time_point until = Clock::now() + timeout;
    while (Clock::now() < until) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
    }
    return predicate();
}

std::string readAll(const fs::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

pid_t readPid(const fs::path& path) {
    std::ifstream in{path};
    long value = 0;
    in >> value;
    return static_cast<pid_t>(value);
}

bool alive(pid_t pid) {
    return ::kill(pid, 0) == 0 && launch::table::parentOf(pid) > 0;
}

void environmentFolderOutput(const fs::path& dir) {
    launch::ProcessLauncher launcher;
    const fs::path output = dir / "output.log";
    std::string failure;
    expect(launcher.start("opensu-test-run", "/bin/sh",
                          {"-c", "echo \"$OPENSU_TEST_VALUE $(pwd -P)\"; exit 7"}, failure,
                          {"OPENSU_TEST_VALUE=seen"}, dir, output),
           "a program starts in its group");
    expect(waitUntil(
               [&launcher] {
                   return !launcher.running();
               },
               std::chrono::seconds{10}),
           "the group ends when its only process exits");
    expect(launcher.exitStatus() == 7, "the exit code is kept");
    const std::string written = readAll(output);
    expect(written == "seen " + fs::canonical(dir).string() + "\n",
           "the program sees its variable and folder and writes to the output file");
}

void grandchildEndsWithGroup(const fs::path& dir) {
    launch::ProcessLauncher launcher;
    const fs::path pidFile = dir / "grandchild.pid";
    std::string failure;
    expect(launcher.start("opensu-test-tree", "/bin/sh",
                          {"-c", "sleep 60 & echo $! > " + pidFile.string() + "; exit 0"}, failure),
           "a program that leaves a grandchild starts");
    expect(waitUntil(
               [&pidFile] {
                   return readPid(pidFile) > 0;
               },
               std::chrono::seconds{10}),
           "the grandchild recorded its pid");
    const pid_t grandchild = readPid(pidFile);
    expect(waitUntil(
               [&launcher] {
                   return !launcher.startedRunning();
               },
               std::chrono::seconds{10}),
           "the started program exits");
    expect(launcher.running(), "the group runs on while its grandchild does");
    expect(launch::table::commandLine(grandchild) == std::string{"sleep\0"
                                                                 "60\0",
                                                                 9},
           "the process table reads the grandchild's command line");
    launcher.stop();
    expect(waitUntil(
               [grandchild] {
                   return !alive(grandchild);
               },
               std::chrono::seconds{10}),
           "stop() ends the grandchild");
    expect(!launcher.running(), "nothing runs after stop()");
}

void termIgnored() {
    launch::ProcessLauncher launcher;
    std::string failure;
    expect(launcher.start("opensu-test-stubborn", "/bin/sh",
                          {"-c", "trap '' TERM; while :; do sleep 1; done"}, failure),
           "a program that ignores SIGTERM starts");
    std::this_thread::sleep_for(std::chrono::milliseconds{300});
    const Clock::time_point began = Clock::now();
    launcher.stop();
    const auto took = Clock::now() - began;
    expect(took >= std::chrono::seconds{4} && took < std::chrono::seconds{30},
           "stop() waits for SIGTERM, then kills the group");
    expect(!launcher.running(), "the stubborn group is gone");
}

void processTable() {
    expect(launch::table::available(), "macOS has a process table");
    const std::vector<launch::ProcessId> pids = launch::table::pids();
    expect(std::ranges::find(pids, getpid()) != pids.end(), "the table lists this process");
    expect(launch::table::parentOf(getpid()) == getppid(), "the parent is read");
    const std::string own = launch::table::commandLine(getpid());
    expect(own.find("darwin_launch_test") != std::string::npos && own.back() == '\0',
           "the command line is read, its arguments ended by NULs");
}

void windows() {
    launch::DesktopWindows windows;
    expect(!windows.anyOwnedBy({}), "no pids own no window");
    expect(!windows.anyOwnedBy({getpid()}), "a process with no window owns none");
}

} // namespace

int main() {
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "darwin_launch"};
    environmentFolderOutput(scratch.path());
    grandchildEndsWithGroup(scratch.path());
    termIgnored();
    processTable();
    windows();
    std::puts("darwin_launch: ok");
    return 0;
}
