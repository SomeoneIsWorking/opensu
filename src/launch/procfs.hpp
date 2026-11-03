// procfs — one process's entries under /proc, the one reader of them: `procfs_linux.cpp` on
// Linux, `procfs_none.cpp` (no /proc) elsewhere.
#pragma once

#include <string>
#include <string_view>

#include "process_id.hpp"

namespace opensu::launch::procfs {

/// `/proc/<pid>/<entry>` whole; empty when the process is gone, even mid-read.
[[nodiscard]] std::string read(ProcessId pid, std::string_view entry);
/// Whether `pid` exists and has not exited (a zombie has).
[[nodiscard]] bool running(ProcessId pid);
/// `pid`'s command name (`comm`); empty when it is gone.
[[nodiscard]] std::string name(ProcessId pid);
/// Whether `pid`'s cgroup path names systemd `unit`.
[[nodiscard]] bool inUnit(ProcessId pid, std::string_view unit);

} // namespace opensu::launch::procfs
