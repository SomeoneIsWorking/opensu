// launch_request — what a launch is asked with beside its game: the environment added to the
// game's own, the profile it plays as and who it is for. A launch held for saves or a download
// keeps it whole.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace opensu::app {

/// Who a launch is for.
enum class LaunchAudience : std::uint8_t {
    /// The player here, who sees the game on this screen.
    Player,
    /// A remote play guest playing it from here, which generates its own frames from the stream.
    RemoteGuest,
};

struct LaunchRequest {
    /// Added to the game's own environment.
    std::vector<std::string> environment{};
    /// The profile it plays as.
    std::string profile{};
    LaunchAudience audience{LaunchAudience::Player};
};

} // namespace opensu::app
