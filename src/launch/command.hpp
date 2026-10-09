// launch — short-lived plain children and executable lookup.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::launch {

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

/// Runs `program` with stdout and stderr merged and stdin closed, handing each complete line to
/// `onLine` as it arrives, until the child exits. When `stop` is requested the child and its
/// processes are ended. Returns the exit status (128 plus the signal when it was killed by one),
/// or nothing when it could not be run or was stopped.
[[nodiscard]] std::optional<int> runStreaming(const std::string& program,
                                              const std::vector<std::string>& args,
                                              const std::function<void(std::string_view)>& onLine,
                                              const std::stop_token& stop);

/// The status and stdout of a finished command.
struct Captured {
    int status{};
    std::string output;
};

/// Where a captured child's stderr goes.
enum class CaptureErrors : std::uint8_t { Discarded, Merged };

/// Runs `program` with stdin closed, collecting stdout until it exits; stderr is discarded, or
/// merged into the output with `Merged`. Returns nothing when it could not be run.
[[nodiscard]] std::optional<Captured>
runCaptured(const std::string& program, const std::vector<std::string>& args,
            CaptureErrors errors = CaptureErrors::Discarded);

} // namespace opensu::launch
