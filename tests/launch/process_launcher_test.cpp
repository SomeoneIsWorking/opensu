// The process launcher, against real systemd scopes and real processes.
#include "launch/process_launcher.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
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

/// Starts a command that leaves a grandchild in its own session, and returns the
/// grandchild's pid once recorded.
pid_t startEscaper(ProcessLauncher& instance, const char* stem, const fs::path& pidFile) {
    fs::remove(pidFile);
    std::string failure;
    expect(instance.start(unitName(stem), "/bin/sh",
                          {"-c", "setsid sleep 60 & echo $! > " + pidFile.string() + "; sleep 60"},
                          failure),
           "an escaping command starts");
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

} // namespace

int main() {
    const fs::path scratch =
        fs::path{OPENSU_TEST_SCRATCH} / ("opensu-instance-test-" + std::to_string(getpid()));
    fs::create_directories(scratch);

    {
        ProcessLauncher instance;
        std::string failure;
        expect(instance.start(unitName("plain"), "/bin/sleep", {"60"}, failure), "sleep starts");
        expect(instance.running(), "a started instance is running");
        const Clock::time_point began = Clock::now();
        instance.stop();
        expect(!instance.running(), "a stopped instance is not running");
        expect(Clock::now() - began < std::chrono::seconds{8}, "stop() is bounded");
    }

    {
        ProcessLauncher instance;
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
        ProcessLauncher instance;
        std::string failure;
        expect(!instance.start(unitName("missing"), "/no/such/program", {}, failure),
               "a missing program is refused");
        expect(failure.find("/no/such/program") != std::string::npos, "the failure names it");
    }

    {
        pid_t grandchild = 0;
        {
            ProcessLauncher instance;
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
        ProcessLauncher instance;
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
        ProcessLauncher instance;
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
        ProcessLauncher instance;
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

    fs::remove_all(scratch);
    std::printf("instance: all checks passed\n");
    return 0;
}
