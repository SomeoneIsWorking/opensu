#include "desktop_steam.hpp"

#include <fstream>
#include <string>
#include <utility>

namespace opensu::steam {
namespace {

/// The first line of a procfs file, empty when it cannot be read.
std::string firstLine(const std::filesystem::path& path) {
    std::ifstream in{path};
    std::string line;
    std::getline(in, line);
    return line;
}

/// A zombie has exited and only waits to be reaped.
bool isZombie(const std::filesystem::path& proc) {
    const std::string stat = firstLine(proc / "stat");
    const std::size_t close = stat.rfind(')');
    return close != std::string::npos && close + 2 < stat.size() && stat[close + 2] == 'Z';
}

/// The live process the pid file names, or nothing.
std::optional<std::filesystem::path> liveProcess(const std::filesystem::path& pidFile) {
    long pid = 0;
    try {
        pid = std::stol(firstLine(pidFile));
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (pid <= 0) {
        return std::nullopt;
    }
    std::filesystem::path proc = "/proc/" + std::to_string(pid);
    std::error_code ec;
    if (!std::filesystem::is_directory(proc, ec) || isZombie(proc)) {
        return std::nullopt;
    }
    return proc;
}

} // namespace

DesktopSteam::DesktopSteam(const std::filesystem::path& home)
    : pidFile_{home / ".steam" / "steam.pid"} {
}

std::optional<pid_t> DesktopSteam::running() const {
    const std::optional<std::filesystem::path> proc = liveProcess(pidFile_);
    // A pid file Steam left behind may name a process that reused its number.
    if (!proc || firstLine(*proc / "comm") != "steam") {
        return std::nullopt;
    }
    return static_cast<pid_t>(std::stol(proc->filename().string()));
}

bool DesktopSteam::runningOutside(const std::string& instanceUnit) const {
    const std::optional<std::filesystem::path> proc = liveProcess(pidFile_);
    if (!proc) {
        return false;
    }
    if (instanceUnit.empty()) {
        return true;
    }
    std::ifstream cgroup{*proc / "cgroup"};
    std::string line;
    while (std::getline(cgroup, line)) {
        if (line.find(instanceUnit) != std::string::npos) {
            return false;
        }
    }
    return true;
}

} // namespace opensu::steam
