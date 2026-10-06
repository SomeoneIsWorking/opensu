// The launch handoff, exercised against throwaway processes.
//
// What is pinned here, all of it observable from outside the shell: start()
// blocks while the game runs and shows the shell again only once the game is
// gone; it keeps waiting across a Steam/Legendary style hand-off, where the
// program it spawned exits at once and a different process runs the game; it
// ends when the instance does or when forceClose() is called. A Steam launch waits
// for the client to be ready, asks it to run the game and watches the game's
// process; it fails while Steam is blocked or failed, and forceClose() ends the
// game's tree but not the client.
//
// start() blocks for as long as the game runs, so every launch here runs on its
// own thread and every wait is bounded: a regression in the wait reports itself
// instead of hanging the test. No display or Gamescope is needed.
#include "launch/handoff.hpp"
#include "launch/process_tree.hpp"
#include "launch/steam_gate.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <csignal>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using iideck::launch::Handoff;
using iideck::launch::ProcessTree;
using iideck::launch::SteamGate;
using iideck::launch::SteamState;
using iideck::library::Game;
using iideck::library::Source;

// Where the fixtures' programs live; tests do not read the environment.
const std::vector<fs::path> kSearchPath{"/usr/bin", "/bin"};

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void write(const fs::path& path, const std::string& body) {
    fs::create_directories(path.parent_path());
    std::ofstream out{path, std::ios::binary};
    out << body;
    expect(static_cast<bool>(out), "a fixture file could not be written");
}

std::string read(const fs::path& path) {
    std::ifstream in{path, std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// Polls until `predicate` holds or `timeout` elapses. These bounds are
/// generous; none of them is infinite, because a broken wait has to fail rather
/// than hang.
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

/// A Steam client whose state the test sets.
class FakeSteam final : public SteamGate {
  public:
    explicit FakeSteam(SteamState initial = SteamState::Ready) : state_{initial} {
    }

    void set(SteamState state) {
        state_.store(state);
    }

    SteamState state() const override {
        return state_.load();
    }

    SteamState waitReady(std::chrono::milliseconds timeout,
                         const std::function<bool()>& cancelled) override {
        const Clock::time_point until = Clock::now() + timeout;
        while (state_.load() == SteamState::Initializing && !cancelled() && Clock::now() < until) {
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }
        return state_.load();
    }

  private:
    std::atomic<SteamState> state_;
};

/// One temp directory per run, holding the scripts, the pid files and the
/// markers those scripts carry. The pid keeps two concurrent runs of this test
/// from sharing a directory.
struct Fixture {
    fs::path base;
    fs::path home;

    Fixture() {
        base = fs::path{IIDECK_TEST_SCRATCH} / ("iideck-handoff-test-" + std::to_string(getpid()));
        fs::remove_all(base);
        home = base / "home";
        fs::create_directories(home);
    }

    ~Fixture() {
        fs::remove_all(base);
    }

    /// Writes a script and returns its path. Scripts are run as
    /// `/bin/sh <path>`, so they need no mode bits.
    fs::path script(const std::string& name, const std::string& body) const {
        const fs::path path = base / name;
        write(path, body);
        return path;
    }

    /// Names this run's scopes, so two runs never share one.
    std::string session() const {
        return "iideck-test-" + std::to_string(getpid());
    }

    /// A hint no process holds by accident: this run's own pid keeps a second
    /// test binary from matching the first one's marker.
    std::string marker(const std::string& stem) const {
        return "iideck-handoff-" + stem + "-" + std::to_string(getpid());
    }
};

/// The fields of /proc/<pid>/stat this test needs. `comm` (field 2) can hold
/// spaces and parentheses of its own, so parsing starts after its closing
/// parenthesis: what follows is state, ppid, pgrp, session, ...
struct Stat {
    char state{'?'};
    long ppid{0};
    long pgrp{0};
    long session{0};
};

bool readStat(pid_t pid, Stat& out) {
    const std::string text = read("/proc/" + std::to_string(pid) + "/stat");
    const std::size_t close = text.rfind(')');
    if (close == std::string::npos) {
        return false;
    }
    std::istringstream fields{text.substr(close + 1)};
    std::string state;
    if (!(fields >> state >> out.ppid >> out.pgrp >> out.session)) {
        return false;
    }
    out.state = state.empty() ? '?' : state.front();
    return true;
}

/// True while `pid` is still executing. A reaped process and a zombie both count
/// as exited: what the hand-off has to get right is that the process it spawned
/// stopped running while the game carried on.
bool running(pid_t pid) {
    Stat stat;
    return readStat(pid, stat) && stat.state != 'Z' && stat.state != 'X';
}

/// Waits for `pid` to stop running, and says whether it did so inside `window`.
bool stoppedWithin(pid_t pid, std::chrono::milliseconds window) {
    return waitUntil(
        [pid] {
            return !running(pid);
        },
        window);
}

/// The pid a launched program recorded for itself, or 0 while it has not written
/// a complete one yet.
pid_t recordedPid(const fs::path& pidFile) {
    const std::string text = read(pidFile);
    if (text.empty()) {
        return 0;
    }
    try {
        return static_cast<pid_t>(std::stol(text));
    } catch (const std::exception&) {
        return 0;
    }
}

/// Waits for a launched program to write down its own pid, failing with `what`
/// when it never does.
pid_t awaitRecordedPid(const fs::path& pidFile, const char* what) {
    expect(waitUntil(
               [&pidFile] {
                   return recordedPid(pidFile) > 0;
               },
               std::chrono::seconds{20}),
           what);
    return recordedPid(pidFile);
}

/// hide and show are called from the thread running start(), so the order they
/// happened in is recorded in a counter instead of being assumed.
struct Shell {
    std::atomic<int> hidden{0};
    std::atomic<int> shown{0};
    std::atomic<int> calls{0};
    std::atomic<int> hiddenAt{0};
    std::atomic<int> shownAt{0};
    std::atomic<bool> returned{false};

    void hide() {
        hiddenAt.store(calls.fetch_add(1));
        hidden.fetch_add(1);
    }

    void show() {
        shownAt.store(calls.fetch_add(1));
        shown.fetch_add(1);
    }

    /// True when start() returned inside `window`. The tests use the inverse: to
    /// prove it is still waiting.
    bool returnedWithin(std::chrono::milliseconds window) const {
        return waitUntil(
            [this] {
                return returned.load();
            },
            window);
    }
};

Game makeGame(const std::string& id, const std::string& program, std::vector<std::string> args,
              std::string hint) {
    Game game;
    game.id = id;
    game.source = Source::Epic;
    game.title = id;
    game.installed = true;
    game.processHint = std::move(hint);
    game.launch.program = std::move(program);
    game.launch.args = std::move(args);
    return game;
}

struct Outcome {
    bool ok{false};
    std::string failure;
};

/// start() is documented to block, so every launch goes on its own thread.
std::future<Outcome> startOnWorker(Handoff& handoff, const Game& game, Shell& shell) {
    return std::async(std::launch::async, [&handoff, &game, &shell]() {
        Outcome outcome;
        outcome.ok = handoff.start(
            game,
            [&shell] {
                shell.hide();
            },
            [&shell] {
                shell.show();
            },
            outcome.failure);
        shell.returned.store(true);
        return outcome;
    });
}

/// Waits for start() to return, reporting instead of blocking forever.
Outcome awaitStart(std::future<Outcome>& pending, const char* what,
                   std::chrono::seconds bound = std::chrono::seconds{60}) {
    if (pending.wait_for(bound) != std::future_status::ready) {
        std::fprintf(stderr, "FAIL: %s: start() did not return within %llds\n", what,
                     (long long)bound.count());
        std::exit(1);
    }
    return pending.get();
}

std::chrono::milliseconds since(Clock::time_point began) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - began);
}

/// A game process that records its own pid and then runs for `seconds`. Its
/// script is named after the marker, so the running process's command line
/// carries the hint -- which is what a real source's hint is, a substring of the
/// game's own command line.
struct RunningGame {
    fs::path pidFile;
    Game game;
};

RunningGame runningGame(const Fixture& fixture, const std::string& stem, int seconds) {
    const std::string marker = fixture.marker(stem);
    const fs::path pidFile = fixture.base / (stem + ".pid");
    const std::string body = "#!/bin/sh\n"
                             "echo $$ > \"" +
                             pidFile.string() +
                             "\"\n"
                             "sleep " +
                             std::to_string(seconds) + "\n";
    const fs::path script = fixture.script(marker + ".sh", body);
    return RunningGame{pidFile, makeGame(stem, "/bin/sh", {script.string()}, marker)};
}

/// start() hides the shell, stays inside its wait for as long as the game runs,
/// and shows the shell again once the game is over.
void testStartBlocksUntilTheGameIsOver() {
    const Fixture fixture;
    const RunningGame child = runningGame(fixture, "blocking", 4);
    Shell shell;
    FakeSteam steam;
    Handoff handoff{kSearchPath, fixture.session(), steam};

    const Clock::time_point began = Clock::now();
    std::future<Outcome> pending = startOnWorker(handoff, child.game, shell);

    const pid_t pid =
        awaitRecordedPid(child.pidFile, "the launched program was started and recorded its pid");
    expect(running(pid), "the launched program is running");

    // Two seconds of a four second game: the wait must not end inside that
    // window, or the shell would come back while the game is still running.
    expect(!shell.returnedWithin(std::chrono::seconds{2}),
           "start() returned while the game was still running");
    expect(running(pid), "the game was still running for that whole window");
    expect(shell.hidden.load() == 1, "the shell was hidden once while the game runs");
    expect(shell.shown.load() == 0, "the shell is not shown again while the game runs");

    const Outcome outcome = awaitStart(pending, "start() returns when the game exits");
    const std::chrono::milliseconds elapsed = since(began);
    expect(outcome.ok, "a game that ran to completion reports success");
    expect(outcome.failure.empty(), "a game that ran to completion reports no failure");
    expect(elapsed >= std::chrono::seconds{4}, "start() stayed blocked for the whole game");
    expect(elapsed < std::chrono::seconds{30}, "start() returned promptly after the game ended");
    expect(!running(pid), "the game process is gone by the time start() returns");
    expect(shell.shown.load() == 1, "the shell was shown again, once");
    expect(shell.hiddenAt.load() < shell.shownAt.load(), "hide precedes show");
}

/// The Steam/Legendary shape: the program that was spawned exits at once, and a
/// different process starts the game a few seconds later. The child exiting says
/// nothing about the game, so start() has to keep waiting after the child is
/// gone -- both until the game appears and until it leaves.
void testStartWaitsAcrossAHandOff() {
    const Fixture fixture;
    const std::string marker = fixture.marker("handoff");
    const int gameDelaySeconds = 3;
    const int gameSeconds = 5;

    const fs::path launcherPid = fixture.base / "launcher.pid";
    const fs::path grandchildPid = fixture.base / "grandchild.pid";

    // The marker rides in the grandchild's script path, never in the launched
    // program's own command line, so a match can only ever come from the
    // process that is actually running the game.
    const fs::path game = fixture.script("grandchild-" + marker + ".sh",
                                         // `wait` keeps this shell in place: a shell that execs
                                         // its last command would replace the very command line
                                         // the marker is found in.
                                         "#!/bin/sh\n"
                                         "echo $$ > \"" +
                                             grandchildPid.string() +
                                             "\"\n"
                                             "sleep \"$1\" &\n"
                                             "wait\n");
    const fs::path launcher =
        fixture.script("launcher.sh", "#!/bin/sh\n"
                                      "echo $$ > \"" +
                                          launcherPid.string() +
                                          "\"\n"
                                          "( sleep " +
                                          std::to_string(gameDelaySeconds) + "; setsid /bin/sh \"" +
                                          game.string() + "\" " + std::to_string(gameSeconds) +
                                          " ) &\n"
                                          "exit 0\n");

    const Game entry = makeGame("handoff", "/bin/sh", {launcher.string()}, marker);
    expect(!ProcessTree::anyMatches(marker), "the marker matches nothing before the launch");

    Shell shell;
    FakeSteam steam;
    Handoff handoff{kSearchPath, fixture.session(), steam};
    const Clock::time_point began = Clock::now();
    std::future<Outcome> pending = startOnWorker(handoff, entry, shell);

    const pid_t launcherChild =
        awaitRecordedPid(launcherPid, "the launched program was started and recorded its pid");
    expect(stoppedWithin(launcherChild, std::chrono::seconds{10}),
           "the launched program exited while the game had not even started");

    // The launched program is gone and nothing matches the hint: start() must
    // still be waiting, and the shell must still be hidden.
    expect(!shell.returnedWithin(std::chrono::milliseconds{1500}),
           "start() returned although nothing was running yet");
    expect(shell.shown.load() == 0, "the shell is still hidden after the launched program exited");
    expect(shell.hidden.load() == 1, "the shell was hidden once, before the wait ended");

    const pid_t gamePid = awaitRecordedPid(grandchildPid, "the game the launcher handed off to "
                                                          "started");
    expect(waitUntil(
               [gamePid] {
                   return running(gamePid);
               },
               std::chrono::milliseconds{500}),
           "the hand-off game is still running half a second after it appeared");

    Stat inTable;
    expect(readStat(gamePid, inTable), "the hand-off game has a readable /proc entry");
    expect(inTable.ppid != getpid(), "the running game is not this test's own child");
    expect(ProcessTree::anyMatches(marker), "the running game's command line holds the hint");
    expect(!shell.returnedWithin(std::chrono::milliseconds{500}),
           "start() returned while the hand-off game was running");
    expect(shell.shown.load() == 0, "the shell is still hidden while the game runs");

    const Outcome outcome = awaitStart(pending, "start() returns after the hand-off game exits");
    const std::chrono::milliseconds elapsed = since(began);

    expect(outcome.ok, "a hand-off that ran to completion reports success");
    expect(outcome.failure.empty(), "a hand-off that ran to completion reports no failure");
    // The launched program exited in milliseconds; only a wait that covers the
    // process table lasts as long as the game did.
    expect(elapsed >=
               std::chrono::seconds{gameDelaySeconds + gameSeconds} - std::chrono::seconds{1},
           "start() kept waiting across the whole hand-off");
    expect(elapsed < std::chrono::seconds{60},
           "start() returned instead of waiting out its twelve hour bound");
    expect(!ProcessTree::anyMatches(marker),
           "no process holds the hint by the time start() returns");
    expect(!running(gamePid), "the hand-off game is gone by the time start() returns");
    expect(shell.shown.load() == 1, "the shell was shown again, once");
    expect(shell.hiddenAt.load() < shell.shownAt.load(), "hide precedes show");
}

/// A command that cannot be started is refused at once, naming the program,
/// before the shell is hidden.
void testMissingProgramReportsFailurePromptly() {
    const Fixture fixture;
    const std::string program = (fixture.base / "no-such-program").string();
    const Game entry = makeGame("missing", program, {"--now"}, fixture.marker("missing"));

    Shell shell;
    FakeSteam steam;
    Handoff handoff{kSearchPath, fixture.session(), steam};
    const Clock::time_point began = Clock::now();
    std::future<Outcome> pending = startOnWorker(handoff, entry, shell);
    const Outcome outcome =
        awaitStart(pending, "a launch of a missing program returns", std::chrono::seconds{20});

    expect(!outcome.ok, "a program that does not exist fails");
    expect(outcome.failure.find(program) != std::string::npos,
           "the failure names the program that could not be started");
    expect(since(began) < std::chrono::seconds{5}, "the failure is reported at once");
    expect(shell.hidden.load() == 0, "the shell was never hidden for a refused launch");
}

/// forceClose() during the run phase kills the instance, so start() returns and
/// shows the shell although the game had hours left.
void testForceCloseEndsTheRunPhase() {
    const Fixture fixture;
    const RunningGame child = runningGame(fixture, "forced", 600);
    Shell shell;
    FakeSteam steam;
    Handoff handoff{kSearchPath, fixture.session(), steam};
    std::future<Outcome> pending = startOnWorker(handoff, child.game, shell);

    const pid_t pid = awaitRecordedPid(child.pidFile, "the game started");
    expect(waitUntil(
               [&child] {
                   return ProcessTree::anyMatches(child.game.processHint);
               },
               std::chrono::seconds{10}),
           "the game is in the process table");
    expect(!shell.returnedWithin(std::chrono::milliseconds{1500}), "start() waits while it runs");

    const Clock::time_point began = Clock::now();
    handoff.forceClose();
    const Outcome outcome =
        awaitStart(pending, "start() returns after forceClose()", std::chrono::seconds{10});
    expect(outcome.ok && outcome.failure.empty(), "a forced close is not a failure");
    expect(since(began) < std::chrono::seconds{8}, "start() returned promptly");
    expect(shell.shown.load() == 1, "show() fired once");
    expect(stoppedWithin(pid, std::chrono::seconds{5}), "the game is gone");
}

/// forceClose() before the game ever appears also ends the wait.
void testForceCloseEndsTheStartPhase() {
    const Fixture fixture;
    const Game entry = makeGame("never", "/bin/sleep", {"600"}, fixture.marker("never"));
    Shell shell;
    FakeSteam steam;
    Handoff handoff{kSearchPath, fixture.session(), steam};
    std::future<Outcome> pending = startOnWorker(handoff, entry, shell);

    expect(waitUntil(
               [&shell] {
                   return shell.hidden.load() == 1;
               },
               std::chrono::seconds{10}),
           "the shell was hidden once the instance started");
    handoff.forceClose();
    const Outcome outcome =
        awaitStart(pending, "start() returns after forceClose()", std::chrono::seconds{10});
    expect(outcome.ok, "a forced close is not a failure");
    expect(shell.shown.load() == 1, "show() fired once");
}

/// The instance ending without the game ever appearing ends the wait with a
/// failure instead of sitting out the appearance bound.
void testInstanceExitEndsTheWait() {
    const Fixture fixture;
    const Game entry = makeGame("quick", "/bin/sh", {"-c", "exit 3"}, fixture.marker("quick"));
    Shell shell;
    FakeSteam steam;
    Handoff handoff{kSearchPath, fixture.session(), steam};
    std::future<Outcome> pending = startOnWorker(handoff, entry, shell);
    const Outcome outcome =
        awaitStart(pending, "start() returns when the instance exits", std::chrono::seconds{20});
    expect(!outcome.ok, "a game that never appeared fails");
    expect(outcome.failure.find("did not start") != std::string::npos, "the failure says so");
    expect(shell.shown.load() == 1, "show() fired once");
}

/// A `steam` that answers `-applaunch <id>` the way a running client does: it
/// starts the game under a shell whose last argument is `AppId=<id>`, outside the
/// caller's process group, and exits at once. Returns the directory it lives in.
struct FakeSteamProgram {
    fs::path bin;
    fs::path launchLog;
    fs::path gamePid;
    std::string appId;
    Game game;

    FakeSteamProgram(const Fixture& fixture, int gameSeconds) {
        appId = std::to_string(getpid());
        bin = fixture.base / "bin";
        launchLog = fixture.base / "steam-launches.log";
        gamePid = fixture.base / "steam-game.pid";
        const fs::path program = bin / "steam";
        write(program, "#!/bin/sh\n"
                       "[ \"$1\" = \"-applaunch\" ] || exit 2\n"
                       "echo \"$2\" >> \"" +
                           launchLog.string() +
                           "\"\n"
                           "setsid sh -c 'echo $$ > \"" +
                           gamePid.string() + "\"; sleep " + std::to_string(gameSeconds) +
                           "; true' \"AppId=$2\" > /dev/null 2>&1 &\n"
                           "exit 0\n");
        fs::permissions(program, fs::perms::owner_all);
        game = makeGame("steam:" + appId, "steam", {"-applaunch", appId},
                        "AppId=" + appId + std::string(1, '\0'));
        game.source = Source::Steam;
    }
};

/// A Steam launch holds until the client is ready, then runs through it: the shell
/// is hidden only once the game is on its way, and the client is left alone.
void testSteamWaitsForReadiness() {
    const Fixture fixture;
    const FakeSteamProgram steamProgram{fixture, 3};
    FakeSteam steam{SteamState::Initializing};
    Handoff handoff{{steamProgram.bin}, fixture.session(), steam};
    Shell shell;
    std::future<Outcome> pending = startOnWorker(handoff, steamProgram.game, shell);

    expect(!shell.returnedWithin(std::chrono::milliseconds{1500}),
           "a Steam launch waits while the client initializes");
    expect(shell.hidden.load() == 0, "the shell stays up while Steam initializes");
    expect(!fs::exists(steamProgram.launchLog), "nothing is launched before Steam is ready");

    steam.set(SteamState::Ready);
    const pid_t game =
        awaitRecordedPid(steamProgram.gamePid, "the game started once Steam was ready");
    expect(running(game), "the game is running");
    expect(read(steamProgram.launchLog) == steamProgram.appId + "\n",
           "steam -applaunch was run once, with the app id");
    expect(waitUntil(
               [&shell] {
                   return shell.hidden.load() == 1;
               },
               std::chrono::seconds{10}),
           "the shell was hidden once the game was on its way");

    const Outcome outcome = awaitStart(pending, "start() returns when the game exits");
    expect(outcome.ok && outcome.failure.empty(), "a Steam game that ran reports success");
    expect(!running(game), "the game is gone");
    expect(shell.shown.load() == 1, "the shell was shown again, once");
    expect(steam.state() == SteamState::Ready, "the client is left ready");
}

/// Steam being blocked or failed refuses the launch by name, before anything is hidden.
void testSteamBlockedAndFailedAreRefused() {
    const Fixture fixture;
    const FakeSteamProgram steamProgram{fixture, 3};

    {
        FakeSteam steam{SteamState::Blocked};
        Handoff handoff{{steamProgram.bin}, fixture.session(), steam};
        Shell shell;
        std::future<Outcome> pending = startOnWorker(handoff, steamProgram.game, shell);
        const Outcome outcome =
            awaitStart(pending, "a blocked Steam refuses the launch", std::chrono::seconds{10});
        expect(!outcome.ok, "a launch beside a desktop Steam fails");
        expect(outcome.failure ==
                   "Steam is running on the desktop; quit it to use it inside iideck",
               "the failure is the named one");
        expect(shell.hidden.load() == 0, "the shell was never hidden");
    }
    {
        FakeSteam steam{SteamState::Failed};
        Handoff handoff{{steamProgram.bin}, fixture.session(), steam};
        Shell shell;
        std::future<Outcome> pending = startOnWorker(handoff, steamProgram.game, shell);
        const Outcome outcome =
            awaitStart(pending, "a failed Steam refuses the launch", std::chrono::seconds{10});
        expect(!outcome.ok && outcome.failure == "Steam failed to start",
               "a failed Steam is named");
        expect(shell.hidden.load() == 0, "the shell was never hidden");
    }
    expect(!fs::exists(steamProgram.launchLog), "neither refusal launched anything");
}

/// forceClose() ends the game's whole tree and returns the shell, and the Steam
/// client keeps running.
void testSteamForceCloseKillsTheGameNotTheClient() {
    const Fixture fixture;
    const FakeSteamProgram steamProgram{fixture, 600};
    FakeSteam steam;
    Handoff handoff{{steamProgram.bin}, fixture.session(), steam};

    // Stands in for the client process: unrelated to the game's command line.
    const pid_t client = fork();
    expect(client >= 0, "fork");
    if (client == 0) {
        execl("/bin/sleep", "sleep", "600", static_cast<char*>(nullptr));
        std::_Exit(127);
    }

    Shell shell;
    std::future<Outcome> pending = startOnWorker(handoff, steamProgram.game, shell);
    const pid_t game = awaitRecordedPid(steamProgram.gamePid, "the game started");
    expect(waitUntil(
               [&steamProgram] {
                   return ProcessTree::anyMatches(steamProgram.game.processHint);
               },
               std::chrono::seconds{10}),
           "the game is in the process table");
    expect(waitUntil(
               [game] {
                   return !ProcessTree::descendants(game).empty();
               },
               std::chrono::seconds{10}),
           "the game has a child process");
    const std::vector<pid_t> tree = ProcessTree::descendants(game);
    expect(!shell.returnedWithin(std::chrono::milliseconds{1500}), "start() waits while it runs");

    const Clock::time_point began = Clock::now();
    handoff.forceClose();
    const Outcome outcome =
        awaitStart(pending, "start() returns after forceClose()", std::chrono::seconds{10});
    expect(outcome.ok && outcome.failure.empty(), "a forced close is not a failure");
    expect(since(began) < std::chrono::seconds{8}, "start() returned promptly");
    expect(shell.shown.load() == 1, "show() fired once");
    expect(stoppedWithin(game, std::chrono::seconds{5}), "the game is gone");
    for (const pid_t child : tree) {
        expect(stoppedWithin(child, std::chrono::seconds{5}), "the game's children are gone");
    }
    expect(running(client), "the client's process was left alone");
    expect(steam.state() == SteamState::Ready, "the client is still ready");

    kill(client, SIGKILL);
    waitpid(client, nullptr, 0);
}

/// forceClose() while the launch waits for Steam ends the wait without starting anything.
void testSteamForceCloseCancelsTheWait() {
    const Fixture fixture;
    const FakeSteamProgram steamProgram{fixture, 3};
    FakeSteam steam{SteamState::Initializing};
    Handoff handoff{{steamProgram.bin}, fixture.session(), steam};
    Shell shell;
    std::future<Outcome> pending = startOnWorker(handoff, steamProgram.game, shell);

    expect(!shell.returnedWithin(std::chrono::milliseconds{500}), "start() waits for Steam");
    handoff.forceClose();
    const Outcome outcome =
        awaitStart(pending, "start() returns after forceClose()", std::chrono::seconds{10});
    expect(outcome.ok && outcome.failure.empty(), "a cancelled wait is not a failure");
    expect(shell.hidden.load() == 0 && shell.shown.load() == 0, "the shell was never touched");
    expect(!fs::exists(steamProgram.launchLog), "nothing was launched");
}
/// A source that cannot identify its own process has no hint, and there is
/// nothing to watch. The handoff has to say so rather than sit out the appearance
/// bound and then report a game that is running fine as having failed to start.
void testEmptyHintIsRefusedPromptly() {
    const Fixture fixture;
    Game entry = makeGame("nohint", "/bin/sh", {"/bin/true"}, fixture.marker("nohint"));
    entry.processHint.clear();

    Shell shell;
    FakeSteam steam;
    Handoff handoff{kSearchPath, fixture.session(), steam};
    const Clock::time_point began = Clock::now();
    std::future<Outcome> pending = startOnWorker(handoff, entry, shell);
    const Outcome outcome =
        awaitStart(pending, "a launch with no hint is refused", std::chrono::seconds{20});
    const std::chrono::milliseconds elapsed = since(began);

    expect(!outcome.ok, "a launch with no hint fails");
    expect(outcome.failure.find("cannot tell") != std::string::npos,
           "the failure says the source cannot identify its process");
    expect(elapsed < std::chrono::seconds{5}, "the refusal is immediate");
    expect(shell.shown.load() == 0, "the shell was never hidden for a refused launch");
}

/// An empty hint must not degenerate into "every process": every launch would then
/// find a match and the shell would stay hidden for the whole watch timeout.
void testEmptyHintMatchesNothing() {
    const std::string own = read("/proc/self/cmdline");
    expect(!own.empty(), "this process has a command line");
    expect(ProcessTree::anyMatches(own), "a hint naming this process matches it");
    expect(!ProcessTree::anyMatches(""), "an empty hint matches no process at all");
}

} // namespace

/// A hint ending in NUL matches a whole command-line argument, never a prefix of one.
void testTerminatedHintMatchesAWholeArgument() {
    const pid_t pid = fork();
    expect(pid >= 0, "fork works");
    if (pid == 0) {
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", "sleep 30; true", "AppId=987650", static_cast<char*>(nullptr));
        std::_Exit(127);
    }
    const std::string nul(1, '\0');
    expect(waitUntil(
               [&nul] {
                   return ProcessTree::anyMatches("AppId=987650" + nul);
               },
               std::chrono::seconds{5}),
           "the whole argument matches");
    expect(!ProcessTree::anyMatches("AppId=98765" + nul), "a prefix of the argument does not");
    kill(-pid, SIGKILL);
    waitpid(pid, nullptr, 0);
}

int main() {
    testTerminatedHintMatchesAWholeArgument();
    testStartBlocksUntilTheGameIsOver();
    testStartWaitsAcrossAHandOff();
    testEmptyHintMatchesNothing();
    testMissingProgramReportsFailurePromptly();
    testForceCloseEndsTheRunPhase();
    testForceCloseEndsTheStartPhase();
    testInstanceExitEndsTheWait();
    testSteamWaitsForReadiness();
    testSteamBlockedAndFailedAreRefused();
    testSteamForceCloseKillsTheGameNotTheClient();
    testSteamForceCloseCancelsTheWait();
    testEmptyHintIsRefusedPromptly();

    std::printf("handoff: all checks passed\n");
    return 0;
}
