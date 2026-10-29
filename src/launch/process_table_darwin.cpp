#include "process_table.hpp"

#include <cstring>

#include <libproc.h>
#include <signal.h>
#include <sys/proc_info.h>
#include <sys/sysctl.h>

namespace opensu::launch::table {

bool available() {
    return true;
}

std::vector<ProcessId> pids() {
    // The table can grow between the count and the listing, so room is left for more.
    const int counted = proc_listallpids(nullptr, 0);
    if (counted <= 0) {
        return {};
    }
    std::vector<pid_t> listed(static_cast<std::size_t>(counted) + 64);
    const int found =
        proc_listallpids(listed.data(), static_cast<int>(listed.size() * sizeof(pid_t)));
    if (found <= 0) {
        return {};
    }
    listed.resize(static_cast<std::size_t>(found));
    return {listed.begin(), listed.end()};
}

ProcessId parentOf(ProcessId pid) {
    proc_bsdinfo info{};
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) !=
        static_cast<int>(sizeof(info))) {
        return -1;
    }
    return static_cast<ProcessId>(info.pbi_ppid);
}

/// KERN_PROCARGS2 holds argc, the executable's path, padding NULs, then the arguments; only the
/// arguments are returned, each ended by a NUL as /proc's cmdline has them. Another user's
/// process reads empty.
std::string commandLine(ProcessId pid) {
    int maximum = 0;
    std::size_t size = sizeof(maximum);
    int argmax[] = {CTL_KERN, KERN_ARGMAX};
    if (sysctl(argmax, 2, &maximum, &size, nullptr, 0) != 0 || maximum <= 0) {
        return {};
    }
    std::string buffer(static_cast<std::size_t>(maximum), '\0');
    size = buffer.size();
    int args[] = {CTL_KERN, KERN_PROCARGS2, pid};
    if (sysctl(args, 3, buffer.data(), &size, nullptr, 0) != 0 || size < sizeof(int)) {
        return {};
    }
    buffer.resize(size);
    int argc = 0;
    std::memcpy(&argc, buffer.data(), sizeof(argc));
    std::size_t at = buffer.find('\0', sizeof(argc));
    at = buffer.find_first_not_of('\0', at);
    std::string line;
    for (int arg = 0; arg < argc && at != std::string::npos && at < buffer.size(); ++arg) {
        const std::size_t end = buffer.find('\0', at);
        const std::size_t stop = end == std::string::npos ? buffer.size() : end;
        line.append(buffer, at, stop - at);
        line.push_back('\0');
        at = stop + 1;
    }
    return line;
}

bool kill(ProcessId pid) {
    return ::kill(pid, SIGKILL) == 0;
}

} // namespace opensu::launch::table
