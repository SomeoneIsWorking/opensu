// Session mode through the real runners with fake sudo, busctl and systemctl on PATH: what counts
// as installed, the install's password handling (wrong password, then right; never in an argument
// or the output), and the order of the selector and the logout in each direction.
#include "host/session_mode.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/stat.h>

#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using test::expect;

constexpr const char* rightPassword = "s3cret-pw";
constexpr const char* wrongPassword = "wr0ng-pw";

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
case "$1" in
-S)
    read pw
    if [ "$pw" != s3cret-pw ]; then echo "Sorry, try again." >&2; exit 1; fi
    echo "opensu-install: start"
    if [ -e "$BENCH_DIR/installer-fails" ]; then
        echo "opensu-install: visudo rejected the sudoers rule" >&2
        exit 1
    fi
    exit 0 ;;
-n)
    if [ -e "$BENCH_DIR/select-fails" ]; then echo "sudo: a password is required" >&2; exit 1; fi
    exit 0 ;;
esac)");
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
    [[nodiscard]] std::string everything() const {
        std::ostringstream all;
        for (const std::string& line : calls()) {
            all << line << '\n';
        }
        return all.str();
    }

    [[nodiscard]] host::SessionPaths paths() const {
        host::SessionPaths paths;
        paths.entryDirs = {root / "local-sessions", root / "sessions"};
        paths.selector = root / "selector";
        paths.installer = root / "prefix" / "libexec" / "opensu" / "install-session.sh";
        paths.executable = root / "prefix" / "bin" / "opensu";
        paths.request = host::DesktopRequest{root / "run" / "desktop"};
        paths.compositorScope = "s-compositor.scope";
        return paths;
    }
    [[nodiscard]] std::unique_ptr<host::SessionMode> mode(bool selector = true) const {
        if (selector) {
            touch(root / "selector");
        }
        return host::makeSddmSessionMode(host::systemRunner(), host::systemLineRunner(), paths());
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

void installBlockerNamesAMissingInstaller() {
    const Bench bench;
    const auto mode = bench.mode();
    expect(mode->installBlocker().find("cmake --install") != std::string::npos,
           "without the installer the blocker says to install openSU");
    fs::create_directories(bench.paths().installer.parent_path());
    touch(bench.paths().installer);
    expect(mode->installBlocker().empty(), "with the installer there is no blocker");
}

void aWrongPasswordIsRefusedAndTheRightOneInstalls() {
    const Bench bench;
    const auto mode = bench.mode(false);
    const host::InstallResult wrong = mode->install(wrongPassword);
    expect(wrong.kind == host::InstallResult::Kind::Refused, "a wrong password is refused");
    expect(wrong.message == "Sorry, try again.", "the refusal is sudo's own line");
    const host::InstallResult right = mode->install(rightPassword);
    expect(right.kind == host::InstallResult::Kind::Installed && right.message.empty(),
           "the right password installs on the retry");
    const std::string all = bench.everything();
    expect(all.find(rightPassword) == std::string::npos &&
               all.find(wrongPassword) == std::string::npos,
           "no password was in any argument");
    expect(bench.calls().size() == 2 &&
               bench.calls()[0] == "sudo -S -k -p  /bin/sh " + bench.paths().installer.string(),
           "one sudo -S -k per attempt runs the installer through /bin/sh");
    expect(wrong.message.find(wrongPassword) == std::string::npos,
           "the message never carries the password");
}

void anInstallerFailureIsNotAWrongPassword() {
    const Bench bench;
    bench.flag("installer-fails");
    const host::InstallResult failed = bench.mode(false)->install(rightPassword);
    expect(failed.kind == host::InstallResult::Kind::Failed, "the installer's failure is Failed");
    expect(failed.message == "opensu-install: visudo rejected the sudoers rule",
           "its last line is the reason");
}

void aMissingSudoFails() {
    const Bench bench;
    fs::remove(bench.root / "bin" / "sudo");
    setenv("PATH", (bench.root / "bin").c_str(), 1);
    const host::InstallResult result = bench.mode(false)->install(rightPassword);
    expect(result.kind == host::InstallResult::Kind::Failed &&
               result.message == "sudo could not be run",
           "a missing sudo is said");
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
    expect(!fs::exists(bench.root / "run" / "desktop"), "the selector path leaves no note");

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
    const host::InstallResult result = mode->install(rightPassword);
    expect(result.kind == host::InstallResult::Kind::Failed && result.message == "changes are off",
           "it refuses the install");
    expect(bench.calls().empty(), "nothing reached the system");
}

} // namespace

int main() {
    installedMeansAnEntryForThisOpenSuAndTheSelector();
    installBlockerNamesAMissingInstaller();
    aWrongPasswordIsRefusedAndTheRightOneInstalls();
    anInstallerFailureIsNotAWrongPassword();
    aMissingSudoFails();
    switchingToSessionModeSelectsThenLogsOut();
    switchingToTheDesktopRestoresThenEndsOpenSu();
    withoutTheSelectorTheSessionStartsTheDesktopItself();
    aReadOnlyModeReadsAndRefusesEverythingElse();
    return 0;
}
