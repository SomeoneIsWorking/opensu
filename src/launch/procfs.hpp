// procfs — one process's entries under /proc, the one reader of them: `procfs_linux.cpp` on
// Linux, `procfs_none.cpp` (no /proc) elsewhere.
#pragma once

#include <cstdint>
#include <optional>
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
/// When `pid` started, in clock ticks after boot (`stat`'s 22nd field), as polkit names a process.
[[nodiscard]] std::optional<std::uint64_t> startTime(ProcessId pid);
/// Whether `pid`'s cgroup path names systemd `unit`.
[[nodiscard]] bool inUnit(ProcessId pid, std::string_view unit);
/// The Flatpak app id `pid` runs in, the `name=` of its sandbox's `/.flatpak-info`; empty outside
/// one. A sandbox's command lines name only its command (`bwrap --args 75 -- eden`).
[[nodiscard]] std::string flatpakApp(ProcessId pid);

} // namespace opensu::launch::procfs
