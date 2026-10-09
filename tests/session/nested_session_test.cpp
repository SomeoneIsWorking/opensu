// The nested session against a fake `gamescope` in real systemd scopes: what it is
// started with, that a leftover scope of the session is stopped when it ends, and
// that SIGTERM ends the session rather than this process.
#include "session/nested_session.hpp"

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
using opensu::session::gamescopeArgs;
using opensu::session::NestedSession;
using opensu::session::Output;

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

struct Fixture {
    fs::path base;
    fs::path bin;
    fs::path argsFile;
    fs::path envFile;
    std::string session;

    Fixture(const std::string& stem, const std::string& tail) {
        session = "opensu-test-" + stem + "-" + std::to_string(getpid());
        base = fs::path{OPENSU_TEST_SCRATCH} / session;
        fs::remove_all(base);
        bin = base / "bin";
        fs::create_directories(bin);
        argsFile = base / "args";
        envFile = base / "env";
        const fs::path program = bin / "gamescope";
        std::ofstream{program} << "#!/bin/sh\n"
                                  "printf '%s\\n' \"$@\" > \"" +
                                      argsFile.string() +
                                      "\"\n"
                                      "echo \"$OPENSU_SESSION\" > \"" +
                                      envFile.string() + "\"\n" + tail;
        fs::permissions(program, fs::perms::owner_all);
    }

    ~Fixture() {
        fs::remove_all(base);
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

    NestedSession session{fixture.session, fixture.bin / "gamescope"};
    const Output output{2560, 1440, 144};
    const int status = session.run(output, {"--flag"});

    expect(status == 3, "Gamescope's exit status is returned");
    expect(slurp(fixture.envFile) == fixture.session + "\n", "OPENSU_SESSION names the session");

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

/// A missing gamescope binary is refused, not run.
void testMissingGamescope() {
    NestedSession session{"opensu-test-missing-" + std::to_string(getpid()),
                          "/nonexistent/gamescope"};
    expect(session.run(Output{1280, 720, 0}, {}) == 1, "a missing gamescope fails");
}

/// SIGTERM stops the session through its scopes; this process lives.
void testSignalStopsTheSession() {
    const Fixture fixture{"signal", "exec sleep 600\n"};
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

    NestedSession session{fixture.session, fixture.bin / "gamescope"};
    std::future<int> done = std::async(std::launch::async, [&] {
        return session.run(Output{1280, 720, 0}, {});
    });
    expect(waitUntil(
               [&fixture] {
                   return fs::exists(fixture.envFile) && !slurp(fixture.envFile).empty();
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
}

} // namespace

int main() {
    fs::create_directories(OPENSU_TEST_SCRATCH);
    testRunPassesArgumentsAndStopsLeftovers();
    testMissingGamescope();
    testSignalStopsTheSession();
    std::printf("nested_session: all checks passed\n");
    return 0;
}
