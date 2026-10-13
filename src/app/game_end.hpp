// game_end — what the player is told when a launch is over: the built-in runner's load failure, a
// crash named by its signal, the launch's own failure, or that the game closed.
#pragma once

#include <string>

namespace opensu::app {

struct GameEnd {
    std::string title;
    /// Why the launch failed; empty when it ran.
    std::string failure;
    /// `Instance::exitStatus` from the handoff's `ended`; -1 when not reported.
    int exitStatus{-1};
    /// The load failure opensu-retro left in its control file.
    std::string runnerError;
};

struct EndNotice {
    std::string text;
    bool error{false};
};

/// "SIGSEGV" when `exitStatus` is a death by a signal the player did not send; empty for an exit,
/// SIGTERM, SIGKILL, SIGINT and SIGHUP.
[[nodiscard]] std::string crashSignal(int exitStatus);

[[nodiscard]] EndNotice noticeOf(const GameEnd& end);

} // namespace opensu::app
