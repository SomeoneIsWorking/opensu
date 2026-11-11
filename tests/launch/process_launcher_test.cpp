// The process launcher, against real processes in real systemd scopes and under keepers.
#include "launch/process_launcher.hpp"
#include "support/test_main.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using opensu::launch::Containment;
using opensu::launch::ProcessLauncher;

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

bool alive(pid_t pid) {
    std::ifstream in{"/proc/" + std::to_string(pid) + "/stat"};
    std::string text;
    std::getline(in, text);
    const std::size_t close = text.rfind(')');
    return close != std::string::npos && close + 2 < text.size() && text[close + 2] != 'Z';
}

std::string unitName(const char* stem) {
    return std::string{"opensu-test-"} + stem + "-" + std::to_string(getpid()) + ".scope";
}

pid_t readPid(const fs::path& path) {
    std::ifstream in{path};
    long value = 0;
    in >> value;
    return static_cast<pid_t>(value);
}

/// The pid a started command recorded in `pidFile`, once it is there and running.
pid_t awaitPid(const fs::path& pidFile) {
    expect(waitUntil(
               [&pidFile] {
                   return readPid(pidFile) > 0;
               },
               std::chrono::seconds{10}),
           "the escaped grandchild recorded its pid");
    const pid_t grandchild = readPid(pidFile);
    expect(alive(grandchild), "the escaped grandchild is running");
    return grandchild;
}

/// Starts a command that leaves a grandchild in its own session, and returns the
/// grandchild's pid once recorded.
pid_t startEscaper(ProcessLauncher& instance, const char* stem, const fs::path& pidFile) {
    fs::remove(pidFile);
    std::string failure;
    expect(instance.start(unitName(stem), "/bin/sh",
                          {"-c", "setsid sleep 60 & echo $! > " + pidFile.string() + "; sleep 60"},
                          failure),
           "an escaping command starts");
    return awaitPid(pidFile);
}

/// Starts a command whose grandchild double-forks into a session of its own, its parent gone,
/// and returns the grandchild's pid once recorded.
pid_t startDoubleForker(ProcessLauncher& instance, const char* stem, const fs::path& pidFile) {
    fs::remove(pidFile);
    std::string failure;
    expect(
        instance.start(unitName(stem), "/bin/sh",
                       {"-c", "(setsid sleep 60 & echo $! > " + pidFile.string() + ") ; sleep 60"},
                       failure),
        "a double-forking command starts");
    return awaitPid(pidFile);
}

bool endsWithin(pid_t pid) {
    return waitUntil(
        [pid] {
            return !alive(pid);
        },
        std::chrono::seconds{8});
}

/// The stand-in openSU: two games under keepers, their grandchildren's pids in `dir`, then it
/// waits to be killed.
int standInParent(const fs::path& dir) {
    ProcessLauncher first{Containment::keeper};
    ProcessLauncher second{Containment::keeper};
    static_cast<void>(startDoubleForker(first, "parent-first", dir / "first.pid"));
    static_cast<void>(startDoubleForker(second, "parent-second", dir / "second.pid"));
    std::ofstream{dir / "ready"} << "ready\n";
    for (;;) {
        pause();
    }
}

/// An openSU that dies without stopping its launchers still ends every game's tree: each
/// keeper sees openSU's end of its pipe close, and neither keeper holds the other's.
void checkParentDeath(const fs::path& scratch) {
    const fs::path dir = scratch / "parent";
    fs::create_directories(dir);
    const std::string self = fs::read_symlink("/proc/self/exe").string();
    const pid_t parent = fork();
    expect(parent >= 0, "the stand-in parent forks");
    if (parent == 0) {
        execl(self.c_str(), self.c_str(), "parent", dir.c_str(), nullptr);
        std::_Exit(127);
    }
    expect(waitUntil(
               [&dir] {
                   return fs::exists(dir / "ready");
               },
               std::chrono::seconds{20}),
           "the stand-in parent started both games");
    const pid_t first = readPid(dir / "first.pid");
    const pid_t second = readPid(dir / "second.pid");
    expect(alive(first) && alive(second), "both games' grandchildren run");
    ::kill(parent, SIGKILL);
    int status = 0;
    waitpid(parent, &status, 0);
    expect(endsWithin(first), "the first game's tree ends with its openSU");
    expect(endsWithin(second), "the second game's tree ends with its openSU");
}

/// Checks only a keeper has to make: a double fork stays owned, and the game's own status
/// comes back whole.
void checkKeeper(const fs::path& scratch) {
    {
        ProcessLauncher instance{Containment::keeper};
        const pid_t grandchild = startDoubleForker(instance, "double", scratch / "double.pid");
        instance.stop();
        expect(endsWithin(grandchild),
               "stop() ends a process that double-forked into its own session");
    }
    {
        // The started process exits at once; its orphan is still the launch's.
        ProcessLauncher instance{Containment::keeper};
        const fs::path pidFile = scratch / "linger.pid";
        std::string failure;
        expect(instance.start(unitName("linger"), "/bin/sh",
                              {"-c", "(setsid sleep 60 & echo $! > " + pidFile.string() + ")"},
                              failure),
               "a launcher that leaves an orphan starts");
        const pid_t orphan = awaitPid(pidFile);
        expect(waitUntil(
                   [&instance] {
                       return !instance.startedRunning();
                   },
                   std::chrono::seconds{10}),
               "the started process is seen to exit");
        expect(instance.running(), "the launch runs while its orphan does");
        instance.stop();
        expect(endsWithin(orphan), "stop() ends the orphan");
    }
    const auto endedWith = [](const std::string& script) {
        ProcessLauncher instance{Containment::keeper};
        std::string failure;
        expect(instance.start(unitName("status"), "/bin/sh", {"-c", script}, failure),
               "the status probe starts");
        expect(waitUntil(
                   [&instance] {
                       return !instance.running();
                   },
                   std::chrono::seconds{10}),
               "the status probe ends");
        return instance.exitStatus();
    };
    expect(endedWith("exit 7") == 7, "a game's exit code comes back as given");
    expect(endedWith("ulimit -c 0; kill -SEGV $$") == 128 + SIGSEGV,
           "a game's crash comes back as 128 plus its signal");
    {
        ProcessLauncher instance{Containment::keeper};
        std::string failure;
        expect(instance.start(unitName("term"), "/bin/sleep", {"60"}, failure), "sleep starts");
        instance.stop();
        expect(instance.exitStatus() == 128 + SIGTERM, "a stopped game ends on SIGTERM");
    }
    {
        // A game that ignores SIGTERM is killed after the scope's 5 s.
        ProcessLauncher instance{Containment::keeper};
        std::string failure;
        const fs::path ready = scratch / "stubborn.ready";
        expect(instance.start(
                   unitName("stubborn"), "/bin/sh",
                   {"-c", "trap '' TERM; echo > " + ready.string() + "; while :; do sleep 1; done"},
                   failure),
               "a stubborn game starts");
        expect(waitUntil(
                   [&ready] {
                       return fs::exists(ready);
                   },
                   std::chrono::seconds{10}),
               "the stubborn game ignores SIGTERM");
        const Clock::time_point began = Clock::now();
        instance.stop();
        const Clock::duration took = Clock::now() - began;
        expect(!instance.running(), "a stubborn game is gone after stop()");
        expect(took >= std::chrono::seconds{5}, "a stubborn game gets the 5 s to end");
        expect(took < std::chrono::seconds{8}, "stop() kills it in bounded time");
        expect(instance.exitStatus() == 128 + SIGKILL, "a stubborn game ends on SIGKILL");
    }
    checkParentDeath(scratch);
}

/// What every Linux containment has to do.
void checkLauncher(Containment containment, const fs::path& scratch) {
    fs::create_directories(scratch);

    {
        ProcessLauncher instance{containment};
        std::string failure;
        expect(instance.start(unitName("plain"), "/bin/sleep", {"60"}, failure), "sleep starts");
        expect(instance.running(), "a started instance is running");
        const Clock::time_point began = Clock::now();
        instance.stop();
        expect(!instance.running(), "a stopped instance is not running");
        expect(Clock::now() - began < std::chrono::seconds{8}, "stop() is bounded");
    }

    {
        ProcessLauncher instance{containment};
        const pid_t grandchild = startEscaper(instance, "stop", scratch / "stop.pid");
        instance.stop();
        expect(waitUntil(
                   [grandchild] {
                       return !alive(grandchild);
                   },
                   std::chrono::seconds{8}),
               "stop() ends a process that escaped through setsid");
    }

    {
        ProcessLauncher instance{containment};
        std::string failure;
        expect(!instance.start(unitName("missing"), "/no/such/program", {}, failure),
               "a missing program is refused");
        expect(failure.starts_with("could not start program") &&
                   failure.find("/no/such") == std::string::npos,
               "the failure names it, not its folder");
    }

    {
        pid_t grandchild = 0;
        {
            ProcessLauncher instance{containment};
            grandchild = startEscaper(instance, "dtor", scratch / "dtor.pid");
        }
        expect(waitUntil(
                   [grandchild] {
                       return !alive(grandchild);
                   },
                   std::chrono::seconds{8}),
               "destroying an instance stops it");
    }

    {
        // A caller that blocks SIGTERM, as the compositor session does, does not hand it on.
        sigset_t term;
        sigemptyset(&term);
        sigaddset(&term, SIGTERM);
        sigset_t previous;
        pthread_sigmask(SIG_BLOCK, &term, &previous);
        const fs::path mask = scratch / "mask";
        ProcessLauncher instance{containment};
        std::string failure;
        expect(instance.start(unitName("mask"), "/bin/sh",
                              {"-c", "grep SigBlk /proc/self/status > " + mask.string()}, failure),
               "the mask probe starts");
        pthread_sigmask(SIG_SETMASK, &previous, nullptr);
        expect(waitUntil(
                   [&instance] {
                       return !instance.running();
                   },
                   std::chrono::seconds{10}),
               "the mask probe ends");
        std::ifstream in{mask};
        std::string label;
        std::string bits;
        in >> label >> bits;
        expect(label == "SigBlk:", "the probe recorded its blocked signals");
        constexpr unsigned long termBit = 1UL << (SIGTERM - 1);
        expect((std::stoul(bits, nullptr, 16) & termBit) == 0,
               "the started program does not inherit a blocked SIGTERM");
    }

    {
        // The shell's own Gamescope WSI switch stays with the shell; a game gets the layer.
        setenv("DISABLE_GAMESCOPE_WSI", "1", 1);
        const fs::path seen = scratch / "wsi";
        ProcessLauncher instance{containment};
        std::string failure;
        expect(instance.start(unitName("wsi"), "/bin/sh", {"-c", "env > " + seen.string()}, failure,
                              {"FROM_CALLER=given"}),
               "the environment probe starts");
        unsetenv("DISABLE_GAMESCOPE_WSI");
        expect(waitUntil(
                   [&instance] {
                       return !instance.running();
                   },
                   std::chrono::seconds{10}),
               "the environment probe ends");
        std::ifstream in{seen};
        bool disabled = false;
        bool given = false;
        for (std::string line; std::getline(in, line);) {
            disabled = disabled || line.starts_with("DISABLE_GAMESCOPE_WSI=");
            given = given || line == "FROM_CALLER=given";
        }
        expect(!disabled, "a started program has no DISABLE_GAMESCOPE_WSI");
        expect(given, "a started program keeps what the caller adds");
    }

    {
        // "$" in the program's path and in its arguments reaches it as given.
        const fs::path folder = scratch / "dollar $HOME ${HOME}";
        fs::create_directories(folder);
        const fs::path seen = scratch / "argv";
        const fs::path program = folder / "probe ${X}.sh";
        {
            std::ofstream out{program};
            out << "#!/bin/sh\nfor word in \"$0\" \"$@\"; do printf '%s\\n' \"$word\"; done > '"
                << seen.string() << "'\n";
        }
        fs::permissions(program, fs::perms::owner_all);
        const std::vector<std::string> args{"$HOME", "${HOME}", "a$$b", "x${X}y", "$"};
        ProcessLauncher instance{containment};
        std::string failure;
        expect(instance.start(unitName("dollar"), program.string(), args, failure),
               "a program with $ in its path starts");
        expect(waitUntil(
                   [&instance] {
                       return !instance.running();
                   },
                   std::chrono::seconds{10}),
               "the argument probe ends");
        std::ifstream in{seen};
        std::vector<std::string> words;
        for (std::string line; std::getline(in, line);) {
            words.push_back(line);
        }
        std::vector<std::string> expected{program.string()};
        expected.insert(expected.end(), args.begin(), args.end());
        expect(words == expected, "the program's path and arguments arrive unchanged");
    }
}

} // namespace

int main(int argc, char** argv) try {
    if (argc == 3 && std::string_view{argv[1]} == "parent") {
        return standInParent(argv[2]);
    }
    const fs::path scratch =
        fs::path{OPENSU_TEST_SCRATCH} / ("opensu-instance-test-" + std::to_string(getpid()));
    fs::create_directories(scratch);
    checkLauncher(Containment::systemdScope, scratch / "scope");
    checkLauncher(Containment::keeper, scratch / "keeper");
    checkKeeper(scratch);
    fs::remove_all(scratch);
    std::printf("instance: all checks passed\n");
    return 0;
} catch (...) {
    return opensu::test::escaped();
}
