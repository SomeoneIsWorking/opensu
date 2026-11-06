// hosted_runner_window — the runner's window when the shell owns it: a surface passed from the
// shell's Activity over Binder and held by `RunnerContext` in `:retro`, drawn into directly. It has
// no events of its own: the shell hears the input and posts it to the control channel. Picked in
// CMakeLists.txt: `hosted_runner_window_android.cpp`; `hosted_runner_window_none.cpp` elsewhere.
#pragma once

#include <memory>

#include "runner_window.hpp"

namespace opensu::retro {

/// The window the shell passed; throws std::runtime_error where there is no such thing.
[[nodiscard]] std::unique_ptr<RunnerWindow> openHostedWindow();

} // namespace opensu::retro
