// runner — how host services run the system's own tools (busctl, systemctl, bluetoothctl): one
// function type each to run or hold a program, so tests never touch the real machine.
#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "launch/command.hpp"

namespace opensu::host {

/// Runs a program with its arguments; stderr is merged into the output so a refusal carries its
/// reason. Nothing when the program could not be run.
using Runner = std::function<std::optional<launch::Captured>(const std::string& program,
                                                             const std::vector<std::string>& args)>;

/// The runner that starts real programs.
[[nodiscard]] Runner systemRunner();

/// Runs a program with one line on its stdin, such as a password for `sudo -S`; output merged as
/// for `Runner`. The line is never logged. Nothing when the program could not be run.
using LineRunner = std::function<std::optional<launch::Captured>(
    const std::string& program, const std::vector<std::string>& args, std::string_view line)>;

/// The line runner that starts real programs.
[[nodiscard]] LineRunner systemLineRunner();

/// Why `out`, the result of running `program`, is not a success: empty when it exited 0, else its
/// first output line, else "<program> refused to <what>", or "<program> could not be run".
[[nodiscard]] std::string refusalOf(const std::string& program,
                                    const std::optional<launch::Captured>& out,
                                    const std::string& what);

/// A program kept running for as long as its holder lives; destroying the holder ends it.
class Holder {
  public:
    virtual ~Holder() = default;
};

/// Starts a program to hold; null when it could not be started.
using Spawner = std::function<std::unique_ptr<Holder>(const std::string& program,
                                                      const std::vector<std::string>& args)>;

/// The spawner that starts real programs found in `executablePath`, their output discarded.
[[nodiscard]] Spawner systemSpawner(std::vector<std::filesystem::path> executablePath);

} // namespace opensu::host
