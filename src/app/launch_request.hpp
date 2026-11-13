// launch_request — what a launch is asked with beside its game: the environment added to the
// game's own and the profile it plays as. A launch held for saves or a download keeps it whole.
#pragma once

#include <string>
#include <vector>

namespace opensu::app {

struct LaunchRequest {
    /// Added to the game's own environment.
    std::vector<std::string> environment;
    /// The profile it plays as.
    std::string profile;
};

} // namespace opensu::app
