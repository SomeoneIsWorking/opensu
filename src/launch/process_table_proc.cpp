#include "process_table.hpp"

#include <csignal>
#include <filesystem>
#include <sstream>

#include "procfs.hpp"

namespace opensu::launch::table {

namespace fs = std::filesystem;

bool available() {
    return true;
}

/// Every pid in /proc.
std::vector<ProcessId> pids() {
    std::vector<ProcessId> found;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator{"/proc", ec}) {
        if (ec) {
            break;
        }
        const std::string name = entry.path().filename().string();
        if (name.empty() || name.front() < '0' || name.front() > '9') {
            continue;
        }
        found.push_back(static_cast<ProcessId>(std::stol(name)));
    }
    return found;
}

/// The command name in /proc/<pid>/stat can hold spaces and parentheses, so the fields are read
/// after its last ')'.
ProcessId parentOf(ProcessId pid) {
    const std::string text = procfs::read(pid, "stat");
    const std::size_t close = text.rfind(')');
    if (close == std::string::npos) {
        return -1;
    }
    std::string state;
    long ppid = -1;
    std::istringstream fields{text.substr(close + 1)};
    if (!(fields >> state >> ppid)) {
        return -1;
    }
    return static_cast<ProcessId>(ppid);
}

std::string commandLine(ProcessId pid) {
    return procfs::read(pid, "cmdline");
}

bool kill(ProcessId pid) {
    return ::kill(pid, SIGKILL) == 0;
}

} // namespace opensu::launch::table
