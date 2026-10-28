// process_id — a process's id on every OS, and openSU's own, from the child backend
// (`posix_child.cpp`, `win32_child.cpp`).
#pragma once

#include <cstdint>

namespace opensu::launch {

/// A process id: `pid_t` on POSIX, the process id `GetCurrentProcessId` names on Windows.
using ProcessId = std::int32_t;

/// This process's id.
[[nodiscard]] ProcessId ownProcessId() noexcept;

} // namespace opensu::launch
