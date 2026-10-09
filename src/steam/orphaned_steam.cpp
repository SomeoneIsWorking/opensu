#include "orphaned_steam.hpp"

#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <thread>

#include <unistd.h>

#include "launch/command.hpp"
#include "lucent/log.h"

namespace opensu::steam {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

constexpr std::string_view prefix = "opensu-";
constexpr std::string_view suffix = "-steam.scope";
constexpr auto systemctlWait = std::chrono::seconds{30};
constexpr auto shutdownCommandWait = std::chrono::seconds{30};
constexpr auto scopePoll = std::chrono::milliseconds{200};

std::string firstLine(const fs::path& path) {
    std::ifstream in{path};
    std::string line;
    std::getline(in, line);
    return line;
}

/// The pid in `opensu-<pid>-steam.scope`, or 0 when the name has another shape.
long ownerPid(const std::string& unit) {
    if (unit.size() <= prefix.size() + suffix.size() ||
        unit.compare(0, prefix.size(), prefix) != 0 ||
        unit.compare(unit.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return 0;
    }
    const std::string digits =
        unit.substr(prefix.size(), unit.size() - prefix.size() - suffix.size());
    for (const char c : digits) {
        if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
            return 0;
        }
    }
    return digits.size() > 9 ? 0 : std::stol(digits);
}

/// True when `pid` is a live (not exited) process running the same program as this one.
bool liveSameProgram(long pid) {
    const fs::path proc = "/proc/" + std::to_string(pid);
    const std::string stat = firstLine(proc / "stat");
    const std::size_t close = stat.rfind(')');
    if (close == std::string::npos || close + 2 >= stat.size() || stat[close + 2] == 'Z') {
        return false;
    }
    return firstLine(proc / "comm") == firstLine("/proc/self/comm");
}

bool inScope(const fs::path& proc, const std::string& unit) {
    std::ifstream in{proc / "cgroup"};
    std::string line;
    while (std::getline(in, line)) {
        if (line.find(unit) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool scopeActive(const std::string& unit) {
    return launch::runCommand("systemctl", {"--user", "is-active", "--quiet", unit},
                              systemctlWait) == 0;
}

} // namespace

OrphanedSteam::OrphanedSteam(const fs::path& home) : pidFile_{home / ".steam" / "steam.pid"} {
}

std::vector<std::string> OrphanedSteam::find() const {
    const auto listed = launch::runCaptured(
        "systemctl", {"--user", "list-units", "--type=scope", "--state=active", "--plain",
                      "--no-legend", "--no-pager", "opensu-*-steam.scope"});
    std::vector<std::string> orphans;
    if (!listed) {
        return orphans;
    }
    std::istringstream lines{listed->output};
    std::string unit;
    while (lines >> unit) {
        const long pid = ownerPid(unit);
        if (pid > 0 && pid != getpid() && !liveSameProgram(pid)) {
            orphans.push_back(unit);
        }
        lines.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return orphans;
}

int OrphanedSteam::stop(const fs::path& steam, std::chrono::milliseconds wait) const {
    const std::vector<std::string> orphans = find();
    for (const std::string& unit : orphans) {
        lucent::warn("steam", "ending {}, left behind by an openSU that died", unit);
        long pid = 0;
        try {
            pid = std::stol(firstLine(pidFile_));
        } catch (const std::exception&) {
            pid = 0;
        }
        if (pid > 0 && inScope("/proc/" + std::to_string(pid), unit)) {
            const auto asked =
                launch::runCommand(steam.string(), {"-shutdown"}, shutdownCommandWait);
            const Clock::time_point until = Clock::now() + wait;
            while (asked && scopeActive(unit) && Clock::now() < until) {
                std::this_thread::sleep_for(scopePoll);
            }
        }
        if (launch::runCommand("systemctl", {"--user", "stop", unit}, systemctlWait) != 0) {
            lucent::warn("steam", "systemctl stop {} failed", unit);
        }
    }
    return static_cast<int>(orphans.size());
}

} // namespace opensu::steam
