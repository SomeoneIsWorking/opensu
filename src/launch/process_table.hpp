// process_table — the OS's table of running processes, under ProcessTree: /proc on Linux
// (`process_table_proc.cpp`), none elsewhere yet (`process_table_none.cpp`).
#pragma once

#include <string>
#include <vector>

#include "process_id.hpp"

namespace opensu::launch::table {

/// Whether this OS's table lists processes at all.
[[nodiscard]] bool available();
/// Every running process.
[[nodiscard]] std::vector<ProcessId> pids();
/// The parent of `pid`, or -1 when it exited.
[[nodiscard]] ProcessId parentOf(ProcessId pid);
/// `pid`'s command line, its arguments separated by NULs; empty when it exited.
[[nodiscard]] std::string commandLine(ProcessId pid);
/// Ends `pid` at once (SIGKILL). False when it could not be signalled.
bool kill(ProcessId pid);

} // namespace opensu::launch::table
