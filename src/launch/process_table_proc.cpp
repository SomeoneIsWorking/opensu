#include "process_table.hpp"

#include <csignal>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

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
    std::ifstream in{"/proc/" + std::to_string(pid) + "/stat"};
    std::string text;
    std::getline(in, text);
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
    // The process may exit between the listing and the read, which reads empty.
    std::ifstream cmdline{"/proc/" + std::to_string(pid) + "/cmdline", std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{cmdline}, std::istreambuf_iterator<char>{}};
}

bool kill(ProcessId pid) {
    return ::kill(pid, SIGKILL) == 0;
}

} // namespace opensu::launch::table
