// Desktop Steam detection against a fake home and real processes.
#include "steam/desktop_steam.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using iideck::steam::DesktopSteam;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

void writePid(const fs::path& home, const std::string& text) {
    fs::create_directories(home / ".steam");
    std::ofstream{home / ".steam" / "steam.pid"} << text;
}

} // namespace

int main() {
    const fs::path home =
        fs::path{IIDECK_TEST_SCRATCH} / ("iideck-steam-test-" + std::to_string(getpid()));
    fs::remove_all(home);
    fs::create_directories(home);
    const DesktopSteam steam{home};

    expect(!steam.runningOutside(""), "a missing pid file is not a running Steam");

    const pid_t child = fork();
    expect(child >= 0, "fork");
    if (child == 0) {
        execl("/bin/sleep", "sleep", "60", static_cast<char*>(nullptr));
        _exit(127);
    }
    writePid(home, std::to_string(child) + "\n");
    expect(steam.runningOutside(""), "a live pid with no instance is the desktop's");
    expect(steam.runningOutside("iideck-game-none.scope"),
           "a live pid outside the instance's cgroup is the desktop's");

    // The test process's own cgroup stands in for a Steam inside the instance.
    std::ifstream cgroup{"/proc/self/cgroup"};
    std::string line;
    std::getline(cgroup, line);
    const std::string own = line.substr(line.rfind('/') + 1);
    writePid(home, std::to_string(getpid()));
    expect(!steam.runningOutside(own), "a Steam inside the instance's scope is not the desktop's");

    writePid(home, std::to_string(child));
    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
    expect(!steam.runningOutside(""), "a dead pid is not a running Steam");

    writePid(home, "not a pid");
    expect(!steam.runningOutside(""), "a malformed pid file is not a running Steam");

    fs::remove_all(home);
    std::printf("desktop_steam: all checks passed\n");
    return 0;
}
