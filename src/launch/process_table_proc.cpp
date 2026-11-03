#include "process_table.hpp"

#include <array>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <filesystem>
#include <sstream>
#include <unistd.h>

namespace opensu::launch::table {

namespace fs = std::filesystem;

namespace {

/// A /proc file of a process; empty once the process is gone, where read() fails with ESRCH and
/// libstdc++'s filebuf would throw.
std::string readProcFile(ProcessId pid, const char* name) {
    const std::string path = "/proc/" + std::to_string(pid) + "/" + name;
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return {};
    }
    std::string text;
    std::array<char, 4096> chunk{};
    ssize_t got = 0;
    while ((got = ::read(fd, chunk.data(), chunk.size())) != 0) {
        if (got < 0 && errno != EINTR) {
            break;
        }
        if (got > 0) {
            text.append(chunk.data(), static_cast<std::size_t>(got));
        }
    }
    ::close(fd);
    return got < 0 ? std::string{} : text;
}

} // namespace

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
    const std::string text = readProcFile(pid, "stat");
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
    return readProcFile(pid, "cmdline");
}

bool kill(ProcessId pid) {
    return ::kill(pid, SIGKILL) == 0;
}

} // namespace opensu::launch::table
