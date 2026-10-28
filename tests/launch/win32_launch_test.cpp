// The Windows launch backends with real processes: commands through cmd.exe, a Job Object launch
// that owns its grandchild, and the Toolhelp process table. The test runs itself as the children.
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

#include "launch/command.hpp"
#include "launch/process_launcher.hpp"
#include "launch/process_table.hpp"
#include "launch/process_tree.hpp"
#include "launch/win32.hpp"
#include "support/scratch_dir.hpp"

namespace {

using Clock = std::chrono::steady_clock;
namespace launch = opensu::launch;

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

std::string ownPath() {
    std::wstring path(MAX_PATH, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(size);
    return launch::win32::narrow(path);
}

std::string readAll(const std::filesystem::path& file) {
    std::ifstream in{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// Child roles: "sleep <tag>" waits a minute; "spawn <tag>" starts "sleep <tag>" and then exits.
int child(const std::string& role, const std::string& tag) {
    if (role == "sleep") {
        Sleep(60'000);
        return 0;
    }
    std::wstring line = launch::win32::commandLine(ownPath(), {"sleep", tag});
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    if (CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup,
                       &info) == 0) {
        return 2;
    }
    CloseHandle(info.hThread);
    CloseHandle(info.hProcess);
    return 0;
}

void commands() {
    const launch::Captured echoed = launch::runCaptured("cmd.exe", {"/d", "/c", "echo one two"})
                                        .value_or(launch::Captured{-1, {}});
    expect(echoed.status == 0, "cmd echo runs");
    expect(echoed.output.find("one two") != std::string::npos, "cmd echo is captured");
    expect(launch::runCommand("cmd.exe", {"/d", "/c", "exit 3"}, std::chrono::seconds{30}) == 3,
           "runCommand returns the exit code");
}

void table() {
    const std::string tag = "table-" + std::to_string(launch::ownProcessId());
    std::wstring line = launch::win32::commandLine(ownPath(), {"sleep", tag});
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    expect(CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                          &startup, &info) != 0,
           "a sleeping child starts");
    const launch::win32::Handle process{info.hProcess};
    const launch::win32::Handle thread{info.hThread};
    const auto child = static_cast<launch::ProcessId>(info.dwProcessId);
    expect(launch::table::parentOf(child) == launch::ownProcessId(), "the child's parent is us");
    const std::string args = launch::table::commandLine(child);
    expect(args.find(std::string{"sleep"} + '\0' + tag + '\0') != std::string::npos,
           "the child's command line is read and split");
    expect(launch::ProcessTree::matching(tag) == std::vector{child}, "the child is found by tag");
    expect(launch::table::kill(child), "the child is killed");
    expect(WaitForSingleObject(process.get(), 10'000) == WAIT_OBJECT_0, "the child is gone");
}

void exitCodeAndOutput(const std::filesystem::path& scratch) {
    const std::filesystem::path output = scratch / "output.txt";
    launch::ProcessLauncher launcher;
    std::string failure;
    expect(launcher.start("opensu-test-exit", "cmd.exe",
                          {"/d", "/c", "echo %OPENSU_TEST_SET%& exit 7"}, failure,
                          {"OPENSU_TEST_SET=from-the-launcher"}, scratch, output),
           "cmd starts in a job");
    expect(waitUntil(
               [&launcher] {
                   return !launcher.running();
               },
               std::chrono::seconds{30}),
           "the job empties when cmd exits");
    expect(launcher.exitStatus() == 7, "the exit code is recorded");
    expect(readAll(output).find("from-the-launcher") != std::string::npos,
           "the added variable reaches the child and its output the file");
}

void jobOwnsGrandchild() {
    const std::string tag = "job-tree-" + std::to_string(launch::ownProcessId());
    launch::ProcessLauncher launcher;
    std::string failure;
    expect(launcher.start("opensu-test-tree", ownPath(), {"spawn", tag}, failure),
           "the spawner starts in a job");
    expect(waitUntil(
               [&launcher] {
                   return !launcher.startedRunning();
               },
               std::chrono::seconds{30}),
           "the spawner exits");
    expect(launcher.exitStatus() == 0, "the spawner started its child");
    expect(launch::ProcessTree::anyMatches(tag), "the grandchild runs");
    expect(launcher.running(), "the job is running while the grandchild is");
    launcher.stop();
    expect(!launcher.running(), "stop empties the job");
    expect(waitUntil(
               [&tag] {
                   return !launch::ProcessTree::anyMatches(tag);
               },
               std::chrono::seconds{10}),
           "the grandchild is gone");
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 3) {
        return child(argv[1], argv[2]);
    }
    const opensu::test::ScratchDir scratch{OPENSU_TEST_SCRATCH, "win32-launch"};
    commands();
    table();
    exitCodeAndOutput(scratch.path());
    jobOwnsGrandchild();
    std::puts("win32 launch: ok");
    return 0;
}
