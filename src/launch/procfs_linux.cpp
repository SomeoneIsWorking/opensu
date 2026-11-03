#include "procfs.hpp"

#include <array>
#include <cerrno>

#include <fcntl.h>
#include <unistd.h>

namespace opensu::launch::procfs {

/// A gone process's entries fail read() with ESRCH, where libstdc++'s filebuf would throw.
std::string read(ProcessId pid, std::string_view entry) {
    const std::string path = "/proc/" + std::to_string(pid) + "/" + std::string{entry};
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

/// The state follows the command name's last ')', since the name can hold either parenthesis.
bool running(ProcessId pid) {
    const std::string stat = read(pid, "stat");
    const std::size_t close = stat.rfind(')');
    return close != std::string::npos && close + 2 < stat.size() && stat[close + 2] != 'Z';
}

std::string name(ProcessId pid) {
    std::string comm = read(pid, "comm");
    if (!comm.empty() && comm.back() == '\n') {
        comm.pop_back();
    }
    return comm;
}

bool inUnit(ProcessId pid, std::string_view unit) {
    return read(pid, "cgroup").find(unit) != std::string::npos;
}

} // namespace opensu::launch::procfs
