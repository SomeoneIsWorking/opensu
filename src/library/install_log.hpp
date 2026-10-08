// install_log — what a store's downloader (legendary, gogdl) logs while it installs.
//
// Both are Python programs on the same logging format, `[<logger>] <LEVEL>: <message>`, and both
// log a `= Progress: <percent> ...` line; they differ in whether the percent carries a `%`.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace iideck::library::install_log {

/// How far an install has got, 0 to 1, from one `= Progress: 12.34% (505/4096), ...` or
/// `= Progress: 12.34 505/4096, ...` line; nothing for any other line.
[[nodiscard]] std::optional<double> progress(std::string_view line);

/// The message of an ERROR or CRITICAL log line; nothing for any other line.
[[nodiscard]] std::optional<std::string> loggedError(std::string_view line);

/// `line` without trailing spaces and carriage returns.
[[nodiscard]] std::string_view trimmed(std::string_view line);

} // namespace iideck::library::install_log
