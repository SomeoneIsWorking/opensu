// local_time — the wall clock in the player's time zone and locale: a moment as local calendar
// parts, and whether the locale's clock is 24-hour. POSIX (`local_time_posix.cpp`) or Windows
// (`local_time_win32.cpp`), picked in CMake.
#pragma once

#include <ctime>
#include <string_view>

namespace opensu::clock {

/// `moment` in the local time zone.
[[nodiscard]] std::tm localTime(std::time_t moment);

/// Whether the player's locale shows a 24-hour clock; true when it cannot be read.
[[nodiscard]] bool localeClock24Hour();

/// Whether a POSIX time format (nl_langinfo T_FMT) is 24-hour, i.e. has no 12-hour directive
/// (%I, %l or %r).
[[nodiscard]] bool timeFormatIs24Hour(std::string_view format) noexcept;

} // namespace opensu::clock
