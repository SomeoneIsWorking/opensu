// remote_reach — what this build can do of remote play, for the owners that offer it: hosting
// needs a screen to capture, playing a host's game needs its UDP socket, Opus and a video decoder.
// Each answer is the reason it cannot, empty when it can; a Linux host outside Gamescope still
// refuses at Play, since that depends on how this run was started.
#pragma once

#include <string>

namespace opensu::stream {

[[nodiscard]] std::string hostingMissing();
[[nodiscard]] std::string playingMissing();

} // namespace opensu::stream
