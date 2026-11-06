// embedded — a game drawn inside the shell's window: the one word on the runner's command line
// that says so, which `Launches` reads on Android too.
#pragma once

#include <string_view>

namespace opensu::retro {

inline constexpr std::string_view embeddedArgument = "--embedded";

} // namespace opensu::retro
