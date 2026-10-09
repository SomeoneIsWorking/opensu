// The instance, against real systemd scopes and real processes.
#include "launch/instance.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using opensu::launch::Instance;

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
pid_t startEscaper(Instance& instance, const char* stem, const fs::path& pidFile) {
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
        Instance instance;
        std::string failure;
        expect(instance.start(unitName("plain"), "/bin/sleep", {"60"}, failure), "sleep starts");
        expect(instance.running(), "a started instance is running");
        const Clock::time_point began = Clock::now();
        instance.stop();
        expect(!instance.running(), "a stopped instance is not running");
        expect(Clock::now() - began < std::chrono::seconds{8}, "stop() is bounded");
    }

    {
        Instance instance;
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
        Instance instance;
        const pid_t grandchild = startEscaper(instance, "kill", scratch / "kill.pid");
        instance.kill();
        expect(waitUntil(
                   [grandchild] {
                       return !alive(grandchild);
                   },
                   std::chrono::seconds{8}),
               "kill() ends a process that escaped through setsid");
        expect(waitUntil(
                   [&instance] {
                       return !instance.running();
                   },
                   std::chrono::seconds{8}),
               "a killed instance stops running");
    }

    {
        Instance instance;
        std::string failure;
        expect(!instance.start(unitName("missing"), "/no/such/program", {}, failure),
               "a missing program is refused");
        expect(failure.find("/no/such/program") != std::string::npos, "the failure names it");
    }

    {
        pid_t grandchild = 0;
        {
            Instance instance;
            grandchild = startEscaper(instance, "dtor", scratch / "dtor.pid");
        }
        expect(waitUntil(
                   [grandchild] {
                       return !alive(grandchild);
                   },
                   std::chrono::seconds{8}),
               "destroying an instance stops it");
    }

    fs::remove_all(scratch);
    std::printf("instance: all checks passed\n");
    return 0;
}
