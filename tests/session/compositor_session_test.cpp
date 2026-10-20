// The nested session against a fake `gamescope` in real systemd scopes: what it is
// started with, that a leftover scope of the session is stopped when it ends, that
// SIGTERM ends the session rather than this process, and that a crashed Gamescope is
// started again within the restart limit but not after a requested end.
#include "session/compositor_session.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <pthread.h>
#include <unistd.h>

#include "launch/command.hpp"
#include "launch/instance.hpp"

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using opensu::host::DesktopRequest;
using opensu::session::CompositorSession;
using opensu::session::gamescopeArgs;
using opensu::session::Output;
using opensu::session::RestartBudget;
using opensu::session::RestartLimit;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

std::string slurp(const fs::path& path) {
    std::ifstream in{path};
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
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

bool scopeActive(const std::string& unit) {
    return opensu::launch::runCommand("systemctl", {"--user", "is-active", "--quiet", unit},
                                      std::chrono::seconds{20}) == 0;
}

/// A fake gamescope body that counts its starts in `starts` beside its bin directory, crashes on
/// SIGABRT for the first `crashes` of them and then runs `then`.
std::string crashing(int crashes, const std::string& then) {
    return "starts=\"$(dirname \"$0\")/../starts\"\n"
           "n=$(cat \"$starts\" 2>/dev/null || echo 0)\n"
           "n=$((n + 1))\n"
           "echo \"$n\" > \"$starts\"\n"
           "[ \"$n\" -le " +
           std::to_string(crashes) + " ] && kill -ABRT $$\n" + then;
}

int starts(const fs::path& dir) {
    std::ifstream in{dir / "starts"};
    int count = 0;
    in >> count;
    return count;
}

struct Fixture {
    fs::path base;
    fs::path bin;
    fs::path argsFile;
    fs::path envFile;
    fs::path desktopNote;
    std::string session;

    Fixture(const std::string& stem, const std::string& tail) {
        session = "opensu-test-" + stem + "-" + std::to_string(getpid());
        base = fs::path{OPENSU_TEST_SCRATCH} / session;
        fs::remove_all(base);
        bin = base / "bin";
        fs::create_directories(bin);
        argsFile = base / "args";
        envFile = base / "env";
        desktopNote = base / "desktop";
        const fs::path program = bin / "gamescope";
        std::ofstream{program}
            << "#!/bin/sh\n"
               "printf '%s\\n' \"$@\" > \"" +
                   argsFile.string() +
                   "\"\n"
                   "echo \"$OPENSU_SESSION ${DISABLE_GAMESCOPE_WSI-unset}\" > \"" +
                   envFile.string() + "\"\n" + tail;
        fs::permissions(program, fs::perms::owner_all);
    }

    ~Fixture() {
        fs::remove_all(base);
    }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    [[nodiscard]] CompositorSession compositor(RestartLimit limit = {}) const {
        return CompositorSession{session, bin / "gamescope", DesktopRequest{desktopNote}, limit};
    }
};

/// Arguments and environment reach Gamescope, its exit status comes back, and a scope
/// of the session that is still up is stopped.
void testRunPassesArgumentsAndStopsLeftovers() {
    const Fixture fixture{"run", "exit 3\n"};
    const std::string steamUnit = fixture.session + "-steam.scope";
    opensu::launch::Instance leftover;
    std::string failure;
    expect(leftover.start(steamUnit, "/bin/sleep", {"600"}, failure), "the leftover scope starts");

    // No restarts: the failing status under test would otherwise start it again.
    CompositorSession session = fixture.compositor(RestartLimit{0, std::chrono::seconds{120}});
    const Output output{2560, 1440, 144};
    const int status = session.run(output, {"--flag"});

    expect(status == 3, "Gamescope's exit status is returned");
    expect(slurp(fixture.envFile) == fixture.session + " 1\n",
           "OPENSU_SESSION names the session and the Gamescope WSI layer is off for the shell");

    std::error_code ec;
    const std::string self = fs::read_symlink("/proc/self/exe", ec).string();
    std::string expected;
    for (const std::string& arg : gamescopeArgs(output, self, {"--flag"})) {
        expected += arg + "\n";
    }
    expect(slurp(fixture.argsFile) == expected,
           "Gamescope wraps this executable and its arguments");

    expect(!scopeActive(steamUnit), "a leftover scope of the session was stopped");
    expect(!scopeActive(fixture.session + "-compositor.scope"), "the compositor scope is gone");
}

/// The login session's top-level Gamescope: DRM backend at the native mode, `--session` handed on.
void testLoginSessionIsTopLevel() {
    const Fixture fixture{"login", "exit 0\n"};
    CompositorSession session = fixture.compositor();
    expect(session.run(Output{}, {"--session"}) == 0, "the top-level run ends with Gamescope");
    expect(slurp(fixture.envFile) == fixture.session + " 1\n",
           "OPENSU_SESSION names the session and the Gamescope WSI layer is off for the shell");

    std::error_code ec;
    const std::string self = fs::read_symlink("/proc/self/exe", ec).string();
    expect(slurp(fixture.argsFile) == "--backend\ndrm\n--\n" + self + "\n--session\n",
           "Gamescope is told the DRM backend, no size, and runs this executable --session");
    expect(!scopeActive(fixture.session + "-compositor.scope"), "the compositor scope is gone");
}

/// A missing gamescope binary is refused, not run.
void testMissingGamescope() {
    CompositorSession session{"opensu-test-missing-" + std::to_string(getpid()),
                              "/nonexistent/gamescope", DesktopRequest{"/nonexistent/desktop"}};
    expect(session.run(Output{1280, 720, 0}, {}) == 1, "a missing gamescope fails");
}

/// SIGTERM stops the session through its scopes; this process lives.
void testSignalStopsTheSession() {
    const Fixture fixture{"signal", crashing(0, "exec sleep 600\n")};
    const std::string steamUnit = fixture.session + "-steam.scope";
    opensu::launch::Instance leftover;
    std::string failure;
    expect(leftover.start(steamUnit, "/bin/sleep", {"600"}, failure), "the leftover scope starts");

    // Blocked here before the worker exists, so the worker inherits it and the
    // signal stays pending until the session takes it.
    sigset_t term;
    sigemptyset(&term);
    sigaddset(&term, SIGTERM);
    sigset_t previous;
    pthread_sigmask(SIG_BLOCK, &term, &previous);

    CompositorSession session = fixture.compositor();
    std::future<int> done = std::async(std::launch::async, [&] {
        return session.run(Output{1280, 720, 0}, {});
    });
    // The start count is the stand-in's last write before it sleeps.
    expect(waitUntil(
               [&fixture] {
                   return starts(fixture.base) == 1;
               },
               std::chrono::seconds{20}),
           "Gamescope started");
    kill(getpid(), SIGTERM);
    expect(done.wait_for(std::chrono::seconds{30}) == std::future_status::ready,
           "the session ended on SIGTERM");
    done.get();
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);

    expect(!scopeActive(fixture.session + "-compositor.scope"), "the compositor scope is gone");
    expect(!scopeActive(steamUnit), "the leftover scope is gone");
    expect(session.stopRequested(), "the end was requested");
    expect(starts(fixture.base) == 1, "a requested stop is not restarted");
}

/// A Gamescope that crashes is started again, and the leftovers of the crashed one are stopped
/// before the fresh one starts.
void testCrashedGamescopeRestarts() {
    const Fixture fixture{"restart", crashing(2, "exit 0\n")};
    const std::string steamUnit = fixture.session + "-steam.scope";
    opensu::launch::Instance leftover;
    std::string failure;
    expect(leftover.start(steamUnit, "/bin/sleep", {"600"}, failure), "the leftover scope starts");

    CompositorSession session = fixture.compositor();
    expect(session.run(Output{1280, 720, 0}, {}) == 0, "the session ends with the last Gamescope");
    expect(starts(fixture.base) == 3, "two crashes, two restarts");
    expect(!session.stopRequested(), "nobody asked for the end");
    expect(!scopeActive(steamUnit), "the crashed session's leftover scope was stopped");
}

/// Past the limit the session gives up with the crash's status.
void testRestartsAreCapped() {
    const Fixture fixture{"cap", crashing(100, "exit 0\n")};
    CompositorSession session = fixture.compositor(RestartLimit{2, std::chrono::seconds{120}});
    expect(session.run(Output{1280, 720, 0}, {}) == 128 + SIGABRT,
           "the session gives up with the crash's status");
    expect(starts(fixture.base) == 3, "the first start and two restarts");
    expect(!scopeActive(fixture.session + "-compositor.scope"), "the compositor scope is gone");
}

/// A Gamescope that ends badly while Switch to desktop stops it is not started again.
void testDesktopRequestIsNotRestarted() {
    const Fixture fixture{"desktop", crashing(100, "exit 0\n")};
    std::ofstream{fixture.desktopNote} << "desktop\n";
    CompositorSession session = fixture.compositor();
    expect(session.run(Output{1280, 720, 0}, {}) == 128 + SIGABRT, "the crash's status comes back");
    expect(starts(fixture.base) == 1, "no restart on the way to the desktop");
    expect(fs::exists(fixture.desktopNote), "the note is left for the login session");
}

/// The limit counts restarts within the window before each one.
void testRestartBudgetWindow() {
    using std::chrono::seconds;
    RestartBudget budget{RestartLimit{3, seconds{120}}};
    const RestartBudget::Clock::time_point start{};
    expect(budget.spend(start), "first restart");
    expect(budget.spend(start + seconds{10}), "second restart");
    expect(budget.spend(start + seconds{20}), "third restart");
    expect(!budget.spend(start + seconds{119}),
           "a fourth within two minutes of the first is refused");
    expect(budget.spend(start + seconds{120}), "the first has left the window");
    expect(!budget.spend(start + seconds{121}), "three again within the window");
    expect(budget.spend(start + seconds{400}), "a quiet spell refills the budget");
}

} // namespace

int main() {
    fs::create_directories(OPENSU_TEST_SCRATCH);
    testRunPassesArgumentsAndStopsLeftovers();
    testLoginSessionIsTopLevel();
    testMissingGamescope();
    testSignalStopsTheSession();
    testCrashedGamescopeRestarts();
    testRestartsAreCapped();
    testDesktopRequestIsNotRestarted();
    testRestartBudgetWindow();
    std::printf("compositor_session: all checks passed\n");
    return 0;
}
