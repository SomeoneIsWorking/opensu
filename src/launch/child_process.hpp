// child_process — the OS's part of a short-lived child (`command.hpp`): `posix_child.cpp`
// (`posix_spawn`, a process group, `poll` on pipes) or `win32_child.cpp` (`CreateProcessW` in a Job
// Object, pipes), picked in CMakeLists.txt.
#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "command.hpp"

namespace opensu::launch::child {

/// Whether `program` names a file by its path rather than a program to look up.
[[nodiscard]] bool namesFile(const std::string& program);

/// The file that runs when `candidate` is started, or nothing when none does.
[[nodiscard]] std::optional<std::filesystem::path>
executableAt(const std::filesystem::path& candidate);

/// `runCommand`'s child: output discarded, killed after `timeout`.
[[nodiscard]] std::optional<int> run(const std::string& program,
                                     const std::vector<std::string>& args,
                                     std::chrono::milliseconds timeout);

/// A child whose stdout (and stderr when `errors` is Merged) is handed to `onLine` line by line,
/// with `stdinLine` and a newline on its stdin when given, else none; ended with everything it
/// started when `stop` is requested.
[[nodiscard]] std::optional<int> stream(const std::string& program,
                                        const std::vector<std::string>& args,
                                        const std::function<void(std::string_view)>& onLine,
                                        const std::stop_token& stop, CaptureErrors errors,
                                        const std::optional<std::string_view>& stdinLine);

} // namespace opensu::launch::child
