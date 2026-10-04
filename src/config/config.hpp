// config — the one owner of the process environment.
//
// Every other module takes what it needs from Config instead of calling
// getenv, so the environment is read once, at startup, and the result is an
// immutable value that cannot drift under a subsystem that reads it later.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace iideck::config {

/// Emulator commands per system, as "SYSTEM=program|arg|arg".
using EmulatorCommands = std::map<std::string, std::vector<std::string>, std::less<>>;

/// The whole environment, resolved once.
struct Config {
    /// The user's home directory. Every store keeps its data under it, so this
    /// is resolved rather than guessed by every library source.
    std::filesystem::path home;

    /// Explicit Steam install roots; discovered when empty.
    std::vector<std::filesystem::path> steamRoots;

    /// Directories scanned for emulator ROMs.
    std::vector<std::filesystem::path> romRoots;

    /// Per-system emulator commands.
    EmulatorCommands emulators;

    /// Where the shipped typeface lives.
    std::filesystem::path assetsDir{"assets"};

    /// Only controllers whose name contains this are accepted. Empty accepts
    /// every device SDL reports.
    std::string gamepadNameFilter;

    /// Window size.
    int width{1280};
    int height{800};

    /// The loopback control channel's port. Zero asks the system for a free one.
    std::uint16_t controlPort{7311};

    /// Whether the control channel runs at all. It is on by default because it
    /// is how an automated run drives the shell, and it listens on loopback
    /// only; the environment can move it, never close it.
    bool controlChannel{true};
};

/// Reads the environment once and returns the same value thereafter. Values that
/// cannot be parsed fall back to the default and are reported.
[[nodiscard]] const Config& read();

} // namespace iideck::config