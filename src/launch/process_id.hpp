// process_id — a process's id on every OS, and openSU's own, from the child backend
// (`posix_child.cpp`, `win32_child.cpp`).
#pragma once

#include <cstdint>
#include <string_view>

namespace opensu::launch {

/// A process id: `pid_t` on POSIX, the process id `GetCurrentProcessId` names on Windows.
using ProcessId = std::int32_t;

/// An openSU names its session `<sessionFamily><its pid>`, and its scopes after the session.
inline constexpr std::string_view sessionFamily = "opensu-";

/// This process's id.
[[nodiscard]] ProcessId ownProcessId() noexcept;

} // namespace opensu::launch
