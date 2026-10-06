// launch — short-lived plain children and executable lookup.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace iideck::launch {

/// The executable `program` names, directly or in one of `path`; empty when there is none.
[[nodiscard]] std::filesystem::path
resolveExecutable(const std::string& program, const std::vector<std::filesystem::path>& path);

/// Runs `program` with its output discarded and waits for it, at most `timeout`; a
/// child still running then is killed and reaped. Returns the exit status (128 plus
/// the signal when it was killed by one), or nothing when it could not be run or
/// timed out.
[[nodiscard]] std::optional<int> runCommand(const std::string& program,
                                            const std::vector<std::string>& args,
                                            std::chrono::milliseconds timeout);

} // namespace iideck::launch
