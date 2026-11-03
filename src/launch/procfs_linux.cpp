#include "procfs.hpp"

#include <array>
#include <cerrno>
#include <charconv>

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

std::optional<std::uint64_t> startTime(ProcessId pid) {
    const std::string stat = read(pid, "stat");
    std::size_t at = stat.rfind(')');
    if (at == std::string::npos) {
        return std::nullopt;
    }
    // The state is field 3, right after the name; the start time is field 22.
    constexpr int fieldsAfterName = 20;
    for (int field = 0; field < fieldsAfterName; ++field) {
        at = stat.find(' ', at + 1);
        if (at == std::string::npos) {
            return std::nullopt;
        }
    }
    std::uint64_t ticks = 0;
    const char* first = stat.data() + at + 1;
    const auto [end, error] = std::from_chars(first, stat.data() + stat.size(), ticks);
    if (error != std::errc{} || end == first) {
        return std::nullopt;
    }
    return ticks;
}

bool inUnit(ProcessId pid, std::string_view unit) {
    return read(pid, "cgroup").find(unit) != std::string::npos;
}

} // namespace opensu::launch::procfs
