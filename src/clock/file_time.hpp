// file_time — a file's modification time on the system clock, the one conversion every standard
// library offers in its own way (`clock_cast`, or `file_clock::to_sys` where libc++ has no
// `clock_cast`).
#pragma once

#include <chrono>
#include <filesystem>

namespace opensu::clock {

/// `written` (from std::filesystem::last_write_time) as time since the Unix epoch.
[[nodiscard]] std::chrono::nanoseconds sinceEpoch(std::filesystem::file_time_type written);

} // namespace opensu::clock
