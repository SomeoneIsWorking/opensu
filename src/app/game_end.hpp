// game_end — what the player is told when a launch is over: the built-in runner's load failure, a
// crash named by its signal, that the player cancelled it, the launch's own failure, or that the
// game closed.
#pragma once

#include <string>

namespace opensu::app {

struct GameEnd {
    std::string title;
    /// Why the launch failed; empty when it ran.
    std::string failure;
    /// `ProcessLauncher::exitStatus` from the handoff's `ended`; -1 when not reported.
    int exitStatus{-1};
    /// The load failure opensu-retro left in its control file.
    std::string runnerError;
    /// The player cancelled the launch from its panel.
    bool cancelled{false};
};

struct EndNotice {
    std::string text;
    bool error{false};
};

/// What crashed the game when `exitStatus` says it crashed, else empty. POSIX
/// (`game_end_posix.cpp`): "SIGSEGV" for a death by a signal the player did not send, empty for an
/// exit, SIGTERM, SIGKILL, SIGINT and SIGHUP. Windows (`game_end_win32.cpp`): "access violation"
/// for an unhandled exception, empty for an exit code and Ctrl+C.
[[nodiscard]] std::string crashName(int exitStatus);

[[nodiscard]] EndNotice noticeOf(const GameEnd& end);

} // namespace opensu::app
