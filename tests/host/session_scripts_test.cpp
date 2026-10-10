// The root-run scripts, run for real against a temp root (OPENSU_SELECT_ROOT, honoured only for a
// non-root caller): the selector's strict arguments and the files it writes, in the autologin and
// the no-autologin setups, and the installer's files and sudoers rule, checked by the real visudo.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <pwd.h>
#include <unistd.h>

#include "launch/command.hpp"
#include "ui/check.hpp"

namespace {

namespace fs = std::filesystem;
using namespace opensu;
using test::expect;

constexpr int skipped = 77;
constexpr const char* entryPath = "/usr/local/share/wayland-sessions/opensu.desktop";

struct Run {
    int status{-1};
    std::string output;
};

std::string slurp(const fs::path& file) {
    std::ifstream in{file};
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void write(const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path());
    std::ofstream{file} << text;
}

/// A fresh machine root and the scripts run against it.
struct Machine {
    explicit Machine(const std::string& name)
        : root{fs::path{OPENSU_TEST_SCRATCH} / "session-scripts" / name} {
        fs::remove_all(root);
        fs::create_directories(root);
        setenv("OPENSU_SELECT_ROOT", root.c_str(), 1);
        user = getpwuid(getuid())->pw_name;
        setenv("SUDO_USER", user.c_str(), 1);
    }

    [[nodiscard]] Run run(const std::string& script, const std::vector<std::string>& args) const {
        std::vector<std::string> argv{(fs::path{OPENSU_PACKAGING_DIR} / script).string()};
        argv.insert(argv.end(), args.begin(), args.end());
        const auto out = launch::runCaptured("/bin/sh", argv, launch::CaptureErrors::Merged);
        return out ? Run{out->status, out->output} : Run{};
    }
    [[nodiscard]] Run select(const std::vector<std::string>& args) const {
        return run("opensu-session-select.sh", args);
    }
    [[nodiscard]] fs::path at(const std::string& path) const {
        return root / path.substr(1);
    }
    void installEntry() const {
        write(at(entryPath), "[Desktop Entry]\nExec=/x/bin/opensu --session\n");
    }
    /// This machine's setup: kde_settings.conf autologin into steam-picker.
    void autologin(const std::string& autologinUser) const {
        write(at("/etc/sddm.conf.d/kde_settings.conf"),
              "[Autologin]\nRelogin=true\nSession=steam-picker\nUser=" + autologinUser +
                  "\n\n[Theme]\nCurrent=breeze\n");
    }

    fs::path root;
    std::string user;
};

void theSelectorTakesOnlyTheTwoFixedArguments() {
    const Machine machine{"arguments"};
    machine.installEntry();
    machine.autologin(machine.user);
    for (const std::vector<std::string>& bad :
         std::vector<std::vector<std::string>>{{},
                                               {"OPENSU"},
                                               {"plasma"},
                                               {""},
                                               {"opensu;id"},
                                               {"opensu restore"},
                                               {"opensu", "restore"},
                                               {"-h"},
                                               {"../opensu"}}) {
        const Run run = machine.select(bad);
        expect(run.status == 1, "a free-form or missing argument is refused");
    }
    expect(!fs::exists(machine.at("/etc/sddm.conf.d/zz-opensu-session.conf")),
           "and nothing was written");
    setenv("SUDO_USER", "", 1);
    expect(machine.select({"opensu"}).status == 1, "without sudo's user it refuses");
    for (const char* name : {"root", "a b", "x;y", "1abc", "$(id)"}) {
        setenv("SUDO_USER", name, 1);
        expect(machine.select({"opensu"}).status == 1,
               "a user name outside the pattern is refused");
    }
    setenv("SUDO_USER", machine.user.c_str(), 1);
    unsetenv("OPENSU_SELECT_ROOT");
    expect(machine.select({"opensu"}).status == 1, "a non-root caller needs the test root");
    setenv("OPENSU_SELECT_ROOT", machine.root.c_str(), 1);
}

void withAutologinTheDropInSetsTheSessionAndRestoreRemovesIt() {
    const Machine machine{"autologin"};
    machine.installEntry();
    machine.autologin(machine.user);
    const fs::path dropIn = machine.at("/etc/sddm.conf.d/zz-opensu-session.conf");
    const fs::path memory = machine.at("/var/lib/opensu/previous-session");

    const Run first = machine.select({"opensu"});
    expect(first.status == 0, "selecting openSU succeeds");
    expect(slurp(dropIn) == "# Written by opensu-session-select; \"restore\" removes it.\n"
                            "[Autologin]\nSession=opensu\n",
           "the drop-in sets only the autologin session");
    expect(slurp(memory) == "autologin=steam-picker\nlast=\n",
           "the previous autologin session is remembered");
    expect(!fs::exists(machine.at("/var/lib/sddm/state.conf")),
           "the remembered last session is not touched when autologin is on");

    write(machine.at("/etc/sddm.conf.d/kde_settings.conf"),
          "[Autologin]\nSession=changed-later\nUser=" + machine.user + "\n");
    expect(machine.select({"opensu"}).status == 0, "selecting twice succeeds");
    expect(slurp(memory) == "autologin=steam-picker\nlast=\n", "only the first time is remembered");

    expect(machine.select({"restore"}).status == 0, "restore succeeds");
    expect(!fs::exists(dropIn) && !fs::exists(memory),
           "restore removes the drop-in and the memory");
    expect(machine.select({"restore"}).status == 0, "restoring twice is fine");
}

void autologinForAnotherUserOrFromTheMainFileIsRefused() {
    const Machine other{"autologin-other"};
    other.installEntry();
    other.autologin("somebody");
    const Run run = other.select({"opensu"});
    expect(run.status == 1 && run.output.find("another user") != std::string::npos,
           "autologin for someone else is not changed");
    expect(!fs::exists(other.at("/etc/sddm.conf.d/zz-opensu-session.conf")), "nothing was written");

    const Machine main{"autologin-main"};
    main.installEntry();
    main.autologin(main.user);
    write(main.at("/etc/sddm.conf"), "[Autologin]\nSession=plasma\n");
    const Run beaten = main.select({"opensu"});
    expect(beaten.status == 1 && beaten.output.find("/etc/sddm.conf") != std::string::npos,
           "a Session= in /etc/sddm.conf, which beats drop-ins, is refused");

    const Machine missing{"entry-missing"};
    missing.autologin(missing.user);
    expect(missing.select({"opensu"}).status == 1, "without the session entry it refuses");
}

void withoutAutologinTheRememberedLastSessionIsSetAndPutBack() {
    const Machine machine{"last-session"};
    machine.installEntry();
    const fs::path state = machine.at("/var/lib/sddm/state.conf");
    write(state,
          "[Last]\nUser=" + machine.user +
              "\nSession=/usr/share/wayland-sessions/plasma.desktop\n\n[Other]\nSession=keep\n");

    expect(machine.select({"opensu"}).status == 0, "selecting openSU succeeds");
    expect(slurp(state) == "[Last]\nUser=" + machine.user + "\nSession=" + entryPath +
                               "\n\n[Other]\nSession=keep\n",
           "SDDM's last session is openSU's entry, and nothing else changed");
    expect(!fs::exists(machine.at("/etc/sddm.conf.d/zz-opensu-session.conf")),
           "no drop-in without autologin");
    expect(slurp(machine.at("/var/lib/opensu/previous-session")) ==
               "autologin=\nlast=/usr/share/wayland-sessions/plasma.desktop\n",
           "the previous last session is remembered");

    expect(machine.select({"restore"}).status == 0, "restore succeeds");
    expect(slurp(state) == "[Last]\nUser=" + machine.user +
                               "\nSession=/usr/share/wayland-sessions/plasma.desktop\n\n"
                               "[Other]\nSession=keep\n",
           "the last session is put back");

    const Machine fresh{"last-session-fresh"};
    fresh.installEntry();
    expect(fresh.select({"opensu"}).status == 0, "selecting works with no state file");
    expect(slurp(fresh.at("/var/lib/sddm/state.conf")) ==
               std::string{"[Last]\nSession="} + entryPath + "\n",
           "the state file is created");
    expect(fresh.select({"restore"}).status == 0, "restore succeeds");
    expect(slurp(fresh.at("/var/lib/sddm/state.conf")) == "[Last]\n",
           "and the key it added is gone");
}

void restoreLeavesALastSessionThePlayerChoseAlone() {
    const Machine machine{"last-session-chosen"};
    machine.installEntry();
    expect(machine.select({"opensu"}).status == 0, "selecting openSU succeeds");
    write(machine.at("/var/lib/sddm/state.conf"),
          "[Last]\nSession=/usr/share/xsessions/other.desktop\n");
    expect(machine.select({"restore"}).status == 0, "restore succeeds");
    expect(slurp(machine.at("/var/lib/sddm/state.conf")) ==
               "[Last]\nSession=/usr/share/xsessions/other.desktop\n",
           "a session chosen at the login screen since is not overwritten");
}

void theInstallerPlacesTheThreeFilesAndAValidSudoersRule() {
    const Machine machine{"installer"};
    const fs::path tree = machine.root / "tree";
    const fs::path libexec = tree / "libexec" / "opensu";
    fs::create_directories(libexec);
    fs::copy_file(fs::path{OPENSU_PACKAGING_DIR} / "install-session.sh",
                  libexec / "install-session.sh");
    fs::copy_file(fs::path{OPENSU_PACKAGING_DIR} / "opensu-session-select.sh",
                  libexec / "opensu-session-select");
    write(tree / "share" / "wayland-sessions" / "opensu.desktop",
          "[Desktop Entry]\nExec=" + tree.string() + "/bin/opensu --session\n");

    const auto run = [&](const fs::path& script) {
        const auto out =
            launch::runCaptured("/bin/sh", {script.string()}, launch::CaptureErrors::Merged);
        return out ? Run{out->status, out->output} : Run{};
    };
    const Run done = run(libexec / "install-session.sh");
    expect(done.status == 0, "the installer succeeds");
    expect(done.output.starts_with("opensu-install: start\n") &&
               done.output.ends_with("opensu-install: done\n"),
           "it opens with the marker and ends done");
    expect(slurp(machine.at("/usr/local/share/wayland-sessions/opensu.desktop")) ==
               slurp(tree / "share" / "wayland-sessions" / "opensu.desktop"),
           "the session entry is installed where SDDM reads it");
    expect(slurp(machine.at("/usr/local/libexec/opensu-session-select")) ==
               slurp(libexec / "opensu-session-select"),
           "the selector is installed");
    expect((fs::status(machine.at("/usr/local/libexec/opensu-session-select")).permissions() &
            fs::perms::others_write) == fs::perms::none,
           "and nobody else can write it");
    const fs::path rule = machine.at("/etc/sudoers.d/zz-opensu-session-select");
    const std::string selector = "/usr/local/libexec/opensu-session-select";
    expect(slurp(rule) == "# Installed by openSU: " + machine.user +
                              " may switch the login session between openSU and the desktop.\n" +
                              machine.user + " ALL=(root) NOPASSWD: " + selector + " opensu, " +
                              selector + " restore\n",
           "the sudoers rule names the selector with each fixed argument");
    expect(fs::status(rule).permissions() == (fs::perms::owner_read | fs::perms::group_read),
           "it is mode 0440");
    const auto checked = launch::runCaptured("/usr/sbin/visudo", {"-cf", rule.string()},
                                             launch::CaptureErrors::Merged);
    expect(checked && checked->status == 0, "the real visudo accepts the rule");
    expect(!fs::exists(machine.at("/etc/sudoers.d/.zz-opensu-session-select.new")),
           "no staging file is left");

    setenv("SUDO_USER", "no such user!", 1);
    expect(run(libexec / "install-session.sh").status == 1, "a bad user name is refused");
    setenv("SUDO_USER", machine.user.c_str(), 1);
    fs::remove(tree / "share" / "wayland-sessions" / "opensu.desktop");
    const Run missing = run(libexec / "install-session.sh");
    expect(missing.status == 1 && missing.output.find("cmake --install") != std::string::npos,
           "without the installed entry it says to run cmake --install");
}

} // namespace

int main() {
    if (geteuid() == 0) {
        std::fprintf(stderr, "SKIP: the scripts honour the test root only for a non-root caller\n");
        return skipped;
    }
    theSelectorTakesOnlyTheTwoFixedArguments();
    withAutologinTheDropInSetsTheSessionAndRestoreRemovesIt();
    autologinForAnotherUserOrFromTheMainFileIsRefused();
    withoutAutologinTheRememberedLastSessionIsSetAndPutBack();
    restoreLeavesALastSessionThePlayerChoseAlone();
    theInstallerPlacesTheThreeFilesAndAValidSudoersRule();
    return 0;
}
