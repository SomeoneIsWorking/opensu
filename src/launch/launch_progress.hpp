// launch — how far a launch has got before its game shows a window.
#pragma once

#include <string>

namespace iideck::launch {

struct LaunchProgress {
    enum class Stage {
        /// The Steam client is still signing in.
        WaitingForSteam,
        /// Steam is downloading an update it must apply first.
        Updating,
        /// Steam is downloading a game that was not installed.
        Installing,
        /// Asked to start; nothing of the game runs yet.
        Starting,
        /// Steam is working through its launch, at `task`.
        Preparing,
        /// The game runs but has no window yet.
        Loading,
    };

    Stage stage{Stage::Starting};
    /// How far through the download, 0 to 1. Updating and Installing only.
    double fraction{0.0};
    /// Steam's own words for its current launch task. Preparing only.
    std::string task;

    bool operator==(const LaunchProgress&) const = default;
};

/// One line for the player: "Updating · 42%".
[[nodiscard]] std::string describe(const LaunchProgress& progress);

} // namespace iideck::launch
