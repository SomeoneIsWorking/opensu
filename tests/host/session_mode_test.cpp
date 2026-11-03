// Session mode through the real runner with fake sudo, pkexec, busctl and systemctl on PATH: what
// counts as installed, the root step staged and run through pkexec (installed, cancelled at the
// password, refused, failed), and the order of the selector and the logout in each direction.
#include "host/session_mode.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <sys/stat.h>

#include "support/test_main.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using test::expect;

void touch(const fs::path& file) {
    const std::ofstream created{file};
}

struct Bench {
    Bench() : root{fs::path{OPENSU_TEST_SCRATCH} / "session-mode"} {
        fs::remove_all(root);
        fs::create_directories(root / "bin");
        setenv("PATH", (root / "bin").string().append(":/usr/bin:/bin").c_str(), 1);
        setenv("BENCH_DIR", root.c_str(), 1);
        fs::path log = root / "calls.log";
        setenv("BENCH_LOG", log.c_str(), 1);
        touch(log);
        script("sudo", R"(echo "sudo $*" >> "$BENCH_LOG"
if [ -e "$BENCH_DIR/select-fails" ]; then echo "sudo: a password is required" >&2; exit 1; fi)");
        // polkit's part: the player cancels, polkit refuses, or the staged installer runs, here by
        // placing what it would place.
        script("pkexec", R"(echo "pkexec $*" >> "$BENCH_LOG"
if [ -e "$BENCH_DIR/cancel" ]; then exit 126; fi
if [ -e "$BENCH_DIR/deny" ]; then
    echo "Error executing command as another user: Not authorized" >&2; exit 127
fi
echo "opensu-install: start"
if [ -e "$BENCH_DIR/installer-fails" ]; then
    echo "opensu-install: visudo rejected the sudoers rule" >&2; exit 1
fi
if [ ! -e "$BENCH_DIR/installer-does-nothing" ]; then
    stage=$(dirname "$2")
    mkdir -p "$BENCH_DIR/local-sessions"
    cp "$stage/opensu.desktop" "$BENCH_DIR/local-sessions/opensu.desktop"
    cp "$stage/opensu-session-select" "$BENCH_DIR/selector"
fi
echo "opensu-install: done")");
        script("busctl", R"(echo "busctl $*" >> "$BENCH_LOG"
if [ -e "$BENCH_DIR/logout-fails" ]; then echo "Failed to call method: no KDE here" >&2; exit 1; fi)");
        script("systemctl", R"(echo "systemctl $*" >> "$BENCH_LOG")");
    }

    void script(const std::string& name, const std::string& body) const {
        const fs::path file = root / "bin" / name;
        std::ofstream{file} << "#!/bin/sh\n" << body << "\n";
        fs::permissions(file, fs::perms::owner_all);
    }
    void flag(const char* name) const {
        touch(root / name);
    }
    void clearFlag(const char* name) const {
        fs::remove(root / name);
    }
    [[nodiscard]] std::vector<std::string> calls() const {
        std::vector<std::string> lines;
        std::ifstream in{root / "calls.log"};
        for (std::string line; std::getline(in, line);) {
            lines.push_back(line);
        }
        return lines;
    }
    void forgetCalls() const {
        const std::ofstream truncated{root / "calls.log", std::ios::trunc};
    }

    [[nodiscard]] host::SessionPaths paths() const {
        host::SessionPaths paths;
        paths.entryDirs = {root / "local-sessions", root / "sessions"};
        paths.selector = root / "selector";
        paths.program = root / "prefix" / "bin" / "opensu";
        paths.stage = root / "run" / "session-install";
        paths.pkexec = root / "bin" / "pkexec";
        paths.request = host::DesktopRequest{root / "run" / "desktop"};
        paths.compositorScope = "s-compositor.scope";
        return paths;
    }
    [[nodiscard]] std::unique_ptr<host::SessionMode> mode(bool selector = true) const {
        return mode(paths(), selector);
    }
    [[nodiscard]] std::unique_ptr<host::SessionMode> mode(host::SessionPaths given,
                                                          bool selector) const {
        if (selector) {
            touch(root / "selector");
        }
        return host::makeSddmSessionMode(host::systemRunner(), std::move(given));
    }
    void entry(const char* dir, const std::string& exec) const {
        fs::create_directories(root / dir);
        std::ofstream{root / dir / "opensu.desktop"}
            << "[Desktop Entry]\nName=openSU\nExec=" << exec << "\n";
    }

    fs::path root;
};

void installedMeansAnEntryForThisOpenSuAndTheSelector() {
    const Bench bench;
    const std::string exec = (bench.root / "prefix" / "bin" / "opensu").string() + " --session";
    const auto without = bench.mode(false);
    expect(!without->installed(), "nothing is installed at first");
    bench.entry("sessions", exec);
    expect(!without->installed(), "an entry without the selector is not session mode");
    const auto with = bench.mode();
    expect(with->installed(), "an entry in any directory SDDM reads, and the selector, is");
    fs::remove(bench.root / "sessions" / "opensu.desktop");
    bench.entry("local-sessions", exec);
    expect(with->installed(), "the first directory counts as well");
    bench.entry("local-sessions",
                (bench.root / "other" / "bin" / "opensu").string() + " --session");
    expect(!with->installed(), "an entry for another openSU is not this one");
    bench.entry("local-sessions", (bench.root / "prefix" / "bin" / "opensu").string());
    expect(!with->installed(), "an entry that does not start the session is not it");
    fs::create_directories(bench.root / "prefix" / "bin");
    fs::create_directory_symlink(bench.root / "prefix", bench.root / "linked");
    bench.entry("local-sessions",
                (bench.root / "linked" / "bin" / "opensu").string() + " --session");
    expect(with->installed(), "a symlinked path to this openSU is this openSU");
}

void theEntryStartsThisOpenSuWhereverItIs() {
    const Bench bench;
    host::SessionPaths paths = bench.paths();
    paths.program = bench.root / "my \"games\" $HOME" / "openSU.AppImage";
    const auto mode = bench.mode(paths, true);
    static_cast<void>(mode->stage());
    fs::create_directories(bench.root / "local-sessions");
    fs::copy_file(paths.stage / "opensu.desktop", bench.root / "local-sessions" / "opensu.desktop");
    expect(mode->installed(), "an entry for a path that needs quoting reads back as this openSU");
}

void installBlockerNamesWhatIsMissing() {
    const Bench bench;
    host::SessionPaths paths = bench.paths();
    paths.pkexec.clear();
    const std::string blocker = bench.mode(paths, true)->installBlocker();
    expect(blocker.find("pkexec") != std::string::npos &&
               blocker.find("sudo dnf install polkit") != std::string::npos,
           "without pkexec the blocker names the package and how to install it");
    expect(bench.mode(paths, true)->install().kind == host::InstallResult::Kind::Failed,
           "and nothing is installed");
    expect(bench.mode()->installBlocker().empty(), "with pkexec there is no blocker");
}

void theRootStepGoesThroughPkexecOverTheStagedFiles() {
    const Bench bench;
    const auto mode = bench.mode(false);
    const host::InstallResult result = mode->install();
    expect(result.kind == host::InstallResult::Kind::Installed && result.message.empty(),
           "pkexec running the staged installer installs session mode");
    const fs::path installer = bench.paths().stage / "install-session.sh";
    expect(bench.calls() == std::vector<std::string>{"pkexec /bin/sh " + installer.string()},
           "one pkexec runs the staged installer through /bin/sh");
    expect(fs::is_regular_file(installer) &&
               fs::is_regular_file(bench.paths().stage / "opensu-session-select"),
           "the installer and the selector are staged");
    expect(mode->installed(), "and session mode is in place after");
}

void cancelRefusalAndFailureAreToldApart() {
    const Bench bench;
    bench.flag("cancel");
    const host::InstallResult cancelled = bench.mode(false)->install();
    expect(cancelled.kind == host::InstallResult::Kind::Cancelled,
           "dismissing the password dialog is Cancelled");
    bench.clearFlag("cancel");
    bench.flag("deny");
    const host::InstallResult denied = bench.mode(false)->install();
    expect(denied.kind == host::InstallResult::Kind::Failed &&
               denied.message == "Error executing command as another user: Not authorized",
           "polkit's refusal is Failed with its line");
    bench.clearFlag("deny");
    bench.flag("installer-fails");
    const host::InstallResult failed = bench.mode(false)->install();
    expect(failed.kind == host::InstallResult::Kind::Failed &&
               failed.message == "opensu-install: visudo rejected the sudoers rule",
           "the installer's failure is Failed with its last line");
    bench.clearFlag("installer-fails");
    bench.flag("installer-does-nothing");
    expect(bench.mode(false)->install().kind == host::InstallResult::Kind::Failed,
           "an install that leaves nothing in place is Failed");
}

void aMissingPkexecFails() {
    const Bench bench;
    fs::remove(bench.root / "bin" / "pkexec");
    const host::InstallResult result = bench.mode(false)->install();
    expect(result.kind == host::InstallResult::Kind::Failed &&
               result.message == "pkexec could not be run",
           "a pkexec that is gone is said");
}

void switchingToSessionModeSelectsThenLogsOut() {
    const Bench bench;
    const auto mode = bench.mode();
    expect(mode->switchToSession().empty(), "switching is accepted");
    expect(bench.calls() ==
               std::vector<std::string>{"sudo -n " + bench.paths().selector.string() + " opensu",
                                        "busctl --user call org.kde.Shutdown /Shutdown "
                                        "org.kde.Shutdown logout"},
           "it selects openSU through sudo -n, then asks KDE to log out");

    bench.forgetCalls();
    bench.flag("logout-fails");
    expect(mode->switchToSession() == "Failed to call method: no KDE here",
           "a logout that fails is said");
    expect(bench.calls().size() == 3 &&
               bench.calls()[2] == "sudo -n " + bench.paths().selector.string() + " restore",
           "and the selection is undone");

    bench.forgetCalls();
    bench.clearFlag("logout-fails");
    bench.flag("select-fails");
    expect(mode->switchToSession() == "sudo: a password is required", "a refused selector is said");
    expect(bench.calls().size() == 1, "and nothing is logged out");
}

void switchingToTheDesktopRestoresThenEndsOpenSu() {
    const Bench bench;
    const auto mode = bench.mode();
    expect(mode->switchToDesktop().empty(), "switching is accepted");
    expect(bench.calls() ==
               std::vector<std::string>{"sudo -n " + bench.paths().selector.string() + " restore",
                                        "systemctl --user stop s-compositor.scope"},
           "it restores through sudo -n, then stops openSU's Gamescope");
    expect(fs::exists(bench.root / "run" / "desktop"),
           "the selector path leaves the note that marks the end as deliberate");

    bench.forgetCalls();
    bench.flag("select-fails");
    expect(mode->switchToDesktop() == "sudo: a password is required", "a refused restore is said");
    expect(bench.calls().size() == 1, "and openSU keeps running");
}

void withoutTheSelectorTheSessionStartsTheDesktopItself() {
    const Bench bench;
    const auto mode = bench.mode(false);
    expect(mode->switchToDesktop().empty(), "the fallback is accepted");
    expect(bench.calls() == std::vector<std::string>{"systemctl --user stop s-compositor.scope"},
           "it only stops the session's Gamescope");
    expect(host::DesktopRequest{bench.root / "run" / "desktop"}.consume(),
           "and leaves the note main reads");

    bench.forgetCalls();
    fs::remove_all(bench.root / "run");
    std::ofstream{bench.root / "run"} << "a file, not a directory\n";
    expect(!mode->switchToDesktop().empty() && bench.calls().empty(),
           "when the note cannot be left nothing is ended");
}

void aReadOnlyModeReadsAndRefusesEverythingElse() {
    const Bench bench;
    bench.entry("sessions", (bench.root / "prefix" / "bin" / "opensu").string() + " --session");
    const auto mode = host::makeReadOnlySessionMode(bench.mode(), "changes are off");
    expect(mode->installed(), "it still reads what is installed");
    expect(mode->switchToSession() == "changes are off" &&
               mode->switchToDesktop() == "changes are off",
           "it refuses both switches");
    const host::InstallResult result = mode->install();
    expect(result.kind == host::InstallResult::Kind::Failed && result.message == "changes are off",
           "it refuses the install");
    expect(fs::is_regular_file(bench.paths().stage / "install-session.sh"),
           "after staging the root step it would run");
    expect(bench.calls().empty(), "nothing reached the system");
}

} // namespace

int main() try {
    installedMeansAnEntryForThisOpenSuAndTheSelector();
    theEntryStartsThisOpenSuWhereverItIs();
    installBlockerNamesWhatIsMissing();
    theRootStepGoesThroughPkexecOverTheStagedFiles();
    cancelRefusalAndFailureAreToldApart();
    aMissingPkexecFails();
    switchingToSessionModeSelectsThenLogsOut();
    switchingToTheDesktopRestoresThenEndsOpenSu();
    withoutTheSelectorTheSessionStartsTheDesktopItself();
    aReadOnlyModeReadsAndRefusesEverythingElse();
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
