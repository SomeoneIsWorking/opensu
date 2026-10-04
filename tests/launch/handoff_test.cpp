// The launch handoff, exercised against throwaway processes.
//
// What is pinned here, all of it observable from outside the shell: a launched
// game leads its own session, so neither the shell's exit nor a hangup can
// reach it; start() blocks while the game runs and shows the shell again only
// once the game is gone; and it keeps waiting across a Steam/Legendary style
// hand-off, where the program it spawned exits at once and a different process
// runs the game.
//
// start() blocks for as long as the game runs, so every launch here runs on its
// own thread and every wait is bounded: a regression in the wait reports itself
// instead of hanging the test.
//
// A real fork and a real process table are the only things that can catch the
// bugs this covers -- a use-after-free in the argument vector, or a wait that
// ends on the wrong signal -- so it is slower than the parser tests, though every
// case is bounded and none waits out a real timeout.
#include "launch/handoff.hpp"

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

#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using iideck::launch::Handoff;
using iideck::library::Game;
using iideck::library::Source;

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

/// One temp directory per run, holding the scripts, the pid files and the
/// markers those scripts carry. The pid keeps two concurrent runs of this test
/// from sharing a directory.
struct Fixture {
    fs::path base;

    Fixture() {
        base = fs::temp_directory_path() / ("iideck-handoff-test-" + std::to_string(getpid()));
        fs::remove_all(base);
        fs::create_directories(base);
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
    game.source = Source::Steam;
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
std::future<Outcome> startOnWorker(const Game& game, Shell& shell) {
    return std::async(std::launch::async, [&game, &shell]() {
        Outcome outcome;
        outcome.ok = Handoff::start(
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

    const Clock::time_point began = Clock::now();
    std::future<Outcome> pending = startOnWorker(child.game, shell);

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

/// The spawned game leads its own session, so a shell exit or a hangup on the
/// shell's terminal cannot reach a running game. Observable from /proc: field 6
/// of /proc/<pid>/stat is the session id, which setsid() makes equal to the
/// process's own pid.
void testSpawnedGameLeadsItsOwnSession() {
    const Fixture fixture;
    const RunningGame child = runningGame(fixture, "session", 5);
    Shell shell;

    Stat self;
    expect(readStat(getpid(), self), "this process has a readable /proc entry");

    std::future<Outcome> pending = startOnWorker(child.game, shell);
    const pid_t pid =
        awaitRecordedPid(child.pidFile, "the launched program was started and recorded its pid");

    Stat game;
    expect(waitUntil(
               [&game, pid] {
                   return readStat(pid, game);
               },
               std::chrono::seconds{10}),
           "the launched program has a readable /proc entry");
    expect(game.state != 'Z' && game.state != 'X', "the launched program is still running");
    expect(game.ppid == getpid(), "the launched program is a child of start()'s caller");
    expect(game.session == pid, "the launched game leads its own session");
    expect(game.session != self.session, "the launched game's session is not the shell's");
    expect(game.pgrp == pid, "the launched game leads its own process group");
    expect(game.pgrp != self.pgrp, "the launched game's process group is not the shell's");

    const Outcome outcome = awaitStart(pending, "start() returns when the game exits");
    expect(outcome.ok, "a game in its own session reports success");
    expect(outcome.failure.empty(), "a game in its own session reports no failure");
    expect(!running(pid), "the game process is gone by the time start() returns");
    expect(shell.shown.load() == 1, "the shell was shown again, once");
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
    expect(!Handoff::processMatches(marker), "the marker matches nothing before the launch");

    Shell shell;
    const Clock::time_point began = Clock::now();
    std::future<Outcome> pending = startOnWorker(entry, shell);

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
    expect(Handoff::processMatches(marker), "the running game's command line holds the hint");
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
    expect(!Handoff::processMatches(marker),
           "no process holds the hint by the time start() returns");
    expect(!running(gamePid), "the hand-off game is gone by the time start() returns");
    expect(shell.shown.load() == 1, "the shell was shown again, once");
    expect(shell.hiddenAt.load() < shell.shownAt.load(), "hide precedes show");
}

/// A command that cannot be started is a launch failure, reported at once and
/// naming the program.
///
/// execvp returns 127 for a command it could not run, and that status is the only
/// thing that distinguishes a bad program from a game that has not appeared yet.
/// Without it a missing program waited out the whole appearance bound and was
/// reported as "<title> did not start", which named neither the program nor the
/// reason.
void testMissingProgramReportsFailurePromptly() {
    const Fixture fixture;
    const std::string program = (fixture.base / "no-such-program").string();
    const Game entry = makeGame("missing", program, {"--now"}, fixture.marker("missing"));

    Shell shell;
    const Clock::time_point began = Clock::now();
    std::future<Outcome> pending = startOnWorker(entry, shell);
    const Outcome outcome =
        awaitStart(pending, "a launch of a missing program returns", std::chrono::seconds{20});
    const std::chrono::milliseconds elapsed = since(began);

    expect(!outcome.ok, "a program that does not exist fails");
    expect(outcome.failure.find(program) != std::string::npos,
           "the failure names the program that could not be started");
    expect(elapsed < std::chrono::seconds{10},
           "the failure is reported without waiting out the appearance bound");
    expect(shell.hidden.load() == 1, "the shell was hidden while the launch was attempted");
    expect(shell.shown.load() == 1, "the shell was shown again after a failed launch");
    expect(shell.hiddenAt.load() < shell.shownAt.load(), "hide precedes show");
}

/// A source that cannot identify its own process has no hint, and there is
/// nothing to watch. The handoff has to say so rather than sit out the appearance
/// bound and then report a game that is running fine as having failed to start.
void testEmptyHintIsRefusedPromptly() {
    const Fixture fixture;
    Game entry = makeGame("nohint", "/bin/sh", {"/bin/true"}, fixture.marker("nohint"));
    entry.processHint.clear();

    Shell shell;
    const Clock::time_point began = Clock::now();
    std::future<Outcome> pending = startOnWorker(entry, shell);
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
    expect(Handoff::processMatches(own), "a hint naming this process matches it");
    expect(!Handoff::processMatches(""), "an empty hint matches no process at all");
}

} // namespace

int main() {
    testStartBlocksUntilTheGameIsOver();
    testSpawnedGameLeadsItsOwnSession();
    testStartWaitsAcrossAHandOff();
    testEmptyHintMatchesNothing();
    testMissingProgramReportsFailurePromptly();
    testEmptyHintIsRefusedPromptly();

    std::printf("handoff: all checks passed\n");
    return 0;
}
