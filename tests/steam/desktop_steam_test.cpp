// Desktop Steam detection against a fake home and real processes.
#include "steam/desktop_steam.hpp"

#include "launch/instance.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;
using opensu::steam::DesktopSteam;

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
        fs::path{OPENSU_TEST_SCRATCH} / ("opensu-steam-test-" + std::to_string(getpid()));
    fs::remove_all(home);
    fs::create_directories(home);
    const DesktopSteam steam{home};

    expect(!steam.runningOutside(""), "a missing pid file is not a running Steam");

    const pid_t child = fork();
    expect(child >= 0, "fork");
    if (child == 0) {
        // Off the test's output, so a failed check does not leave ctest waiting on the sleep.
        const int null = open("/dev/null", O_RDWR);
        if (null < 0 || dup2(null, STDOUT_FILENO) < 0 || dup2(null, STDERR_FILENO) < 0) {
            _exit(127);
        }
        execl("/bin/sleep", "sleep", "60", static_cast<char*>(nullptr));
        _exit(127);
    }
    writePid(home, std::to_string(child) + "\n");
    expect(steam.runningOutside(""), "a live pid with no instance is the desktop's");
    expect(steam.runningOutside("opensu-game-none.scope"),
           "a live pid outside the instance's cgroup is the desktop's");

    // A process in a real instance scope stands in for a Steam the instance started.
    const std::string unit = "opensu-steam-test-" + std::to_string(getpid()) + ".scope";
    const fs::path inside = home / "inside.pid";
    opensu::launch::Instance instance;
    std::string failure;
    expect(instance.start(unit, "/bin/sh",
                          {"-c", "echo $$ > \"$0\"; exec sleep 60", inside.string()}, failure),
           "the instance starts");
    std::string insidePid;
    for (int tries = 0; tries < 100 && insidePid.empty(); ++tries) {
        std::ifstream{inside} >> insidePid;
        usleep(20000);
    }
    writePid(home, insidePid);
    const bool outside = steam.runningOutside(unit);
    instance.stop();
    expect(!insidePid.empty() && !outside,
           "a Steam inside the instance's scope is not the desktop's");

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
