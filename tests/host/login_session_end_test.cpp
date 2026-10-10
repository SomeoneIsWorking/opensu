// How a login session's end is judged, through the real runner with a fake sudo on PATH: an
// abnormal end runs `sudo -n <selector> restore`, a requested or deliberate one does not, and a
// restore that fails is reported rather than swallowed.
#include "host/login_session_end.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>

#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using Ending = host::LoginSessionEnd::Ending;
using test::expect;

struct Bench {
    Bench() : root{fs::path{OPENSU_TEST_SCRATCH} / "login-session-end"} {
        fs::remove_all(root);
        fs::create_directories(root / "bin");
        setenv("PATH", (root / "bin").string().append(":/usr/bin:/bin").c_str(), 1);
        setenv("BENCH_DIR", root.c_str(), 1);
        std::ofstream{root / "bin" / "sudo"} << "#!/bin/sh\n"
                                                "echo \"sudo $*\" >> \"$BENCH_DIR/calls.log\"\n"
                                                "if [ -e \"$BENCH_DIR/restore-fails\" ]; then\n"
                                                "    echo 'sudo: a password is required' >&2\n"
                                                "    exit 1\n"
                                                "fi\n";
        fs::permissions(root / "bin" / "sudo", fs::perms::owner_all);
    }

    void installSelector() const {
        const std::ofstream created{selector()};
    }
    [[nodiscard]] fs::path selector() const {
        return root / "selector";
    }
    [[nodiscard]] host::DesktopRequest request() const {
        return host::DesktopRequest{root / "run" / "desktop"};
    }
    [[nodiscard]] host::LoginSessionEnd end(std::chrono::seconds window = std::chrono::seconds{
                                                0}) const {
        return host::LoginSessionEnd{host::systemRunner(), selector(), request(), window};
    }
    [[nodiscard]] std::vector<std::string> calls() const {
        std::vector<std::string> lines;
        std::ifstream in{root / "calls.log"};
        for (std::string line; std::getline(in, line);) {
            lines.push_back(line);
        }
        return lines;
    }
    [[nodiscard]] std::string restoreCall() const {
        return "sudo -n " + selector().string() + " restore";
    }

    fs::path root;
};

void anAbnormalEndRestoresThePreviousSession() {
    for (const int status : {1, 3, 134, 139}) {
        const Bench bench;
        bench.installSelector();
        host::LoginSessionEnd end = bench.end();
        expect(end.settle(status, false) == Ending::Restored, "a failing Gamescope restores");
        expect(bench.calls() == std::vector<std::string>{bench.restoreCall()},
               "exactly one sudo -n selector restore ran");
    }
}

void aFailedRestoreIsReported() {
    const Bench bench;
    bench.installSelector();
    std::ofstream{bench.root / "restore-fails"}.close();
    host::LoginSessionEnd end = bench.end();
    expect(end.settle(1, false) == Ending::RestoreFailed, "a refused restore is not silent");
    expect(end.failure() == "sudo: a password is required", "the reason is sudo's line");
}

void aQuickCleanEndIsAFailureAndASlowOneIsNot() {
    const Bench bench;
    bench.installSelector();
    host::LoginSessionEnd quick = bench.end(std::chrono::seconds{3600});
    expect(quick.settle(0, false) == Ending::Restored, "exit 0 inside the quick window restores");

    const Bench slow;
    slow.installSelector();
    host::LoginSessionEnd end = slow.end(std::chrono::seconds{0});
    expect(end.settle(0, false) == Ending::Ordinary, "a clean end after the window is left alone");
    expect(slow.calls().empty(), "and runs nothing");
}

void aRequestedStopIsNotAFailure() {
    const Bench bench;
    bench.installSelector();
    host::LoginSessionEnd end = bench.end(std::chrono::seconds{3600});
    expect(end.settle(143, true) == Ending::Ordinary,
           "a logout or shutdown signal is not a failure, even inside the window");
    expect(bench.calls().empty(), "and the autologin choice stays");
}

void aDeliberateSwitchToDesktopIsNotRestoredAgain() {
    const Bench bench;
    bench.installSelector();
    expect(bench.request().write().empty(), "the note is written");
    host::LoginSessionEnd end = bench.end(std::chrono::seconds{3600});
    expect(end.settle(143, false) == Ending::SwitchedToDesktop,
           "the note makes the end deliberate");
    expect(bench.calls().empty(), "the selector already ran, so nothing runs");
    expect(!bench.request().consume(), "the note is consumed");
}

void withoutTheSelectorNothingIsChanged() {
    const Bench bench;
    host::LoginSessionEnd failing = bench.end();
    expect(failing.settle(1, false) == Ending::NothingToRestore,
           "no selector means no autologin change of ours to undo");
    expect(bench.calls().empty(), "and no sudo runs");

    expect(bench.request().write().empty(), "the note is written");
    host::LoginSessionEnd deliberate = bench.end();
    expect(deliberate.settle(143, false) == Ending::StartDesktop,
           "a deliberate end without the selector starts the desktop in place");
}

} // namespace

int main() {
    fs::create_directories(OPENSU_TEST_SCRATCH);
    anAbnormalEndRestoresThePreviousSession();
    aFailedRestoreIsReported();
    aQuickCleanEndIsAFailureAndASlowOneIsNot();
    aRequestedStopIsNotAFailure();
    aDeliberateSwitchToDesktopIsNotRestoredAgain();
    withoutTheSelectorNothingIsChanged();
    return 0;
}
