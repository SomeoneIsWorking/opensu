#include "desktop_steam.hpp"

#include <fstream>
#include <string>

#include "launch/procfs.hpp"

namespace opensu::steam {
namespace {

/// The live process the pid file names, or nothing.
std::optional<launch::ProcessId> liveProcess(const std::filesystem::path& pidFile) {
    std::ifstream in{pidFile};
    long pid = 0;
    if (!(in >> pid) || pid <= 0 || !launch::procfs::running(static_cast<launch::ProcessId>(pid))) {
        return std::nullopt;
    }
    return static_cast<launch::ProcessId>(pid);
}

} // namespace

DesktopSteam::DesktopSteam(const std::filesystem::path& home)
    : pidFile_{home / ".steam" / "steam.pid"} {
}

std::optional<launch::ProcessId> DesktopSteam::running() const {
    const std::optional<launch::ProcessId> pid = liveProcess(pidFile_);
    // A pid file Steam left behind may name a process that reused its number.
    if (!pid || launch::procfs::name(*pid) != "steam") {
        return std::nullopt;
    }
    return pid;
}

bool DesktopSteam::runningOutside(const std::string& instanceUnit) const {
    const std::optional<launch::ProcessId> pid = liveProcess(pidFile_);
    if (!pid) {
        return false;
    }
    return instanceUnit.empty() || !launch::procfs::inUnit(*pid, instanceUnit);
}

} // namespace opensu::steam
