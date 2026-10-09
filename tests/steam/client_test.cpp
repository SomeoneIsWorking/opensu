// The Steam client opensu owns, against a fake home and a fake `steam` program run
// in real systemd scopes. Every wait is bounded.
#include "steam/client.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

#include "launch/command.hpp"

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using opensu::launch::SteamState;
using opensu::steam::Client;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

constexpr const char* logonLine = "[2026-10-06 19:56:14] [Logged On, 4, 7] [U:1:71770072] "
                                  "RecvMsgClientLogOnResponse() : processing complete";

enum class Mode : std::uint8_t {
    /// Logs on after a second, writing the line in two pieces, then runs until -shutdown.
    Ready,
    /// Never logs on; runs until -shutdown.
    Slow,
    /// Exits at once.
    Exits,
    /// Logs on, and ignores -shutdown.
    Stubborn,
};

/// Counts the fixtures of this run, so each gets a scope name of its own.
int fixtures = 0;

struct Fixture {
    fs::path base;
    fs::path home;
    fs::path bin;
    fs::path pidFile;
    /// The bus address the fake steam was started with.
    fs::path busFile;
    fs::path log;
    std::string session;

    explicit Fixture(Mode mode) {
        const std::string name = std::to_string(getpid()) + "-" + std::to_string(fixtures++);
        base = fs::path{OPENSU_TEST_SCRATCH} / ("opensu-steam-client-test-" + name);
        fs::remove_all(base);
        home = base / "home";
        bin = base / "bin";
        pidFile = base / "steam.pid";
        busFile = base / "steam.bus";
        log = home / ".steam" / "steam" / "logs" / "connection_log.txt";
        session = "opensu-test-" + name;
        fs::create_directories(log.parent_path());
        fs::create_directories(bin);

        // A logon from before this client started, which must not count.
        std::ofstream{log} << logonLine << "\n";

        std::string body = "#!/bin/sh\n";
        if (mode != Mode::Stubborn) {
            body += "if [ \"$1\" = \"-shutdown\" ]; then kill $(cat \"" + pidFile.string() +
                    "\"); exit 0; fi\n";
        } else {
            body += "if [ \"$1\" = \"-shutdown\" ]; then exit 0; fi\n";
        }
        if (mode == Mode::Exits) {
            body += "exit 1\n";
        } else {
            body += "echo $$ > \"" + pidFile.string() + "\"\n";
            body += "echo \"$DBUS_SESSION_BUS_ADDRESS\" > \"" + busFile.string() + "\"\n";
            if (mode != Mode::Slow) {
                body += "sleep 1\n"
                        "printf '%s' '[2026-10-06 19:56:14] [Logged On, 4, 7] [U:1:7] Recv' >> \"" +
                        log.string() +
                        "\"\n"
                        "sleep 0.3\n"
                        "echo 'MsgClientLogOnResponse() : processing complete' >> \"" +
                        log.string() + "\"\n";
            }
            body += "exec sleep 600\n";
        }
        const fs::path program = bin / "steam";
        std::ofstream{program} << body;
        fs::permissions(program, fs::perms::owner_all);
        // The real one, beside the fake steam, since the client looks only in `bin`.
        const fs::path bus =
            opensu::launch::resolveExecutable("dbus-run-session", {"/usr/bin", "/bin"});
        expect(!bus.empty(), "dbus-run-session is installed");
        fs::create_symlink(bus, bin / "dbus-run-session");
    }

    ~Fixture() {
        fs::remove_all(base);
    }

    Client::Options options() const {
        return Client::Options{home, {bin}, session};
    }

    std::string unit() const {
        return session + "-steam.scope";
    }
};

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

bool alive(pid_t pid) {
    std::ifstream in{"/proc/" + std::to_string(pid) + "/stat"};
    std::string text;
    std::getline(in, text);
    const std::size_t close = text.rfind(')');
    return close != std::string::npos && close + 2 < text.size() && text[close + 2] != 'Z';
}

pid_t recordedPid(const fs::path& file) {
    std::ifstream in{file};
    long value = 0;
    in >> value;
    return static_cast<pid_t>(value);
}

const auto never = [] {
    return false;
};

/// Initializing, then Ready once a logon is logged after the start; then a graceful
/// shutdown ends the scope.
void testReadyThenShutdown() {
    const Fixture fixture{Mode::Ready};
    Client client{fixture.options()};
    expect(client.state() == SteamState::Stopped, "a new client is stopped");
    client.start();
    expect(client.state() == SteamState::Initializing, "a started client is initializing");
    expect(scopeActive(fixture.unit()), "steam runs in the session's scope");
    expect(waitUntil(
               [&fixture] {
                   std::error_code ec;
                   return fs::file_size(fixture.busFile, ec) > 1 && !ec;
               },
               std::chrono::seconds{10}),
           "steam runs on a session bus");
    {
        std::ifstream in{fixture.busFile};
        std::string address;
        std::getline(in, address);
        const char* desktop = std::getenv("DBUS_SESSION_BUS_ADDRESS");
        expect(desktop == nullptr || address != desktop,
               "steam's session bus is its own, not the desktop's");
    }

    // The log already held a logon line; only one logged after the start counts.
    std::this_thread::sleep_for(std::chrono::milliseconds{700});
    expect(client.state() == SteamState::Initializing, "an old logon line does not make it ready");

    expect(client.waitReady(std::chrono::seconds{30}, never) == SteamState::Ready,
           "the logon appended after the start makes it ready");
    expect(client.state() == SteamState::Ready, "the state reads ready");
    const pid_t steam = recordedPid(fixture.pidFile);
    expect(steam > 0 && alive(steam), "the fake steam is running");

    const Clock::time_point began = Clock::now();
    client.shutdown();
    expect(Clock::now() - began < std::chrono::seconds{15}, "a graceful shutdown is quick");
    expect(client.state() == SteamState::Stopped, "a shut down client is stopped");
    expect(!scopeActive(fixture.unit()), "the scope is gone");
    expect(waitUntil(
               [steam] {
                   return !alive(steam);
               },
               std::chrono::seconds{5}),
           "steam is gone");
}

/// A steam that exits at once is a failure.
void testExitIsAFailure() {
    const Fixture fixture{Mode::Exits};
    Client client{fixture.options()};
    client.start();
    expect(client.waitReady(std::chrono::seconds{30}, never) == SteamState::Failed,
           "a steam that exits fails");
    expect(client.state() == SteamState::Failed, "the state reads failed");
}

/// No `steam` to run is a failure too.
void testMissingSteamIsAFailure() {
    const Fixture fixture{Mode::Ready};
    Client client{Client::Options{fixture.home, {}, fixture.session}};
    client.start();
    expect(client.state() == SteamState::Failed, "no steam program fails");
}

/// A client running outside opensu blocks, and nothing is started.
void testDesktopSteamBlocks() {
    const Fixture fixture{Mode::Ready};
    const pid_t desktop = fork();
    expect(desktop >= 0, "fork");
    if (desktop == 0) {
        execl("/bin/sleep", "sleep", "60", static_cast<char*>(nullptr));
        _exit(127);
    }
    std::ofstream{fixture.home / ".steam" / "steam.pid"} << desktop;

    Client client{fixture.options()};
    client.start();
    expect(client.state() == SteamState::Blocked, "a desktop Steam blocks");
    expect(client.waitReady(std::chrono::seconds{5}, never) == SteamState::Blocked,
           "waiting on a blocked client returns at once");
    expect(!scopeActive(fixture.unit()), "no scope was started");
    expect(!fs::exists(fixture.pidFile), "the fake steam never ran");

    kill(desktop, SIGKILL);
    waitpid(desktop, nullptr, 0);
}

/// A wait ends on its timeout and on cancellation, still initializing.
void testWaitIsBounded() {
    const Fixture fixture{Mode::Slow};
    Client client{fixture.options()};
    client.start();

    Clock::time_point began = Clock::now();
    expect(client.waitReady(std::chrono::milliseconds{300}, never) == SteamState::Initializing,
           "a wait that times out reports initializing");
    expect(Clock::now() - began < std::chrono::seconds{3}, "the timeout holds");

    std::atomic<bool> cancel{false};
    std::thread canceller{[&cancel] {
        std::this_thread::sleep_for(std::chrono::milliseconds{300});
        cancel.store(true);
    }};
    began = Clock::now();
    expect(client.waitReady(std::chrono::seconds{60},
                            [&cancel] {
                                return cancel.load();
                            }) == SteamState::Initializing,
           "a cancelled wait reports initializing");
    expect(Clock::now() - began < std::chrono::seconds{5}, "cancellation is prompt");
    canceller.join();
}

/// A steam that ignores -shutdown is stopped with its scope once the wait is over.
void testStubbornSteamIsStopped() {
    const Fixture fixture{Mode::Stubborn};
    Client client{fixture.options()};
    client.start();
    expect(client.waitReady(std::chrono::seconds{30}, never) == SteamState::Ready, "it logs on");
    const pid_t steam = recordedPid(fixture.pidFile);

    const Clock::time_point began = Clock::now();
    client.shutdown();
    const auto took = Clock::now() - began;
    expect(took >= opensu::steam::shutdownWait - std::chrono::seconds{1},
           "a steam that does not exit is waited for");
    expect(took < opensu::steam::shutdownWait + std::chrono::seconds{15}, "the bound holds");
    expect(!scopeActive(fixture.unit()), "the scope is gone");
    expect(waitUntil(
               [steam] {
                   return !alive(steam);
               },
               std::chrono::seconds{5}),
           "steam is gone");
}

/// Destroying a running client shuts it down.
void testDestructorShutsDown() {
    const Fixture fixture{Mode::Slow};
    {
        Client client{fixture.options()};
        client.start();
        expect(scopeActive(fixture.unit()), "the scope is up");
    }
    expect(!scopeActive(fixture.unit()), "destroying the client ended the scope");
}

} // namespace

int main() {
    fs::create_directories(OPENSU_TEST_SCRATCH);
    testReadyThenShutdown();
    testExitIsAFailure();
    testMissingSteamIsAFailure();
    testDesktopSteamBlocks();
    testWaitIsBounded();
    testDestructorShutsDown();
    testStubbornSteamIsStopped();
    std::printf("steam client: all checks passed\n");
    return 0;
}
