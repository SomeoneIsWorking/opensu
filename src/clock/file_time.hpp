// file_time — a file's modification time on the system clock and back, the one conversion every
// standard library offers in its own way (`clock_cast`, or `file_clock::to_sys` and `from_sys`
// where libc++ has no `clock_cast`).
#pragma once

#include <chrono>
#include <filesystem>

namespace opensu::clock {

/// `written` (from std::filesystem::last_write_time) as time since the Unix epoch.
[[nodiscard]] std::chrono::nanoseconds sinceEpoch(std::filesystem::file_time_type written);

/// The modification time to give a file for `at`.
[[nodiscard]] std::filesystem::file_time_type fileTimeOf(std::chrono::system_clock::time_point at);

} // namespace opensu::clock
