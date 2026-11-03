#include "orphaned_steam.hpp"

#include <cctype>
#include <limits>
#include <optional>
#include <sstream>
#include <thread>

#include "launch/command.hpp"
#include "launch/process_id.hpp"
#include "launch/procfs.hpp"
#include "lucent/log.h"

namespace opensu::steam {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

/// The program whose scopes these are, as `/proc/<pid>/comm` names it.
constexpr std::string_view program = "opensu";
constexpr std::string_view prefix = "opensu-";
/// The client's scope, then Isolated's Secret Service relay, in the order they are ended.
constexpr std::string_view suffixes[] = {"-steam.scope", "-steam-bus.scope"};
constexpr auto systemctlWait = std::chrono::seconds{30};
constexpr auto shutdownCommandWait = std::chrono::seconds{30};
constexpr auto scopePoll = std::chrono::milliseconds{200};

/// The pid in `opensu-<pid><suffix>`, or 0 when the name has another shape.
long ownerPid(const std::string& unit, std::string_view suffix) {
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

/// True when `pid` is a live (not exited) openSU.
bool liveOpenSu(launch::ProcessId pid) {
    return launch::procfs::running(pid) && launch::procfs::name(pid) == program;
}

bool scopeActive(const std::string& unit) {
    return launch::runCommand("systemctl", {"--user", "is-active", "--quiet", unit},
                              systemctlWait) == 0;
}

} // namespace

OrphanedSteam::OrphanedSteam(const fs::path& home) : desktop_{home} {
}

std::vector<std::string> OrphanedSteam::find() const {
    std::vector<std::string> orphans;
    for (const std::string_view suffix : suffixes) {
        const auto listed = launch::runCaptured(
            "systemctl", {"--user", "list-units", "--type=scope", "--state=active", "--plain",
                          "--no-legend", "--no-pager", "opensu-*" + std::string{suffix}});
        if (!listed) {
            continue;
        }
        std::istringstream lines{listed->output};
        std::string unit;
        while (lines >> unit) {
            const long pid = ownerPid(unit, suffix);
            if (pid > 0 && pid != launch::ownProcessId() &&
                !liveOpenSu(static_cast<launch::ProcessId>(pid))) {
                orphans.push_back(unit);
            }
            lines.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }
    return orphans;
}

int OrphanedSteam::stop(const fs::path& steam, std::chrono::milliseconds wait) const {
    const std::vector<std::string> orphans = find();
    for (const std::string& unit : orphans) {
        lucent::warn("steam", "ending {}, left behind by an openSU that died", unit);
        const std::optional<launch::ProcessId> steamPid = desktop_.running();
        if (steamPid && launch::procfs::inUnit(*steamPid, unit)) {
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
