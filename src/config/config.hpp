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
#include <string_view>
#include <vector>

namespace iideck::config {

/// iiSU's two single-screen dashboard modes (iiSU fs7).
enum class HomeMode {
    /// One horizontal grid that scrolls continuously (iiSU ap6 Flow); iiSU's default.
    Standard,
    /// Horizontal pages with neighbour peeks and page dots (iiSU ap6 Paged).
    WiiSu,
};

/// Emulator commands per system, as "SYSTEM=program|arg|arg".
using EmulatorCommands = std::map<std::string, std::vector<std::string>, std::less<>>;

/// The whole environment, resolved once.
struct Config {
    /// The user's home directory. Every store keeps its data under it, so this
    /// is resolved rather than guessed by every library source.
    std::filesystem::path home;

    /// Explicit Steam install roots; discovered when empty.
    std::vector<std::filesystem::path> steamRoots;

    /// ROM roots, each holding one folder per system; discovered when empty.
    std::vector<std::filesystem::path> romRoots;

    /// Per-system emulator commands, keyed by system ("ps2"), overriding the ones found.
    EmulatorCommands emulators;

    /// Where the shipped typeface lives: `IIDECK_ASSETS`, else `share/iideck` beside the
    /// executable's directory, as installed and as staged in the build tree.
    std::filesystem::path assetsDir;

    /// iideck's cache, downloaded artwork among it: `$XDG_CACHE_HOME/iideck`, else
    /// `~/.cache/iideck`.
    std::filesystem::path cacheDir;

    /// iideck's own data, the store sign-ins among it: `$XDG_DATA_HOME/iideck`, else
    /// `~/.local/share/iideck`.
    std::filesystem::path dataDir;

    /// The home grid's dashboard mode, from IIDECK_HOME_MODE (`standard` or `wiisu`).
    HomeMode homeMode{HomeMode::Standard};

    /// Whether the clock reads 24-hour time, from the LC_TIME locale's time format.
    bool clock24Hour{true};

    /// Window size.
    int width{1280};
    int height{800};

    /// The loopback control channel's port. Zero asks the system for a free one.
    std::uint16_t controlPort{7311};

    /// Whether the control channel runs at all. It is on by default because it
    /// is how an automated run drives the shell, and it listens on loopback
    /// only; the environment can move it, never close it.
    bool controlChannel{true};

    /// Whether iideck itself runs inside a Gamescope; when not, it starts one.
    bool insideGamescope{false};

    /// Names every scope iideck creates, `<session>-<role>[-N].scope`. From
    /// IIDECK_SESSION, which a nested session sets for the iideck inside it;
    /// otherwise `iideck-<pid>`.
    std::string session;

    /// Whether IIDECK_SESSION was set, which means a session already wraps this process.
    bool sessionInherited{false};

    /// The directories searched for a launch's program, from PATH.
    std::vector<std::filesystem::path> executablePath;
};

/// Whether a POSIX time format (nl_langinfo T_FMT) is 24-hour, i.e. has no 12-hour directive
/// (%I, %l or %r).
[[nodiscard]] bool timeFormatIs24Hour(std::string_view format) noexcept;

/// Reads the environment once and returns the same value thereafter. Values that
/// cannot be parsed fall back to the default and are reported.
[[nodiscard]] const Config& read();

} // namespace iideck::config