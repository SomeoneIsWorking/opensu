#include "desktop_steam.hpp"

#include <fstream>
#include <string>
#include <utility>

namespace iideck::launch {
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

} // namespace

DesktopSteam::DesktopSteam(std::filesystem::path home)
    : pidFile_{std::move(home) / ".steam" / "steam.pid"} {
}

bool DesktopSteam::runningOutside(const std::string& instanceUnit) const {
    long pid = 0;
    try {
        pid = std::stol(firstLine(pidFile_));
    } catch (const std::exception&) {
        return false;
    }
    if (pid <= 0) {
        return false;
    }
    const std::filesystem::path proc = "/proc/" + std::to_string(pid);
    std::error_code ec;
    if (!std::filesystem::is_directory(proc, ec) || isZombie(proc)) {
        return false;
    }
    if (instanceUnit.empty()) {
        return true;
    }
    std::ifstream cgroup{proc / "cgroup"};
    std::string line;
    while (std::getline(cgroup, line)) {
        if (line.find(instanceUnit) != std::string::npos) {
            return false;
        }
    }
    return true;
}

} // namespace iideck::launch
